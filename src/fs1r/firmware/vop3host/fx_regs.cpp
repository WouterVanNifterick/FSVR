// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The chip's register layer, the window loaders and the engine initialisation.
#include "effect_host.h"
#include <algorithm>

namespace fs1r::vop3host {

// one 16-bit write to the effect chip's register window
void EffectHost::writeReg(uint32_t reg, uint32_t value)
{
    sink->write(reg, value & 0xFFFF);
}

// register 0xB of n listed steps, unless the hold flag is set
void EffectHost::writeCoefs(uint32_t n, const uint16_t* steps)
{
    if (hold)
        return;
    for (uint32_t i = 0; i < n; i++) {
        writeReg(0, steps[i]);
        writeReg(0x0B, coef[steps[i]]);
    }
}

// the same through the auto-increment latch (no hold check)
void EffectHost::writeCoefsBurst(uint32_t n, const uint16_t* steps)
{
    writeReg(0, steps[0] | 0x8000);
    for (uint32_t i = 0; i < n; i++)
        writeReg(0x0B, coef[steps[i]]);
}

// register 0xC (tag) of n listed steps, unless held
void EffectHost::writeTags(uint32_t n, const uint16_t* steps)
{
    if (hold)
        return;
    for (uint32_t i = 0; i < n; i++) {
        writeReg(0, steps[i]);
        writeReg(0x0C, tag[steps[i]]);
    }
}

// tags through the latch
void EffectHost::writeTagsBurst(uint32_t n, const uint16_t* steps)
{
    writeReg(0, steps[0] | 0x8000);
    for (uint32_t i = 0; i < n; i++)
        writeReg(0x0C, tag[steps[i]]);
}

// register 0x16 entry `index`
void EffectHost::writeConfig(uint32_t index, uint32_t value)
{
    writeReg(0, index + 1);
    writeReg(0x16, value);
}

// delay offsets of n listed slots as registers 0xD/0xE, unless held. The
// first slot's area is where every slot of the list is taken to start.
void EffectHost::writeDelays(uint32_t n, const uint8_t* slots)
{
    if (hold)
        return;
    const int area = rom::SLOT_AREA[slots[0]];
    const int32_t base = area >= 0 && area <= 4 ? int32_t(AREA[area]) : 0;
    for (uint32_t i = 0; i < n; i++) {
        writeReg(0, slots[i]);
        const uint32_t v = delay[slots[i]] + base;
        writeReg(0x0D, (v >> 9) & 0x1FF);
        writeReg(0x0E, v & 0x1FF);
    }
}

// one of an LFO's four registers, 0x24..0x27, from its shadow, unless held
void EffectHost::writeLfo(uint32_t reg, uint32_t lfo)
{
    if (hold)
        return;
    writeReg(0, lfo);
    writeReg(reg, lfoReg[reg - 0x24][lfo]);
}

// LFO phase reset mask, then clear register 0x1C
void EffectHost::resetLfoPhase(uint32_t mask)
{
    writeReg(0x20, mask);
    writeReg(0x1C, 0);
}

const Window WINDOWS[5] = {
    {rom::STEPS_REVERB, 94, rom::SLOTS_REVERB, 54},
    {rom::STEPS_AUX, 40, rom::SLOTS_AUX, 10, rom::LFOS_AUX, 0x000F, rom::AUX_PROGRAM, rom::AUX_TAGS},
    {rom::STEPS_INSERTION, 96, rom::SLOTS_INSERTION, 40, rom::LFOS_INSERTION, 0xF000, rom::INSERTION_PROGRAMS, rom::INSERTION_TAGS, 14},
    {rom::STEPS_REVERB_DELAY, 72, rom::SLOTS_REVERB_DELAY, 12, rom::LFOS_REVERB_DELAY, 0x00F0, rom::REVERB_DELAY_PROGRAMS, rom::DELAY_TAGS, 11},
    {rom::STEPS_VARIATION, 72, rom::SLOTS_VARIATION, 12, rom::LFOS_VARIATION, 0x0F00, rom::VARIATION_PROGRAMS, rom::DELAY_TAGS, 11},
};

// the tag and the five program words of every step of a window, for
// its type msb or, with `clear`, the program that silences it. Kind 0 is the base program.
void EffectHost::uploadProgram(int kind, int clear)
{
    if (kind >= 1 && kind <= 4) {
        const Window& w = WINDOWS[kind];
        const int entry = clear ? w.clearEntry : code[kind].msb;
        // the tag shadow is filled from the window's first step on, not step by step
        std::copy_n(w.tags + entry * w.nsteps, w.nsteps, tag + w.steps[0]);
        writeTagsBurst(w.nsteps, w.steps);
        const uint16_t (*program)[5] = w.programs + entry * w.nsteps;
        for (int word = 0; word < 5; word++) {
            writeReg(0, w.steps[0] | 0x8000);
            for (int s = 0; s < w.nsteps; s++)
                writeReg(10 - word, program[s][word]);
        }
        return;
    }
    std::copy_n(rom::BASE_TAGS[0], 232, tag);
    writeTags(50, rom::STEPS_MASTER_EQ);
    writeTags(88, rom::STEPS_SYSTEM);
    for (int word = 0; word < 5; word++) {
        writeReg(0, 0x8000);
        for (int s = 0; s < 232; s++)
            writeReg(10 - word, rom::BASE_PROGRAM[s][word]);
    }
}

// bracket the chip, upload a window's program, constants, delay
// offsets and LFO registers, reset its LFO phases, unbracket.
void EffectHost::uploadWindow(int kind)
{
    writeConfig(0, 0x40);
    writeReg(0x17, 0x23);
    switch (kind) {
    case 0:
        writeDelays(54, rom::SLOTS_REVERB);
        writeCoefs(94, rom::STEPS_REVERB);
        std::copy_n(rom::BASE_TAGS[0], 232, tag);
        writeTags(94, rom::STEPS_REVERB);
        break;
    case 1: case 2: case 3: case 4: {
        const Window& w = WINDOWS[kind];
        uploadProgram(kind, 0);
        writeDelays(w.nslots, w.slots);
        writeCoefsBurst(w.nsteps, w.steps);
        for (int i = 0; i < 4; i++)
            for (uint32_t reg = 0x24; reg <= 0x27; reg++)
                writeLfo(reg, w.lfos[i]);
        resetLfoPhase(w.lfoMask);
        break;
    }
    case 5:
        writeCoefs(50, rom::STEPS_MASTER_EQ);
        break;
    default:
        writeTags(88, rom::STEPS_SYSTEM);
        writeCoefs(88, rom::STEPS_SYSTEM);
        break;
    }
    writeConfig(0, rom::CFG16[0]);
    writeReg(0x17, 0);
}

// reset, mode, the three config tables, register 0x16 config
void EffectHost::initChip()
{
    writeReg(1, 1);
    writeReg(5, 8180);
    for (uint32_t i = 0; i < 15; i++)
        writeConfig(i, rom::CFG16[i]);
    for (uint32_t reg : {0x28u, 0x29u, 0x2Au})
        for (uint32_t i = 0; i < 15; i++) {
            writeReg(0, i);
            writeReg(reg, reg == 0x2A ? 0 : 0x15);
        }
    writeReg(1, 0);
    setMute(5, 1);
}

void EffectHost::uploadAll()
{
    uploadProgram(REVERB, 0);
    for (int kind = 0; kind <= 6; kind++)
        uploadWindow(kind);
}

// per-effect enable byte, then the system parameter that applies it
void EffectHost::setMute(int effect, uint32_t enabled)
{
    enabled &= 0xFF;
    static const int SYS_PARAM[6] = {0, 2, 5, 10, 11, 14};
    if (effect >= 0 && effect <= 5) {
        mute[effect] = enabled == 1 ? 1 : 0;
        mixerParam(SYS_PARAM[effect]);
        return;
    }
    for (int e = 0; e <= 4; e++)
        setMute(e, enabled);
    setMute(5, enabled);
}

// the effect host's reset. Mutes, defaults, every kind's
// preset load, the master EQ mid bands, all windows, the performance's effect bytes.
void EffectHost::init()
{
    masterLevel = 8192;
    hold = false;
    for (int e = 0; e < 6; e++)
        mute[e] = 1;
    initChip();
    applyParam(MIXER, 255);
    code[REVERB].lsb = 0;
    applyParam(REVERB, 255);
    code[AUX].msb = 0;
    code[AUX].lsb = 0;
    applyParam(AUX, 255);
    code[INSERTION].msb = 0;
    code[INSERTION].lsb = 0;
    applyParam(INSERTION, 255);
    code[REVERB_DELAY].msb = 0;
    code[REVERB_DELAY].lsb = 0;
    applyParam(REVERB_DELAY, 255);
    code[VARIATION].msb = 0;
    code[VARIATION].lsb = 0;
    applyParam(VARIATION, 255);
    eqParam[5] = 64;
    eqParam[6] = 28;
    eqParam[7] = 7;
    eqParam[13] = 64;
    eqParam[14] = 46;
    eqParam[15] = 7;
    applyParam(MASTER_EQ, 255);
    uploadAll();
    perf[169] = 64;
    perf[170] = 0;
    perf[172] = 64;
    perf[173] = 0;
    perf[174] = 0;
    perf[176] = 64;
    perf[177] = 0;
    perf[178] = 0;
    mixParam[1] = 0;
    applyParam(MIXER, 1);
    mixParam[6] = 0;
    applyParam(MIXER, 6);
    mixParam[9] = 0;
    applyParam(MIXER, 9);
    setOutputGain();
    setMute(255, 0);
}

}  // namespace fs1r::vop3host
