// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The register layer, the window loaders and the initialisation.
#include "effects/host/effect_host.h"
#include <algorithm>

namespace fsvr::vop3host {

// the constants of n listed steps, unless the hold flag is set
void EffectHost::writeCoefs(uint32_t n, const uint16_t* steps)
{
    if (hold)
        return;
    for (uint32_t i = 0; i < n; i++) {
        writeReg(SELECT, steps[i]);
        writeReg(COEF, coef[steps[i]]);
    }
}

// the same through the auto-increment latch (no hold check)
void EffectHost::writeCoefsBurst(uint32_t n, const uint16_t* steps)
{
    writeReg(SELECT, steps[0] | AUTO_INCREMENT);
    for (uint32_t i = 0; i < n; i++)
        writeReg(COEF, coef[steps[i]]);
}

// the offsets of n listed delay slots, unless held. The
// first slot's area is where every slot of the list is taken to start.
void EffectHost::writeDelays(uint32_t n, const uint8_t* slots)
{
    if (hold)
        return;
    const int area = rom::SLOT_AREA[slots[0]];
    const int32_t base = area >= 0 && area <= 4 ? int32_t(AREA[area]) : 0;
    for (uint32_t i = 0; i < n; i++) {
        writeReg(SELECT, slots[i]);
        const uint32_t v = delay[slots[i]] + base;
        writeReg(SLOT_HIGH, (v >> 9) & 0x1FF);
        writeReg(SLOT_LOW,  v & 0x1FF);
    }
}

// one of an LFO's four registers, LFO_SPEED..LFO_DIRECTION, from its shadow, unless held
void EffectHost::writeLfo(Reg reg, uint32_t lfo)
{
    if (!hold)
        ChipHost::writeLfo(reg, lfo);
}

const Window WINDOWS[5] = {
    {rom::STEPS_REVERB, int(std::size(rom::STEPS_REVERB)), rom::SLOTS_REVERB, int(std::size(rom::SLOTS_REVERB))},
    {rom::STEPS_AUX, 40, rom::SLOTS_AUX, 10, rom::LFOS_AUX, 0x000F},
    {rom::STEPS_INSERTION, 96, rom::SLOTS_INSERTION, 40, rom::LFOS_INSERTION, 0xF000, 14},
    {rom::STEPS_REVERB_DELAY, 72, rom::SLOTS_REVERB_DELAY, 12, rom::LFOS_REVERB_DELAY, 0x00F0, 11},
    {rom::STEPS_VARIATION, 72, rom::SLOTS_VARIATION, 12, rom::LFOS_VARIATION, 0x0F00, 11},
};

// a window's program for its type msb or, with `clear`, the program that silences it. Kind 0 is the base program,
// which every performance runs.
void EffectHost::uploadProgram(int kind, int clear)
{
    if (kind >= 1 && kind <= 4)
        sink->load(kind, clear ? WINDOWS[kind].clearEntry : code[kind].msb);
}

// bracket the chip, upload a window's program, constants, delay
// offsets and LFO registers, reset its LFO phases, unbracket.
void EffectHost::uploadWindow(int kind)
{
    writeConfig(0, 0x40);
    writeReg(UPLOADING, 0x23);
    switch (kind) {
        case REVERB: {
            const Window& w = WINDOWS[REVERB];
            writeDelays(w.nslots, w.slots);
            writeCoefs(w.nsteps, w.steps);
            break;
        }
        case AUX: case INSERTION: case REVERB_DELAY: case VARIATION: {
            const Window& w = WINDOWS[kind];
            uploadProgram(kind, 0);
            writeDelays(w.nslots, w.slots);
            writeCoefsBurst(w.nsteps, w.steps);
            for (int i = 0; i < 4; i++)
                for (Reg reg : {LFO_SPEED, LFO_WAVE, LFO_DEPTH, LFO_DIRECTION})
                    writeLfo(reg, w.lfos[i]);
            resetLfoPhase(w.lfoMask);
            break;
        }
        case MASTER_EQ:
            writeCoefs(int(std::size(rom::STEPS_MASTER_EQ)), rom::STEPS_MASTER_EQ);
            break;
        default:
            writeCoefs(int(std::size(rom::STEPS_SYSTEM)), rom::STEPS_SYSTEM);
            break;
    }
    writeConfig(0, rom::CFG16[0]);
    writeReg(UPLOADING, 0);
}

// register 1 brackets the boot writes: register 5, the CONFIG table, and three tables of
// fifteen nobody names
void EffectHost::initChip()
{
    writeReg(1, 1);
    writeReg(5, 8180);
    for (uint32_t i = 0; i < 15; i++)
        writeConfig(i, rom::CFG16[i]);
    for (uint32_t reg : {0x28u, 0x29u, 0x2Au})
        for (uint32_t i = 0; i < 15; i++) {
            writeReg(SELECT, i);
            writeReg(reg, reg == 0x2A ? 0 : 0x15);
        }
    writeReg(1, 0);
    setMute(OUTPUT, 1);
}

void EffectHost::uploadAll()
{
    uploadProgram(REVERB, 0);
    for (int kind = REVERB; kind <= MIXER; kind++)
        uploadWindow(kind);
}

// a window's or the output's mute flag, then the mixer parameter that applies it
void EffectHost::setMute(int window, uint32_t on)
{
    static const int APPLIED_BY[6] = {MIX_REVERB_PAN, MIX_AUX_PAN, MIX_INSERTION_PAN, MIX_DELAY_PAN, MIX_VARIATION_PAN, MIX_VOLUME};
    if (window >= REVERB && window <= OUTPUT) {
        mute[window] = (on & 0xFF) == 1;
        mixerParam(APPLIED_BY[window]);
        return;
    }
    for (int w = REVERB; w <= OUTPUT; w++)
        setMute(w, on);
}

// the effect host's reset. Mutes, defaults, every kind's
// preset load, all windows, the performance's effect bytes.
void EffectHost::init()
{
    masterLevel = 8192;
    hold = false;
    std::fill_n(mute, 6, 1);
    initChip();
    resetWindow(MIXER, true);
    for (int w : {REVERB, AUX, INSERTION, REVERB_DELAY, VARIATION}) {
        code[w] = {};
        resetWindow(w, true);
    }
    resetWindow(MASTER_EQ, true);
    uploadAll();
    perf[REVERB_PAN]             = 64;
    perf[REVERB_RETURN]          = 0;
    perf[VARIATION_PAN]          = 64;
    perf[VARIATION_RETURN]       = 0;
    perf[VARIATION_TO_REVERB]    = 0;
    perf[INSERTION_PAN]          = 64;
    perf[INSERTION_TO_REVERB]    = 0;
    perf[INSERTION_TO_VARIATION] = 0;
    setMixer(MIX_REVERB_RETURN, 0);
    setMixer(MIX_VARIATION_RETURN, 0);
    setMixer(9, 0);
    setOutputGain();
    setMute(ALL, 0);
}

}  // namespace fsvr::vop3host
