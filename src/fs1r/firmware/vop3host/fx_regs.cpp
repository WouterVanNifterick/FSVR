// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The chip's register layer, the window loaders and the engine initialisation.
#include "host.h"
#include <algorithm>

namespace fs1r::vop3host {

// 0x39648 writeSweWordSub: one 16-bit write to the effect chip's register window
void Host::writeSweWordSub(uint32_t reg, uint32_t value)
{
    sink->write(reg, value & 0xFFFF);
}

// 0x39652 writeSweWordListSub: register 0xB of n listed steps, unless the hold flag is set
void Host::writeSweWordListSub(uint32_t n, const uint16_t* steps)
{
    if (hold)
        return;
    for (uint32_t i = 0; i < n; i++) {
        writeSweWordSub(0, steps[i]);
        writeSweWordSub(0x0B, coef[steps[i]]);
    }
}

// 0x396A6 writeSweWordListLatchSub: the same through the auto-increment latch (no hold check)
void Host::writeSweWordListLatchSub(uint32_t n, const uint16_t* steps)
{
    writeSweWordSub(0, steps[0] | 0x8000);
    for (uint32_t i = 0; i < n; i++)
        writeSweWordSub(0x0B, coef[steps[i]]);
}

// 0x396FC writeSwe0CSub: register 0xC (tag) of n listed steps, unless held
void Host::writeSwe0CSub(uint32_t n, const uint16_t* steps)
{
    if (hold)
        return;
    for (uint32_t i = 0; i < n; i++) {
        writeSweWordSub(0, steps[i]);
        writeSweWordSub(0x0C, tag[steps[i]]);
    }
}

// 0x39764 writeSweBytesSub: tags through the latch
void Host::writeSweBytesSub(uint32_t n, const uint16_t* steps)
{
    writeSweWordSub(0, steps[0] | 0x8000);
    for (uint32_t i = 0; i < n; i++)
        writeSweWordSub(0x0C, tag[steps[i]]);
}

// 0x397BA writeSweIndexedSub: register 0x16 entry `index`
void Host::writeSweIndexedSub(uint32_t index, uint32_t value)
{
    writeSweWordSub(0, index + 1);
    writeSweWordSub(0x16, value);
}

// 0x397F4 writeSweDeSub: delay offsets of n listed slots as registers 0xD/0xE, unless held. The
// first slot's area is where every slot of the list is taken to start.
void Host::writeSweDeSub(uint32_t n, const uint8_t* slots)
{
    if (hold)
        return;
    const int area = rom::SLOT_AREA[slots[0]];
    const int32_t base = area >= 0 && area <= 4 ? int32_t(AREA[area]) : 0;
    for (uint32_t i = 0; i < n; i++) {
        writeSweWordSub(0, slots[i]);
        const uint32_t v = uint32_t(delay[slots[i]] + base);
        writeSweWordSub(0x0D, (v >> 9) & 0x1FF);
        writeSweWordSub(0x0E, v & 0x1FF);
    }
}

// 0x39B24..0x39BB4 writeSwe24Sub..writeSwe27Sub: one LFO register from its word cache
void Host::writeSwe24Sub(uint32_t index)
{
    if (hold) return;
    writeSweWordSub(0, index);
    writeSweWordSub(0x24, lfoReg[0][index]);
}
void Host::writeSwe25Sub(uint32_t index)
{
    if (hold) return;
    writeSweWordSub(0, index);
    writeSweWordSub(0x25, lfoReg[1][index]);
}
void Host::writeSwe26Sub(uint32_t index)
{
    if (hold) return;
    writeSweWordSub(0, index);
    writeSweWordSub(0x26, lfoReg[2][index]);
}
void Host::writeSwe27Sub(uint32_t index)
{
    if (hold) return;
    writeSweWordSub(0, index);
    writeSweWordSub(0x27, lfoReg[3][index]);
}

// 0x39C74 writeSwe20Sub: LFO phase reset mask, then clear register 0x1C
void Host::writeSwe20Sub(uint32_t mask)
{
    writeSweWordSub(0x20, mask);
    writeSweWordSub(0x1C, 0);
}

const Window WINDOWS[5] = {
    {rom::STEPS_REVERB, 94, rom::SLOTS_REVERB, 54},
    {rom::STEPS_AUX, 40, rom::SLOTS_AUX, 10, rom::LFOS_AUX, 0x000F, rom::AUX_PROGRAM, rom::AUX_TAGS},
    {rom::STEPS_INSERTION, 96, rom::SLOTS_INSERTION, 40, rom::LFOS_INSERTION, 0xF000, rom::INSERTION_PROGRAMS, rom::INSERTION_TAGS, 14},
    {rom::STEPS_REVERB_DELAY, 72, rom::SLOTS_REVERB_DELAY, 12, rom::LFOS_REVERB_DELAY, 0x00F0, rom::REVERB_DELAY_PROGRAMS, rom::DELAY_TAGS, 11},
    {rom::STEPS_VARIATION, 72, rom::SLOTS_VARIATION, 12, rom::LFOS_VARIATION, 0x0F00, rom::VARIATION_PROGRAMS, rom::DELAY_TAGS, 11},
};

// 0x39C8A loadSubCoeffBlock: the tag and the five program words of every step of a window, for
// its type msb or, with `clear`, the program that silences it. Kind 0 is the base program.
void Host::loadSubCoeffBlock(int kind, int clear)
{
    if (kind >= 1 && kind <= 4) {
        const Window& w = WINDOWS[kind];
        const int entry = clear ? w.clearEntry : code[kind].msb;
        // the tag shadow is filled from the window's first step on, not step by step
        std::copy_n(w.tags + entry * w.nsteps, w.nsteps, tag + w.steps[0]);
        writeSweBytesSub(w.nsteps, w.steps);
        const uint16_t (*program)[5] = w.programs + entry * w.nsteps;
        for (int word = 0; word < 5; word++) {
            writeSweWordSub(0, w.steps[0] | 0x8000);
            for (int s = 0; s < w.nsteps; s++)
                writeSweWordSub(10 - word, program[s][word]);
        }
        return;
    }
    std::copy_n(rom::BASE_TAGS[0], 232, tag);
    writeSwe0CSub(50, rom::STEPS_MASTER_EQ);
    writeSwe0CSub(88, rom::STEPS_SYSTEM);
    for (int word = 0; word < 5; word++) {
        writeSweWordSub(0, 0x8000);
        for (int s = 0; s < 232; s++)
            writeSweWordSub(10 - word, rom::BASE_PROGRAM[s][word]);
    }
}

// 0x398C2 loadSubEffectType: bracket the chip, upload a window's program, constants, delay
// offsets and LFO registers, reset its LFO phases, unbracket.
void Host::loadSubEffectType(int kind)
{
    writeSweIndexedSub(0, 0x40);
    writeSweWordSub(0x17, 0x23);
    switch (kind) {
    case 0:
        writeSweDeSub(54, rom::SLOTS_REVERB);
        writeSweWordListSub(94, rom::STEPS_REVERB);
        std::copy_n(rom::BASE_TAGS[0], 232, tag);
        writeSwe0CSub(94, rom::STEPS_REVERB);
        break;
    case 1: case 2: case 3: case 4: {
        const Window& w = WINDOWS[kind];
        loadSubCoeffBlock(kind, 0);
        writeSweDeSub(w.nslots, w.slots);
        writeSweWordListLatchSub(w.nsteps, w.steps);
        for (int i = 0; i < 4; i++) {
            writeSwe24Sub(w.lfos[i]);
            writeSwe25Sub(w.lfos[i]);
            writeSwe26Sub(w.lfos[i]);
            writeSwe27Sub(w.lfos[i]);
        }
        writeSwe20Sub(w.lfoMask);
        break;
    }
    case 5:
        writeSweWordListSub(50, rom::STEPS_MASTER_EQ);
        break;
    default:
        writeSwe0CSub(88, rom::STEPS_SYSTEM);
        writeSweWordListSub(88, rom::STEPS_SYSTEM);
        break;
    }
    writeSweIndexedSub(0, rom::CFG16[0]);
    writeSweWordSub(0x17, 0);
}

// 0x3A064 initSubDefaults: reset, mode, the three config tables, register 0x16 config
void Host::initSubDefaults()
{
    writeSweWordSub(1, 1);
    writeSweWordSub(5, 8180);
    for (uint32_t i = 0; i < 15; i++)
        writeSweIndexedSub(i, rom::CFG16[i]);
    for (uint32_t reg : {0x28u, 0x29u, 0x2Au})
        for (uint32_t i = 0; i < 15; i++) {
            writeSweWordSub(0, i);
            writeSweWordSub(reg, reg == 0x2A ? 0 : 0x15);
        }
    writeSweWordSub(1, 0);
    setSubFlag(5, 1);
}

// 0x3A110 loadAllSubEffects
void Host::loadAllSubEffects()
{
    loadSubCoeffBlock(0, 0);
    for (int kind = 0; kind <= 6; kind++)
        loadSubEffectType(kind);
}

// 0x3A140 setSubFlag: per-effect enable byte, then the system parameter that applies it
void Host::setSubFlag(int effect, uint32_t enabled)
{
    enabled &= 0xFF;
    static const int SYS_PARAM[6] = {0, 2, 5, 10, 11, 14};
    if (effect >= 0 && effect <= 5) {
        mute[effect] = enabled == 1 ? 1 : 0;
        fxSystemParam(SYS_PARAM[effect]);
        return;
    }
    for (int e = 0; e <= 4; e++)
        setSubFlag(e, enabled);
    setSubFlag(5, enabled);
}

// 0x3AAC8 initSubEngine: the effect host's reset. Mutes, defaults, every kind's
// preset load, the master EQ mid bands, all windows, the performance's effect bytes.
void Host::initSubEngine()
{
    masterLevel = 8192;
    hold = false;
    for (int e = 0; e < 6; e++)
        mute[e] = 1;
    initSubDefaults();
    fxParamEntry(6, 255);
    code[REVERB].lsb = 0;
    fxParamEntry(0, 255);
    code[AUX].msb = 0;
    code[AUX].lsb = 0;
    fxParamEntry(1, 255);
    code[INSERTION].msb = 0;
    code[INSERTION].lsb = 0;
    fxParamEntry(2, 255);
    code[REVERB_DELAY].msb = 0;
    code[REVERB_DELAY].lsb = 0;
    fxParamEntry(3, 255);
    code[VARIATION].msb = 0;
    code[VARIATION].lsb = 0;
    fxParamEntry(4, 255);
    eqParam[5] = 64;
    eqParam[6] = 28;
    eqParam[7] = 7;
    eqParam[13] = 64;
    eqParam[14] = 46;
    eqParam[15] = 7;
    fxParamEntry(5, 255);
    loadAllSubEffects();
    perf[0xA9] = 64;
    perf[0xAA] = 0;
    perf[0xAC] = 64;
    perf[0xAD] = 0;
    perf[0xAE] = 0;
    perf[0xB0] = 64;
    perf[0xB1] = 0;
    perf[0xB2] = 0;
    mixParam[1] = 0;
    fxParamEntry(6, 1);
    mixParam[6] = 0;
    fxParamEntry(6, 6);
    mixParam[9] = 0;
    fxParamEntry(6, 9);
    setSubGain8();
    setSubFlag(255, 0);
}

}  // namespace fs1r::vop3host
