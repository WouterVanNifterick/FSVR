// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The chip's register layer, the window loaders and the engine initialisation.
#include "host.h"

namespace fs1r::vop3host {

// 0x39648 writeSweWordSub: one 16-bit write to the effect chip's register window
void Host::writeSweWordSub(uint32_t reg, uint32_t value)
{
    sink->write(reg, value & 0xFFFF);
}

// 0x39652 writeSweWordListSub: register 0xB of n listed steps, unless the hold flag is set
void Host::writeSweWordListSub(uint32_t n, uint32_t steps)
{
    if (hold)
        return;
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t step = rom::u16(steps + 2 * i);
        writeSweWordSub(0, int16_t(step) & 0xFFFF);
        writeSweWordSub(0x0B, coef[step]);
    }
}

// 0x396A6 writeSweWordListLatchSub: the same through the auto-increment latch (no hold check)
void Host::writeSweWordListLatchSub(uint32_t n, uint32_t steps)
{
    writeSweWordSub(0, (int16_t(rom::u16(steps)) | 0x8000) & 0xFFFF);
    for (uint32_t i = 0; i < n; i++)
        writeSweWordSub(0x0B, coef[rom::u16(steps + 2 * i)]);
}

// 0x396FC writeSwe0CSub: register 0xC (tag) of n listed steps, unless held
void Host::writeSwe0CSub(uint32_t n, uint32_t steps)
{
    if (hold)
        return;
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t step = rom::u16(steps + 2 * i);
        writeSweWordSub(0, int16_t(step) & 0xFFFF);
        writeSweWordSub(0x0C, tag[step]);
    }
}

// 0x39764 writeSweBytesSub: tags through the latch
void Host::writeSweBytesSub(uint32_t n, uint32_t steps)
{
    writeSweWordSub(0, (int16_t(rom::u16(steps)) | 0x8000) & 0xFFFF);
    for (uint32_t i = 0; i < n; i++)
        writeSweWordSub(0x0C, tag[rom::u16(steps + 2 * i)]);
}

// 0x397BA writeSweIndexedSub: register 0x16 entry `index`
void Host::writeSweIndexedSub(uint32_t index, uint32_t value)
{
    writeSweWordSub(0, index + 1);
    writeSweWordSub(0x16, value);
}

// 0x397F4 writeSweDeSub: delay offsets of n listed slots as registers 0xD/0xE. The area base
// is chosen by the FIRST slot's selector byte (0x3720A4) and applied to every slot of the list.
void Host::writeSweDeSub(uint32_t n, uint32_t slots)
{
    uint8_t ids[128];
    if (n > 128) n = 128;
    for (uint32_t i = 0; i < n; i++)
        ids[i] = uint8_t(rom::u8(slots + i));
    writeSweDeSubIds(n, ids);
}

void Host::writeSweDeSubIds(uint32_t n, const uint8_t* ids)
{
    if (hold)
        return;
    const int area = rom::s8(rom::FX_SLOT_AREA + ids[0]);
    int32_t base = 0;
    if (area >= 0 && area <= 4)
        base = int32_t(AREA[area]);
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t slot = ids[i];
        writeSweWordSub(0, slot);
        const uint32_t v = uint32_t(delay[slot] + base);
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

namespace {
struct Window {
    uint32_t steps_list, nsteps, tags, tag_stride, prog, prog_stride;
    uint32_t clear_entry;
    bool tag_wrap;     // kind 1 computes the tag table offset in a byte: (entry * 0x28) & 0xFF
};
// kinds 1..4 of loadSubCoeffBlock (flash 0x39C8A); the entry is the kind's msb type byte
const Window WINDOWS[5] = {
    {},
    {0x35E11E, 0x28, 0x3669F4, 0x28, 0x3663B4, 0x190, 3, true},
    {0x35E17C, 0x60, 0x36A2D4, 0x60, 0x366A94, 0x3C0, 0xE, false},
    {0x35E268, 0x48, 0x36EBF4, 0x48, 0x36A874, 0x2D0, 0xB, false},
    {0x35E308, 0x48, 0x36EBF4, 0x48, 0x36CA34, 0x2D0, 0xB, false},
};
}  // namespace

// 0x39C8A loadSubCoeffBlock: the tags and program words of one window. kind 0 is the base
// program (232 steps from 0x3658D4, written from step 0 with the latch); kinds 1-4 take the
// entry from the kind's msb type byte, or the clear program when `clear` is set.
void Host::loadSubCoeffBlock(int kind, int clear)
{
    if (kind >= 1 && kind <= 4) {
        const Window& w = WINDOWS[kind];
        const uint32_t entry = clear ? w.clear_entry : code[kind].msb;
        const uint32_t tag_off = w.tag_wrap ? ((entry * w.tag_stride) & 0xFF) : uint32_t(int16_t(entry * w.tag_stride));
        // the firmware indexes the tag shadow from the list's FIRST step, not step by step
        const uint32_t first = rom::u16(w.steps_list);
        for (uint32_t i = 0; i < w.nsteps; i++)
            tag[first + i] = rom::u8(w.tags + tag_off + i);
        writeSweBytesSub(w.nsteps, w.steps_list);     // latched burst
        const uint32_t prog = w.prog + uint32_t(int16_t(entry * w.prog_stride));
        for (uint32_t word = 0; word < 5; word++) {
            writeSweWordSub(0, (int16_t(rom::u16(w.steps_list)) | 0x8000) & 0xFFFF);
            for (uint32_t s = 0; s < w.nsteps; s++)
                writeSweWordSub(10 - word, rom::u16(prog + 10 * s + 2 * word));
        }
        return;
    }
    // kind 0 (and anything else): the base program
    for (uint32_t i = 0; i < 232; i++)
        tag[i] = rom::u8(rom::FX_TAGS_BASE + i);
    writeSwe0CSub(0x32, 0x35E3A8);
    writeSwe0CSub(0x58, 0x35E40C);
    for (uint32_t word = 0; word < 5; word++) {
        writeSweWordSub(0, 0x8000);
        for (uint32_t s = 0; s < 232; s++)
            writeSweWordSub(10 - word, rom::u16(rom::FX_PROG_BASE + 10 * s + 2 * word));
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
        writeSweDeSub(0x36, 0x35E0E8);
        writeSweWordListSub(0x5E, 0x35E02C);
        for (uint32_t i = 0; i < 232; i++)
            tag[i] = rom::u8(rom::FX_TAGS_BASE + i);
        writeSwe0CSub(0x5E, 0x35E02C);
        break;
    case 1: case 2: case 3: case 4: {
        static const struct { uint32_t nslots, slots, nsteps, steps, lfo_ids, mask; } K[5] = {
            {}, {10, 0x35E16E, 0x28, 0x35E11E, 0x35E178, 0x000F}, {0x28, 0x35E23C, 0x60, 0x35E17C, 0x35E264, 0xF000},
            {0x0C, 0x35E2F8, 0x48, 0x35E268, 0x35E304, 0x00F0}, {0x0C, 0x35E398, 0x48, 0x35E308, 0x35E3A4, 0x0F00},
        };
        const auto& k = K[kind];
        loadSubCoeffBlock(kind, 0);
        writeSweDeSub(k.nslots, k.slots);
        writeSweWordListLatchSub(k.nsteps, k.steps);
        for (uint32_t i = 0; i < 4; i++) {
            const uint32_t id = rom::u8(k.lfo_ids + i);
            writeSwe24Sub(id);
            writeSwe25Sub(id);
            writeSwe26Sub(id);
            writeSwe27Sub(id);
        }
        writeSwe20Sub(k.mask);
        break;
    }
    case 5:
        writeSweWordListSub(0x32, 0x35E3A8);
        break;
    default:
        writeSwe0CSub(0x58, 0x35E40C);
        writeSweWordListSub(0x58, 0x35E40C);
        break;
    }
    writeSweIndexedSub(0, rom::u8(rom::FX_CFG16));
    writeSweWordSub(0x17, 0);
}

// 0x3A064 initSubDefaults: reset, mode, the three config tables, register 0x16 config
void Host::initSubDefaults()
{
    writeSweWordSub(1, 1);
    writeSweWordSub(5, 8180);
    for (uint32_t i = 0; i < 15; i++)
        writeSweIndexedSub(i, rom::u8(rom::FX_CFG16 + i));
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
