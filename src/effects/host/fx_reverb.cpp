// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The reverbs: the reverb block's halls and rooms (types 1-12), the variation's reverbs and the
// insertion's early reflections, and the design helpers they share.
#include "effects/host/fx_common.h"
#include <algorithm>

namespace fsvr::vop3host {

namespace {

// the comb-gain table's entry at x clamped to 0..255: the loss word, then the gain
const uint16_t* combGain(int x)
{
    int32_t v = int16_t(x);
    v = v < 0 ? 0 : v > 255 ? 255 : v;
    return rom::COMB_GAIN + 2 * v;
}

// a comb unit (g, x, x, a): its gain, the damping lowpass's first word at the loss, and the lowpass's third
void combUnit(EffectHost& h, const uint16_t* unit, const uint16_t* lpf, uint32_t g, uint32_t loss)
{
    h.coef[unit[0]] = g;
    const float f = float(int16_t(h.coef[lpf[0]])) * float(loss);
    const int32_t x = int(f / 32768.0f);
    h.coef[unit[1]] = int16_t(x);
    h.coef[unit[2]] = int16_t(x);
    h.coef[unit[3]] = h.coef[lpf[2]];
}

}  // namespace

// The damping lowpass at list[damper..], then `count` comb units (g, x, x, a) from the RT60
// gain table's rows.
void EffectHost::designCombs(int time, int damp, int count, int sel, const uint16_t* list)
{
    count &= 0xFF;
    const uint32_t damper = count == 6 ? 21 : 5;
    designFilter(1, 1, LPF1, rom::DAMP_FREQ[damp], 64, 0, list + damper);
    const uint8_t* row = rom::TIME_ROW + (uint32_t(time) & 0xFFFF);
    const uint8_t* classes = rom::COMB_CLASS + (uint32_t(sel) & 0xFFFF) * 6;
    for (int i = 0; i < count; i++) {
        const uint32_t cls = classes[i];
        const uint16_t* entry = cls != 0 ? combGain(row[0] + cls - 56) : nullptr;
        combUnit(*this, list + 4 * i, list + damper, entry ? entry[1] : 0, entry ? entry[0] : 0);
    }
}

// the offsets of `count` (base, a, b, c) slot quads from a scaled table row
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

// two (base, a, b) slot triples from the second word of each scaled table pair (the variation's
// early reflections)
void EffectHost::combDelays2(int clampSrc, int sel, const uint8_t* ids)
{
    const int16_t* tbl = rom::SCALED_PAIRS + (uint32_t(sel) & 0xFFFF) * 12;
    const uint32_t diff = rom::DIFFUSION[clampSrc];
    for (uint32_t i = 0; i < 2; i++) {
        const uint32_t w = uint32_t(scaleDelay(tbl[2 * i + 1])) & 0xFFFF;
        const uint32_t base = uint32_t(delay[ids[3 * i]]) & 0xFFFF;
        delay[ids[3 * i + 1]] = base + w;
        delay[ids[3 * i + 2]] = (w - diff) + base;
    }
}

// per unit, a delay offset from a scaled table word and four constants (allpass and feedback words)
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
            s24 = int16_t(HALF - s28);
            s26 = int16_t(UNITY - x);
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

// four early-reflection taps (two offsets and two words each) plus the pre-delay pair, clamped to the area
void EffectHost::erTaps(int indexA, int indexB, int offset, int row)
{
    const uint16_t* words = rev::ER_WORDS;
    const uint8_t* ids = rev::ER_BASE_SLOTS + 2;
    const uint16_t* rowp = rom::SPAN_ROWS + (uint32_t(row) & 0xFFFF) * 18;
    const uint32_t t = toSamples(rom::DELAY_WORD[indexA]);
    const uint32_t limit = AREA[1] - 1;
    const uint32_t diff = rom::DIFFUSION[offset];
    const uint32_t ref0 = rev::ER_BASE_SLOTS[0], ref1 = rev::ER_BASE_SLOTS[1];
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
    delay[rev::ER_BASE_SLOTS[10]] = uint32_t(delay[ref0]) + tt;
    delay[rev::ER_BASE_SLOTS[11]] = tt + uint32_t(delay[ref1]);
}

// The first ten early-reflection words (always the reverb's) scaled by the ER/Rev balance into
// the list, then twelve words of the complementary gain.
void EffectHost::erBalance(int balance)
{
    const uint16_t* list = rev::ER_WORDS;
    const uint32_t idx = uint32_t(balance) & 0xFFFF;
    for (int i = 0; i < 10; i++) {
        const float f1 = float(int16_t(coef[rev::ER_WORDS[i]]));
        const float f2 = float(rom::ER_REV[128u - idx]);
        const int32_t x = int(f2 * f1 / 32768.0f);
        coef[list[i]] = int16_t(x) >> 1;
    }
    const uint32_t g = uint16_t(rom::ER_REV[idx]);
    for (int i = 10; i < 22; i++)
        coef[list[i]] = g;
}

// early reflection delay offsets from room table C
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

// A room's early reflection units: the damping lowpass, then two comb-like units from the RT60
// gain row of the time nearest to extra * RT60[time] / 10.
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
        const uint16_t* entry = combGain(src[0] + rowp[0] - 56);
        combUnit(*this, u, lpf, entry[1], entry[0]);
    };
    designFilter(1, 1, LPF1, dampf, 64, 0, list + 5);
    unit(list, ya, list + 5);
    unit(list + 4, yb, list + 5);
}

// ---- the reverb block ----

namespace {

enum { REVERB_TIME, DIFFUSION, INIT_DELAY, HPF_CUTOFF, LPF_CUTOFF, WIDTH, HEIGHT, DEPTH, WALL_VARY,
       REV_DELAY = 10, DENSITY, ER_REV_BALANCE, HIGH_DAMP, FB_LEVEL };

// the three (row, class, offset) words of the reverb type's comb selector
const int16_t* combSelector(const EffectHost& h, int w) { return rom::COMB_SELECT + 3 * h.code[w].lsb; }

// the early reflections' taps and their ER/Rev balance, from the reverb's parameters
void reflections(EffectHost& h)
{
    const uint16_t* P = h.param[REVERB];
    h.erTaps(P[INIT_DELAY], P[REV_DELAY], P[DIFFUSION], combSelector(h, REVERB)[0]);
    h.erBalance(P[ER_REV_BALANCE]);
}

// The parameters the halls and the rooms take alike; false for the others
bool reverbParam(EffectHost& h, int p)
{
    const uint16_t* P = h.param[REVERB];
    const int16_t* sel = combSelector(h, REVERB);
    switch (p) {
        case INIT_DELAY: case REV_DELAY:
            reflections(h);
            h.writeCoefs(10, rev::ER_WORDS);
            h.writeDelays(10, rev::ER_BASE_SLOTS + 2);
            return true;
        case HPF_CUTOFF:
            h.designFilter(1, 0, HPF1, P[HPF_CUTOFF], 64, 0, rev::HPF_STEPS);
            h.copyCoefs(3, rev::HPF_STEPS, rev::HPF_COPY);
            return true;
        case LPF_CUTOFF:
            h.designFilter(1, 1, LPF1, P[LPF_CUTOFF], 64, 0, rev::LPF_STEPS);
            h.copyCoefs(3, rev::LPF_STEPS, rev::LPF_COPY);
            return true;
        case DENSITY:
            h.diffusers(P[DENSITY], P[DIFFUSION], sel[2], 6, 20, rev::ROW_STEPS, rev::ROW_SLOTS);
            h.writeCoefs(24, rev::ROW_STEPS);
            return true;
        case ER_REV_BALANCE:
            reflections(h);
            h.writeCoefs(10, rev::ER_WORDS);
            h.writeCoefs(12, rev::ER_WORDS + 10);
            return true;
        case FB_LEVEL:
            h.coef[rev::WIDTH_STEPS[0]] = h.bipolar500(P[FB_LEVEL]);
            h.coef[rev::WIDTH_STEPS[1]] = h.bipolar500(P[FB_LEVEL]);
            h.writeCoefs(2, rev::WIDTH_STEPS);
            return true;
    }
    return false;
}

// After a diffusion change: the early reflections and the diffusers' delays follow it.
void diffusionFollows(EffectHost& h)
{
    const uint16_t* P = h.param[REVERB];
    const int16_t* sel = combSelector(h, REVERB);
    reflections(h);
    h.writeDelays(10, rev::ER_BASE_SLOTS + 2);
    h.diffusers(P[DENSITY], P[DIFFUSION], sel[2], 6, 20, rev::ROW_STEPS, rev::ROW_SLOTS);
    h.writeDelays(1, rev::ROW_SLOTS + 9);
    h.writeDelays(1, rev::ROW_SLOTS + 11);
}

// every parameter of a hall or a room, after its own preset offsets and before its level
void reverbAll(EffectHost& h, uint32_t preDelayBase, EffectHost::Handler handler, std::initializer_list<int> ps)
{
    h.hold = true;
    h.delay[rom::RESET_SLOTS_REVERB[0]] = ((AREA[1] - AREA[0] - preDelayBase) >> 1) + 28652u;
    const uint16_t mix[5] = {HALF, HALF, 0, QUARTER, QUARTER};
    for (int i = 0; i < 5; i++)
        h.coef[rev::MIX_STEPS[i]] = mix[i];
    for (int p : ps)
        (h.*handler)(REVERB, p);
    h.designTypeShelf(h.code[REVERB].lsb, rev::TABLE_STEPS);
    h.coef[rev::LEVEL_STEPS[1]] = h.coef[rev::LEVEL_STEPS[0]] = h.typeGain(REVERB, QUARTER);
    h.hold = false;
}

}  // namespace

// Hall 1 and 2, Room 1 to 3, Stage 1 and 2, Plate
void EffectHost::hall(int w, int p)
{
    const uint16_t* P = param[w];
    const int16_t* sel = combSelector(*this, w);
    switch (p) {
        case REVERB_TIME: case HIGH_DAMP:
            designCombs(P[REVERB_TIME], P[HIGH_DAMP], 6, sel[1], rev::HALL_COMB_STEPS);
            writeCoefs(24, rev::HALL_COMB_STEPS);
            break;
        case DIFFUSION: {
            combDelays(P[DIFFUSION], 6, sel[1], rev::COMB_SLOTS);
            uint8_t ids[12];
            for (int i = 0; i < 6; i++) {
                ids[2 * i]     = rev::COMB_SLOTS[4 * i + 2];
                ids[2 * i + 1] = rev::COMB_SLOTS[4 * i + 3];
            }
            writeDelays(12, ids);
            diffusionFollows(*this);
            break;
        }
        case ALL: {
            // halls run from one of two sets of offsets
            const uint32_t set = rom::REVERB_PRESETS.blockOf[code[REVERB].lsb];
            if (set == 1 || set == 2)
                for (int i = 0; i < 13; i++)
                    delay[rom::RESET_SLOTS_REVERB[i + 1]] = (set == 1 ? rom::HALL_OFFSETS_A : rom::HALL_OFFSETS_B)[i];
            reverbAll(*this, 28653u, &EffectHost::hall, {REVERB_TIME, DIFFUSION, INIT_DELAY, HPF_CUTOFF, LPF_CUTOFF, DENSITY, ER_REV_BALANCE, FB_LEVEL});
            break;
        }
        default:
            reverbParam(*this, p);
            break;
    }
}

// White Room, Tunnel, Basement, Canyon: three early reflection groups sized by width, height and depth
void EffectHost::room(int w, int p)
{
    const uint16_t* P = param[w];
    const int16_t* extra = rom::ROOM_B + 6 * (code[REVERB].lsb - 9);
    auto group = [&](int j) {
        roomDelays(P[DIFFUSION], P[WIDTH + j], P[WALL_VARY], rev::COMB_SLOTS + 8 * j);
        designRoom(P[REVERB_TIME], P[WIDTH + j], P[WALL_VARY], P[HIGH_DAMP], extra[j], rev::HALL_COMB_STEPS + 8 * j);
    };
    auto allGroups = [&]() {
        roomDelays(P[DIFFUSION], P[WIDTH], P[WALL_VARY], rev::COMB_SLOTS);
        roomDelays(P[DIFFUSION], P[HEIGHT], P[WALL_VARY], rev::COMB_SLOTS + 8);
        roomDelays(P[DIFFUSION], P[DEPTH], P[WALL_VARY], rev::COMB_SLOTS + 16);
        for (int j = 0; j < 3; j++)
            designRoom(P[REVERB_TIME], P[WIDTH + j], P[WALL_VARY], P[HIGH_DAMP], extra[j], rev::HALL_COMB_STEPS + 8 * j);
        uint8_t ids[18];
        for (int i = 0; i < 6; i++)
            for (int k = 0; k < 3; k++)
                ids[3 * i + k] = rev::COMB_SLOTS[4 * i + k + 1];
        writeDelays(18, ids);
        writeCoefs(24, rev::HALL_COMB_STEPS);
    };
    switch (p) {
        case REVERB_TIME: case WALL_VARY: case HIGH_DAMP:
            allGroups();
            break;
        case DIFFUSION:
            allGroups();
            diffusionFollows(*this);
            break;
        case WIDTH: case HEIGHT: case DEPTH: {
            const int j = p - WIDTH;
            group(j);
            writeDelays(3, rev::COMB_SLOTS + 8 * j + 1);
            writeDelays(3, rev::COMB_SLOTS + 8 * j + 5);
            writeCoefs(8, rev::HALL_COMB_STEPS + 8 * j);
            break;
        }
        case ALL:
            for (int i = 0; i < 13; i++)
                delay[rom::RESET_SLOTS_REVERB[i + 1]] = rom::ROOM_OFFSETS[i];
            reverbAll(*this, 28397u, &EffectHost::room, {REVERB_TIME, DIFFUSION, INIT_DELAY, HPF_CUTOFF, LPF_CUTOFF, WIDTH, HEIGHT, DEPTH,
                                                       DENSITY, ER_REV_BALANCE, FB_LEVEL});
            break;
        default:
            reverbParam(*this, p);
            break;
    }
}

// ---- the variation's ----

// Hall, Room, Stage, Plate (types 25-28): the comb selector row by the type's lsb
void EffectHost::variationReverb(int w, int p)
{
    enum { DRY_WET = 9 };
    const uint16_t* P = param[w];
    const int16_t* sel = combSelector(*this, w);
    switch (p) {
        case REVERB_TIME: case HIGH_DAMP:
            designCombs(P[REVERB_TIME], P[HIGH_DAMP], 2, sel[1], var::REVERB_COMBS);
            writeCoefs(8, var::REVERB_COMBS);
            break;
        case DIFFUSION: {
            combDelays2(P[DIFFUSION], sel[1], var::REVERB_ER_IDS);
            const uint8_t ids[2] = {var::REVERB_ER_IDS[2], var::REVERB_ER_IDS[5]};
            writeDelays(2, ids);
            variationReverb(w, ER_REV_BALANCE);
            break;
        }
        case INIT_DELAY:
            delay[var::REVERB_PRE[1]] = (uint32_t(delay[var::REVERB_PRE[0]]) + toSamples(rom::DELAY_WORD[P[INIT_DELAY]])) & 0xFFFF;
            writeDelays(1, var::REVERB_PRE + 1);
            break;
        case HPF_CUTOFF:
            designFilter(1, 0, HPF1, P[HPF_CUTOFF], 64, 0, var::REVERB_HPF);
            break;
        case LPF_CUTOFF:
            designFilter(1, 1, LPF1, P[LPF_CUTOFF], 64, 0, var::REVERB_LPF);
            break;
        case DRY_WET:
            dryWet(w, var::REVERB_PAN, false, rom::ER_REV);
            break;
        case DENSITY:
            diffusers(P[DENSITY], P[DIFFUSION], sel[2], 2, 2, var::REVERB_ROW_STEPS, var::REVERB_ROW_SLOTS);
            writeCoefs(8, var::REVERB_ROW_STEPS);
            break;
        case ER_REV_BALANCE: {
            // the early reflections' level into the first word and into the one the diffusion picks,
            // the reverb's into the last four
            const int32_t row = int16_t(rom::SPAN_ROWS[18 * (uint32_t(sel[0]) & 0xFFFF) + 6]);
            const uint16_t er = int16_t(uint32_t(row * rom::ER_REV[128 - P[ER_REV_BALANCE]]) >> 16);
            coef[var::REVERB_W[0]] = er;
            for (int i = 1; i <= 5; i++)
                coef[var::REVERB_W[i]] = 0;
            coef[var::REVERB_W[std::min<int>(P[DIFFUSION], 4) + 1]] = er;
            for (int i = 6; i <= 9; i++)
                coef[var::REVERB_W[i]] = (3 * rom::ER_REV[P[ER_REV_BALANCE]]) >> 2;
            writeCoefs(10, var::REVERB_W);
            break;
        }
        case FB_LEVEL:
            coef[var::REVERB_WIDTH[0]] = bipolar500(P[FB_LEVEL]);
            writeCoefs(1, var::REVERB_WIDTH);
            break;
        case ALL:
            applyAll(w, &EffectHost::variationReverb, {REVERB_TIME, DIFFUSION, INIT_DELAY, HPF_CUTOFF, LPF_CUTOFF, DRY_WET, DENSITY, FB_LEVEL});
            break;
    }
}

// ---- the insertion's ----

namespace {

// the comb-gain table's loss word at x clamped to 0..255
int32_t erGain(int x) { return int16_t(combGain(x)[0]); }

// The 20 early reflections of the pattern row (the early or gate type, past six for Gate Reverb
// and past eight for Reverse Gate), spread by the room size after the initial delay, at the
// liveness's level, clamped to the delay area.
void erPattern(EffectHost& h)
{
    const uint16_t* P = h.param[INSERTION];
    const uint32_t t = h.code[INSERTION].lsb;
    const uint32_t rowi = (P[0] + (t == 3 ? 6u : t == 4 ? 8u : 0u)) & 0xFFFF;
    const int16_t* row = rom::ER_PATTERNS + rowi * 40;
    const uint32_t size = rom::ER_DENSITY[P[1]];
    const int32_t level = int32_t(rom::ER_LEVEL[P[10]]) - 128;
    const int32_t base = h.toSamples(rom::DELAY_WORD[P[3]]) + h.delay[ins::ER_SLOTS[21]];
    const int32_t limit = int32_t(AREA[3] - AREA[2]) - 1;
    for (uint32_t i = 0; i < 20; i++, row += 2) {
        const uint32_t w = uint32_t(h.scaleDelay(row[0])) & 0xFFFF;
        int32_t pos = base + int32_t(size * w) / 10;
        int32_t gain;
        if (pos > limit) {
            pos = limit;
            gain = 0;
        } else {
            const int32_t g = row[1];
            gain = g > 0 ? int16_t(uint32_t(erGain(g + level))) : -int16_t(uint32_t(erGain(level - g)));
        }
        h.delay[ins::ER_SLOTS[i]] = pos;
        h.coef[ins::ER_STEPS[i]] = (gain + (gain < 0 ? 1 : 0)) >> 1;
        if (i == 19)
            h.delay[ins::ER_SLOTS[20]] = pos > 0x8000 ? 0x8000 : pos;
    }
    h.coef[ins::ER_STEPS[19]] = 0;
    if (t == 3)                                     // Gate Reverb has fifteen
        for (uint32_t k = 15; k <= 18; k++)
            h.coef[ins::ER_STEPS[k]] = 0;
}

}  // namespace

// ER 1, ER 2, Gate Reverb, Reverse Gate (types 37-40)
void EffectHost::earlyReflections(int w, int p)
{
    enum { TYPE, ROOM_SIZE, DIFFUSION, INIT_DELAY, FB_LEVEL, HPF_CUTOFF, LPF_CUTOFF, DRY_WET = 9, LIVENESS, DENSITY, HIGH_DAMP };
    const uint16_t* P = param[w];
    auto diffuse = [&]() {
        diffusers(P[DENSITY], P[DIFFUSION], rom::ER_ROWS[6 * code[w].lsb + P[TYPE]], 4, 0, ins::ER_ROW_STEPS, ins::ER_ROW_SLOTS);
    };
    switch (p) {
        case TYPE:
            erPattern(*this);
            diffuse();
            writeCoefs(20, ins::ER_STEPS);
            writeDelays(21, ins::ER_SLOTS);
            writeCoefs(16, ins::ER_ROW_STEPS);
            writeDelays(8, ins::ER_ROW_SLOTS);
            break;
        case ROOM_SIZE: case INIT_DELAY:
            erPattern(*this);
            writeCoefs(20, ins::ER_STEPS);
            writeDelays(21, ins::ER_SLOTS);
            break;
        case LIVENESS:
            erPattern(*this);
            writeCoefs(20, ins::ER_STEPS);
            break;
        case DIFFUSION: {
            const uint8_t* s = ins::ER_DIFF_SLOTS;
            const int32_t d = rom::DIFFUSION[P[DIFFUSION]];
            delay[s[2]] = delay[s[0]] + d + 1;
            delay[s[3]] = delay[s[1]] + d + 1;
            writeDelays(2, s + 2);
            diffuse();
            writeDelays(1, ins::ER_ROW_SLOTS + 7);
            break;
        }
        case FB_LEVEL:
            coef[ins::ER_FEEDBACK[0]] = bipolar500(P[FB_LEVEL]);
            writeCoefs(1, ins::ER_FEEDBACK);
            break;
        case HPF_CUTOFF:
            designFilter(2, 1, HPF1, P[HPF_CUTOFF], 64, 0, ins::ER_HPF);
            break;
        case LPF_CUTOFF:
            designFilter(2, 2, LPF1, P[LPF_CUTOFF], 64, 0, ins::ER_LPF_2);
            break;
        case DRY_WET:
            dryWet(w, ins::ER_WET, true, rom::ER_REV);
            break;
        case DENSITY:
            diffuse();
            writeCoefs(16, ins::ER_ROW_STEPS);
            break;
        case HIGH_DAMP:
            designFilter(1, 0, LPF1, rom::DAMP_FREQ[P[HIGH_DAMP]], 64, 0, ins::ER_LPF);
            break;
        case ALL:
            applyAll(w, &EffectHost::earlyReflections, {TYPE, DIFFUSION, FB_LEVEL, HPF_CUTOFF, LPF_CUTOFF, DRY_WET, HIGH_DAMP});
            break;
    }
}

}  // namespace fsvr::vop3host
