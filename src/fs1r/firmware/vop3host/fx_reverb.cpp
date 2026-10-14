// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Reverb 1-12 (halls and rooms) and the design helpers they share.
#include "effect_host.h"

namespace fs1r::vop3host {

using namespace rom::rev;

// ---- small helpers ----
int32_t EffectHost::bipolar500(int v) { return 500 * (v - 64); }                      // 0x39CF62
int32_t EffectHost::bipolar376(int v) { return 376 * (v - 64); }                      // 0x39CF72
int32_t EffectHost::lessDiffusion(int x, int v)                                               // 0x39D468
{
    const int32_t d = x - int32_t(rom::DIFFUSION[v]);
    return d > 0 ? d : 1;
}
int32_t EffectHost::toSamples(int v) { return int32_t((19660u * (uint32_t(v) & 0xFFFF)) >> 12); }   // 0x39D47C
int32_t EffectHost::scaleDelay(int v) { return int16_t((31457u * (uint32_t(v) & 0xFFFF)) >> 16); }    // 0x39D490
// 0x39D4A2, 0x39D500, 0x39D53E, 0x39D598: x at the level the window's type runs at, in 64ths
int32_t EffectHost::typeGain(int window, int x)
{
    const TypeCode c = code[window];
    const int level = window == REVERB ? rom::TYPE_LEVEL[c.lsb]
                    : window == INSERTION ? rom::TYPE_SCALE_INSERTION[18 * c.msb + c.lsb]
                    : rom::TYPE_SCALE_DELAY[14 * c.msb + c.lsb];
    return (level * int16_t(x)) >> 6;
}

// 0x39C796 designCombs(time, damp, count, sel, list): the damping LPF at list[r10..], then
// `count` comb units (g, x, x, a) from the RT60 gain table rows
void EffectHost::designCombs(int time, int damp, int count, int sel, const uint16_t* list)
{
    count &= 0xFF;
    const uint32_t r10 = count == 6 ? 21 : 5;
    designFilter(1, 1, 0, rom::DAMP_FREQ[damp], 64, 0, list + r10);
    const uint8_t* row = rom::TIME_ROW + (uint32_t(time) & 0xFFFF);
    const uint8_t* classes = rom::COMB_CLASS + (uint32_t(sel) & 0xFFFF) * 6;
    for (int i = 0; i < count; i++) {
        const uint32_t cls = classes[i];
        uint32_t g = 0, loss = 0;
        if (cls != 0) {
            int32_t v = int16_t(row[0] + cls - 56);
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            g = rom::COMB_GAIN[2 * v + 1];
            loss = rom::COMB_GAIN[2 * v];
        }
        const uint16_t* unit = list + 4 * i;
        coef[unit[0]] = g;
        const float f = float(int16_t(coef[list[r10]])) * float(loss);
        const int32_t x = int(f / 32768.0f);
        coef[unit[1]] = int16_t(x);
        coef[unit[2]] = int16_t(x);
        coef[unit[3]] = coef[list[r10 + 2]];
    }
}

// 0x39C8F4 combDelays(clampSrc, count, sel, ids): delay offsets of `count` (base, a, b, c)
// slot quads from a scaled table row
void EffectHost::combDelays(int clampSrc, int count, int sel, const uint8_t* ids)
{
    const int16_t* tbl = rom::SCALED_PAIRS + (uint32_t(sel) & 0xFFFF) * 12;
    for (int i = 0; i < (count & 0xFF); i++) {
        const uint32_t a = uint32_t(scaleDelay(tbl[2 * i])) & 0xFFFF;
        const uint32_t b = uint32_t(scaleDelay(tbl[2 * i + 1])) & 0xFFFF;
        const uint8_t* q = ids + 4u * uint32_t(i);
        const int32_t base = delay[q[0]];
        delay[q[1]] = b + uint32_t(base);
        if ((i & 1) == 0) {
            delay[q[2]] = a + uint32_t(base);
            delay[q[3]] = base + lessDiffusion(int(a), clampSrc);
        } else {
            delay[q[2]] = lessDiffusion(int(a), clampSrc) + base;
            delay[q[3]] = a + uint32_t(base);
        }
    }
}

// 0x39C9E8 diffusers(src, scale, offset, count, stride, steps, slots): per unit, a delay
// offset from a scaled table word and four constants (allpass/feedback words)
void EffectHost::diffusers(int src, int scale, int offset, int count, int stride, const uint16_t* steps, const uint8_t* slots)
{
    count &= 0xFF;
    const int16_t* row = rom::ROW_TABLE + (uint32_t(offset) & 0xFFFF) * 12;
    uint32_t flags = 0;
    if (count == 6)
        flags = rom::ROW_FLAGS_A[src] & 0xFF;
    else if (count == 2)
        flags = rom::ROW_FLAGS_2[src] & 0xFF;
    else if (count == 4) {
        flags = rom::ROW_FLAGS_4[src] & 0xFF;
        row = rom::ROW_TABLE_4 + (uint32_t(offset) & 0xFFFF) * 8;
    }
    uint32_t str = uint32_t(stride) & 0xFF;
    const int16_t scale16 = int16_t(scale);
    for (int i = 0; i < count; i++) {
        int32_t w;
        if ((i == 4 || i == 5) && scale16 == 0 && count == 6)
            w = scaleDelay(row[-4]);
        else if (i == 3 && scale16 == 0 && count == 4)
            w = scaleDelay(row[-2]);
        else
            w = scaleDelay(row[0]);
        uint32_t uw = uint32_t(w) & 0xFFFF;
        if (uw == 0)
            uw = 1;
        delay[slots[2 * i + 1]] = uw + uint32_t(delay[slots[2 * i]]);
        const int16_t x = int16_t(row[1]);
        row += 2;
        int16_t s28, s26, s24, s22;
        if ((flags & 1) == 0) {
            s28 = 0x3FFF; s26 = 0; s24 = 0; s22 = 0;
        } else {
            s28 = int16_t((int32_t(x > 0 ? 1 : 0) - int32_t(x)) >> 1);
            s24 = int16_t(0x4000 - s28);
            s26 = int16_t(0x7FFF - x);
            s22 = x;
        }
        if (str & 1) {
            s28 = int16_t(s28 * 2);
            s26 = int16_t(s26 * 2);
        }
        flags >>= 1;
        str >>= 1;
        const uint16_t* unit = steps + 4 * i;
        coef[unit[0]] = uint16_t(s28);
        coef[unit[1]] = uint16_t(s24);
        coef[unit[2]] = uint16_t(s26);
        coef[unit[3]] = uint16_t(s22);
    }
}

// 0x39CC78 earlyReflections(indexA, indexB, offset, row, words, ids): four early-reflection
// taps (two offsets and two words each) plus the pre-delay pair, clamped to the area
void EffectHost::earlyReflections(int indexA, int indexB, int offset, int row, const uint16_t* words, const uint8_t* ids)
{
    const uint16_t* rowp = rom::SPAN_ROWS + (uint32_t(row) & 0xFFFF) * 18;
    const uint32_t t = toSamples(rom::DELAY_WORD[indexA]);
    const uint32_t limit = AREA[1] - 1;
    const uint32_t diff = rom::DIFFUSION[offset];
    const uint32_t ref0 = ER_BASE_SLOTS[0], ref1 = ER_BASE_SLOTS[1];
    for (int i = 0; i < 4; i++) {
        const uint32_t w = uint32_t(scaleDelay(rowp[1])) & 0xFFFF;
        const uint32_t d = t + w;
        if (uint32_t(delay[ref0]) + d + diff < limit) {
            coef[words[i]] = rowp[6];
            coef[words[i + 5]] = rowp[12];
        } else {
            coef[words[i]] = 0;
            coef[words[i + 5]] = 0;
        }
        const uint32_t s0 = ids[2 * i], s1 = ids[2 * i + 1];
        if ((i & 1) == 0) {
            delay[s0] = uint32_t(delay[ref0]) + d;
            delay[s1] = uint32_t(delay[ref1]) + diff + d;
        } else {
            delay[s0] = uint32_t(delay[ref0]) + diff + d;
            delay[s1] = uint32_t(delay[ref1]) + d;
        }
        rowp += 1;
    }
    const uint32_t tt = uint32_t(toSamples(rom::DELAY_WORD[indexA])) +
                        uint32_t(toSamples(rom::DELAY_WORD[indexB]));
    const uint32_t s10 = ER_BASE_SLOTS[10], s11 = ER_BASE_SLOTS[11];
    delay[s10] = uint32_t(delay[ref0]) + tt;
    delay[s11] = tt + uint32_t(delay[ref1]);
}

// 0x39CEC8 erBalance(balance, list): the first ten early-reflection words scaled in
// place by the ER/Rev law (the source list is hard-wired to ER_WORDS), then twelve words of
// the complementary gain
void EffectHost::erBalance(int balance, const uint16_t* list)
{
    const uint32_t idx = uint32_t(balance) & 0xFFFF;
    for (int i = 0; i < 10; i++) {
        const float f1 = float(int16_t(coef[ER_WORDS[i]]));
        const float f2 = float(rom::ER_REV[(128u - idx) & 0xFFFFFFFFu]);
        const int32_t x = int(f2 * f1 / 32768.0f);
        coef[list[i]] = int16_t(x) >> 1;
    }
    const uint32_t g = uint16_t(rom::ER_REV[idx]);
    for (int i = 10; i < 22; i++)
        coef[list[i]] = g;
}

// 0x39D2C4 roomDelays(clampSrc, index, span, ids): early reflection delay offsets from room table C
void EffectHost::roomDelays(int clampSrc, int index, int span, const uint8_t* ids)
{
    auto pair = [&](const uint16_t* tp, uint32_t& a, uint32_t& b) {
        a = uint32_t(scaleDelay(int16_t(tp[0]))) & 0xFFFF;
        b = uint32_t(scaleDelay(int(tp[0] >> 1))) & 0xFFFF;
    };
    uint32_t a, b;
    const uint16_t* tp = rom::ROOM_C + (uint32_t(index) & 0xFFFF);
    pair(tp, a, b);
    int32_t base = delay[ids[0]];
    delay[ids[1]] = a + uint32_t(base);
    delay[ids[2]] = b + uint32_t(base);
    delay[ids[3]] = base + lessDiffusion(int(b), clampSrc);
    const uint16_t* tp2 = rom::ROOM_C + (uint32_t(index) & 0xFFFF) + (uint32_t(span) & 0xFFFF);
    pair(tp2, a, b);
    base = delay[ids[4]];
    delay[ids[5]] = a + uint32_t(base);
    delay[ids[6]] = lessDiffusion(int(b), clampSrc) + base;
    delay[ids[7]] = b + uint32_t(base);
}

// 0x39CF82 designRoom(time, y0, y1, damp, extra, list): the room's early reflection units:
// the damping LPF, then two comb-like units from the RT60 gain row of the time nearest to
// extra * RT60[time] / 10
void EffectHost::designRoom(int time, int y0, int y1, int damp, int extra, const uint16_t* list)
{
    const float target = float(double(float(uint32_t(extra) & 0xFFFF) * rom::RT60_SECONDS[time]) / 10.0);
    int32_t k = 0;
    while (target > rom::RT60_SECONDS[k + 1])
        k++;
    const float mid = rom::RT60_SECONDS[k + 1] + rom::RT60_SECONDS[k];
    if (0.0f > mid - 2.0f * target)
        k++;
    const uint8_t* rowp = rom::TIME_ROW + uint32_t(int16_t(k));
    const int32_t dampf = rom::DAMP_FREQ[damp];
    const uint8_t* ya = rom::ROOM_A + (uint32_t(y0) & 0xFFFF);
    const uint8_t* yb = rom::ROOM_A + (uint32_t(y0) & 0xFFFF) + (uint32_t(y1) & 0xFFFF);
    auto unit = [&](const uint16_t* u, const uint8_t* src, const uint16_t* lpf) {
        int32_t v = int16_t(src[0] + rowp[0] - 56);
        v = v < 0 ? 0 : v > 255 ? 255 : v;
        const uint16_t* entry = rom::COMB_GAIN + 2 * v;
        coef[u[0]] = entry[1];
        const float f = float(int16_t(coef[lpf[0]])) * float(entry[0]);
        const int32_t x = int(f / 32768.0f);
        coef[u[1]] = int16_t(x);
        coef[u[2]] = int16_t(x);
        coef[u[3]] = coef[lpf[2]];
    };
    designFilter(1, 1, 0, dampf, 64, 0, list + 5);
    unit(list, ya, list + 5);
    unit(list + 4, yb, list + 5);
}

// ---- the handlers ----
namespace {
// the three (row, class, offset) words of the reverb type's comb selector
inline const int16_t* sel_row(const EffectHost& h) { return rom::COMB_SELECT + 3 * h.code[REVERB].lsb; }
}  // namespace

// 0x3A3728 fxRevHall: reverb types 1-8
void EffectHost::fxRevHall(int p)
{
    const int16_t* sel = sel_row(*this);
    auto prm = [&](int i) -> int32_t { return param[REVERB][i]; };
    switch (p) {
    case 0: case 13:
        designCombs(prm(0), prm(13), 6, sel[1], HALL_COMB_STEPS);
        writeCoefs(0x18, HALL_COMB_STEPS);
        break;
    case 1: {
        combDelays(prm(1), 6, sel[1], COMB_SLOTS);
        uint8_t ids[12];
        for (int i = 0; i < 6; i++) {
            ids[2 * i] = uint8_t(COMB_SLOTS[4 * i + 2]);
            ids[2 * i + 1] = uint8_t(COMB_SLOTS[4 * i + 3]);
        }
        writeDelays(12, ids);
        earlyReflections(prm(2), prm(10), prm(1), sel[0], ER_WORDS, ER_BASE_SLOTS + 2);
        erBalance(prm(12), ER_WORDS);
        writeDelays(10, ER_BASE_SLOTS + 2);
        diffusers(prm(11), prm(1), sel[2], 6, 20, ROW_STEPS, ROW_SLOTS);
        writeDelays(1, ROW_SLOTS + 9);
        writeDelays(1, ROW_SLOTS + 11);
        break;
    }
    case 2: case 10:
        earlyReflections(prm(2), prm(10), prm(1), sel[0], ER_WORDS, ER_BASE_SLOTS + 2);
        erBalance(prm(12), ER_WORDS);
        writeCoefs(10, ER_WORDS);
        writeDelays(10, ER_BASE_SLOTS + 2);
        break;
    case 3:
        designFilter(1, 0, 1, prm(3), 64, 0, HPF_STEPS);
        copyCoefs(3, HPF_STEPS, HPF_COPY);
        break;
    case 4:
        designFilter(1, 1, 0, prm(4), 64, 0, LPF_STEPS);
        copyCoefs(3, LPF_STEPS, LPF_COPY);
        break;
    case 11:
        diffusers(prm(11), prm(1), sel[2], 6, 20, ROW_STEPS, ROW_SLOTS);
        writeCoefs(0x18, ROW_STEPS);
        break;
    case 12:
        earlyReflections(prm(2), prm(10), prm(1), sel[0], ER_WORDS, ER_BASE_SLOTS + 2);
        erBalance(prm(12), ER_WORDS);
        writeCoefs(10, ER_WORDS);
        writeCoefs(0xC, ER_WORDS + 10);
        break;
    case 14:
        coef[WIDTH_STEPS[0]] = bipolar500(prm(14));
        coef[WIDTH_STEPS[1]] = bipolar500(prm(14));
        writeCoefs(2, WIDTH_STEPS);
        break;
    case 255: {
        hold = true;
        const uint32_t idx = rom::REVERB_PRESETS.blockOf[code[REVERB].lsb];
        if (idx == 1 || idx == 2) {
            const uint16_t* src = idx == 1 ? rom::HALL_OFFSETS_A : rom::HALL_OFFSETS_B;
            for (uint32_t i = 0; i < 13; i++)
                delay[rom::RESET_SLOTS_REVERB[i + 1]] = src[i];
        }
        const uint32_t span = AREA[1] - AREA[0];
        delay[rom::RESET_SLOTS_REVERB[0]] = ((span - 28653u) >> 1) + 28652u;
        coef[MIX_STEPS[0]] = 0x4000;
        coef[MIX_STEPS[1]] = 0x4000;
        coef[MIX_STEPS[2]] = 0;
        coef[MIX_STEPS[3]] = 8192;
        coef[MIX_STEPS[4]] = 8192;
        for (int q : {0, 1, 2, 3, 4, 11, 12, 14})
            fxRevHall(q);
        designTypeShelf(code[REVERB].lsb, TABLE_STEPS);
        const uint32_t w = typeGain(REVERB, 8192);
        coef[LEVEL_STEPS[1]] = w;
        coef[LEVEL_STEPS[0]] = w;
        hold = false;
        break;
    }
    }
}

// 0x3A3C54 fxRevRoom: reverb types 9-12
void EffectHost::fxRevRoom(int p)
{
    const int16_t* sel = sel_row(*this);
    auto prm = [&](int i) -> int32_t { return param[REVERB][i]; };
    const int16_t* roomrow = rom::ROOM_B + 6 * (code[REVERB].lsb - 9);
    auto pairs = [&](int j) {   // j = 0, 1, 2: the three early reflection groups
        roomDelays(prm(1), prm(5 + j), prm(8), COMB_SLOTS + 8u * uint32_t(j));
    };
    auto rooms = [&](int j) {
        designRoom(prm(0), prm(5 + j), prm(8), prm(13), roomrow[j], HALL_COMB_STEPS + 8 * j);
    };
    auto slots18 = [&]() {
        uint8_t ids[18];
        for (int i = 0; i < 6; i++)
            for (int k = 0; k < 3; k++)
                ids[3 * i + k] = uint8_t(COMB_SLOTS[4 * i + k + 1]);
        writeDelays(18, ids);
    };
    switch (p) {
    case 0: case 8: case 13:
        pairs(0); pairs(1); pairs(2);
        rooms(0); rooms(1); rooms(2);
        slots18();
        writeCoefs(0x18, HALL_COMB_STEPS);
        break;
    case 1:
        pairs(0); pairs(1); pairs(2);
        rooms(0); rooms(1); rooms(2);
        slots18();
        writeCoefs(0x18, HALL_COMB_STEPS);
        earlyReflections(prm(2), prm(10), prm(1), sel[0], ER_WORDS, ER_BASE_SLOTS + 2);
        erBalance(prm(12), ER_WORDS);
        writeDelays(10, ER_BASE_SLOTS + 2);
        diffusers(prm(11), prm(1), sel[2], 6, 20, ROW_STEPS, ROW_SLOTS);
        writeDelays(1, ROW_SLOTS + 9);
        writeDelays(1, ROW_SLOTS + 11);
        break;
    case 2: case 10:
        earlyReflections(prm(2), prm(10), prm(1), sel[0], ER_WORDS, ER_BASE_SLOTS + 2);
        erBalance(prm(12), ER_WORDS);
        writeCoefs(10, ER_WORDS);
        writeDelays(10, ER_BASE_SLOTS + 2);
        break;
    case 3:
        designFilter(1, 0, 1, prm(3), 64, 0, HPF_STEPS);
        copyCoefs(3, HPF_STEPS, HPF_COPY);
        break;
    case 4:
        designFilter(1, 1, 0, prm(4), 64, 0, LPF_STEPS);
        copyCoefs(3, LPF_STEPS, LPF_COPY);
        break;
    case 5: case 6: case 7: {
        const int j = p - 5;
        pairs(j);
        rooms(j);
        writeDelays(3, COMB_SLOTS + 8u * uint32_t(j) + 1);
        writeDelays(3, COMB_SLOTS + 8u * uint32_t(j) + 5);
        writeCoefs(8, HALL_COMB_STEPS + 8 * j);
        break;
    }
    case 11:
        diffusers(prm(11), prm(1), sel[2], 6, 20, ROW_STEPS, ROW_SLOTS);
        writeCoefs(0x18, ROW_STEPS);
        break;
    case 12:
        earlyReflections(prm(2), prm(10), prm(1), sel[0], ER_WORDS, ER_BASE_SLOTS + 2);
        erBalance(prm(12), ER_WORDS);
        writeCoefs(10, ER_WORDS);
        writeCoefs(0xC, ER_WORDS + 10);
        break;
    case 14:
        coef[WIDTH_STEPS[0]] = bipolar500(prm(14));
        coef[WIDTH_STEPS[1]] = bipolar500(prm(14));
        writeCoefs(2, WIDTH_STEPS);
        break;
    case 255: {
        hold = true;
        for (uint32_t i = 0; i < 13; i++)
            delay[rom::RESET_SLOTS_REVERB[i + 1]] = rom::ROOM_OFFSETS[i];
        const uint32_t span = AREA[1] - AREA[0];
        delay[rom::RESET_SLOTS_REVERB[0]] = ((span - 28397u) >> 1) + 28652u;
        coef[MIX_STEPS[0]] = 0x4000;
        coef[MIX_STEPS[1]] = 0x4000;
        coef[MIX_STEPS[2]] = 0;
        coef[MIX_STEPS[3]] = 8192;
        coef[MIX_STEPS[4]] = 8192;
        for (int q : {0, 1, 2, 3, 4, 5, 6, 7, 11, 12, 14})
            fxRevRoom(q);
        designTypeShelf(code[REVERB].lsb, TABLE_STEPS);
        const uint32_t w = typeGain(REVERB, 8192);
        coef[LEVEL_STEPS[1]] = w;
        coef[LEVEL_STEPS[0]] = w;
        hold = false;
        break;
    }
    }
}

}  // namespace fs1r::vop3host
