// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// Reverb 1-12 (halls and rooms) and the design helpers they share.
#include "host.h"

namespace fs1r::vop3host {

using namespace ram;

namespace {
constexpr uint32_t COMB_SLOTS = 0x35DB39;        // u8 x 26: (base, a, b, c) x 6 then 2
constexpr uint32_t ROW_STEPS = 0x35DBF4;         // u16 x 24: fxScaleRow's 6 x 4 steps
constexpr uint32_t ROW_SLOTS = 0x35DBCC;         // u8 x 14: fxScaleRow's slot pairs, then 2 singles
constexpr uint32_t ER_WORDS = 0x35DCA4;          // u16 x 24: early reflection words
constexpr uint32_t ER_SLOTS = 0x35DC96;          // u8 x 14: early reflection slots
constexpr uint32_t ER_BASE_SLOTS = 0x35DC94;     // u8 x 12: the ER reference slots (first two before ER_SLOTS)
constexpr uint32_t WIDTH_STEPS = 0x35DCEC;       // u16 x 4
constexpr uint32_t HPF_STEPS = 0x35E4BC, HPF_COPY = 0x35E4C8, LPF_STEPS = 0x35E4C2, LPF_COPY = 0x35E4CE;
constexpr uint32_t TABLE_STEPS = 0x35E4D4;       // fxDesignFromTable destination
constexpr uint32_t MIX_STEPS = 0x35DF48;         // u16 x 5 at 255: 0x4000, 0x4000, 0, 8192, 8192
constexpr uint32_t LEVEL_STEPS = 0x35DF68;       // u16 x 2: the type level (fxMulTable6)
constexpr uint32_t SCALED_PAIRS = 0x36FE3C;      // s16 rows of 12, stride 0x18
constexpr uint32_t ROW_TABLE = 0x36FEB4;         // s16 rows of 12, stride 0x18 (fxScaleRow)
constexpr uint32_t ROW_FLAGS_A = 0x36FF2C, ROW_FLAGS_2 = 0x36FF40;
constexpr uint32_t ROW_FLAGS_4 = 0x370A0C, ROW_TABLE_4 = 0x370A14;
constexpr uint32_t SPAN_ROWS = 0x36FF4A;         // u16 rows of 18, stride 0x24 (fxWriteSpan4)
constexpr uint32_t TYPE_LEVEL = 0x371ED4;        // u8 per reverb type (fxMulTable6)
constexpr uint32_t TYPE_TABLE_B = 0x371F0B, TYPE_TABLE_CD = 0x372007;   // C and D share one table
constexpr uint32_t OFFS_PRESET_1 = 0x36568C, OFFS_PRESET_2 = 0x3656A8, OFFS_PRESET_ROOM = 0x3656C4;
}  // namespace

// ---- small helpers (fx_design.c) ----
int32_t Host::fxBipolar500(int v) { return 500 * (v - 64); }                      // 0x39CF62
int32_t Host::fxBipolar376(int v) { return 376 * (v - 64); }                      // 0x39CF72
int32_t Host::fxDiffusionClamp(int x, int v)                                               // 0x39D468
{
    const int32_t d = x - int32_t(rom::u8(rom::FX_DIFFUSION + (uint32_t(v) & 0xFFFF)));
    return d > 0 ? d : 1;
}
int32_t Host::fxMsToSamples(int v) { return int32_t((19660u * (uint32_t(v) & 0xFFFF)) >> 12); }   // 0x39D47C
int32_t Host::fxMulShift16(int v) { return int16_t((31457u * (uint32_t(v) & 0xFFFF)) >> 16); }    // 0x39D490
int32_t Host::fxMulTable6(int x)                                                           // 0x39D4A2
{
    return int32_t(rom::u8(TYPE_LEVEL + ram.u8(FX_TYPE0)) * int16_t(x)) >> 6;
}
int32_t Host::fxMulTypeTableB(int x)                                                       // 0x39D500
{
    const uint32_t a = TYPE_TABLE_B + ((uint32_t(ram.s8(FX_TYPE1 + 2)) * 18) & 0xFF) + ram.u8(FX_TYPE1 + 3);
    return int32_t(rom::u8(a) * int16_t(x)) >> 6;
}
int32_t Host::fxMulTypeTableC(int x)                                                       // 0x39D53E
{
    const uint32_t a = TYPE_TABLE_CD + ((uint32_t(ram.s8(FX_TYPE1 + 4)) & 0xFFFF) * 14 & 0xFF) + ram.u8(FX_TYPE1 + 5);
    return int32_t(rom::u8(a) * int16_t(x)) >> 6;
}
int32_t Host::fxMulTypeTableD(int x)                                                       // 0x39D598
{
    const uint32_t a = TYPE_TABLE_CD + ((uint32_t(ram.s8(FX_TYPE1 + 6)) & 0xFFFF) * 14 & 0xFF) + ram.u8(FX_TYPE1 + 7);
    return int32_t(rom::u8(a) * int16_t(x)) >> 6;
}

// 0x39C796 fxDesignCombs(time, damp, count, sel, list): the damping LPF at list[r10..], then
// `count` comb units (g, x, x, a) from the RT60 gain table rows
void Host::fxDesignCombs(int time, int damp, int count, int sel, uint32_t list)
{
    count &= 0xFF;
    const uint32_t r10 = count == 6 ? 21 : 5;
    fxDesignFilter(1, 1, 0, rom::s8(rom::FX_DAMP_FREQ + (uint32_t(damp) & 0xFFFF)), 64, 0, list + 2 * r10);
    const uint32_t row = rom::FX_TIME_ROW + (uint32_t(time) & 0xFFFF);
    const uint32_t classes = rom::FX_COMB_CLASS + (uint32_t(sel) & 0xFFFF) * 6;
    for (int i = 0; i < count; i++) {
        const uint32_t cls = rom::u8(classes + uint32_t(i));
        uint32_t g = 0, loss = 0;
        if (cls != 0) {
            int32_t v = int16_t(rom::u8(row) + cls - 56);
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            g = rom::u16(rom::FX_COMB_GAIN + 4u * uint32_t(v) + 2);
            loss = rom::u16(rom::FX_COMB_GAIN + 4u * uint32_t(v));
        }
        const uint32_t unit = list + 8u * uint32_t(i);
        set_coef(rom::u16(unit), g);
        const float f = sh::fmul(sh::itof(int16_t(coef(rom::u16(list + 2 * r10)))), sh::utof(loss));
        const int32_t x = sh::ftoi(sh::fdiv(32768.0f, f));
        set_coef(rom::u16(unit + 2), uint32_t(int16_t(x)));
        set_coef(rom::u16(unit + 4), uint32_t(int16_t(x)));
        set_coef(rom::u16(unit + 6), coef(rom::u16(list + 2 * r10 + 4)));
    }
}

// 0x39C8F4 fxAddScaledPairs(clampSrc, count, sel, ids): delay offsets of `count` (base, a, b, c)
// slot quads from a scaled table row
void Host::fxAddScaledPairs(int clampSrc, int count, int sel, uint32_t ids)
{
    const uint32_t tbl = SCALED_PAIRS + (uint32_t(sel) & 0xFFFF) * 0x18;
    for (int i = 0; i < (count & 0xFF); i++) {
        const uint32_t a = uint32_t(fxMulShift16(rom::s16(tbl + 4u * uint32_t(i)))) & 0xFFFF;
        const uint32_t b = uint32_t(fxMulShift16(rom::s16(tbl + 4u * uint32_t(i) + 2))) & 0xFFFF;
        const uint32_t q = ids + 4u * uint32_t(i);
        const int32_t base = offs(rom::u8(q));
        set_offs(rom::u8(q + 1), int32_t(b + uint32_t(base)));
        if ((i & 1) == 0) {
            set_offs(rom::u8(q + 2), int32_t(a + uint32_t(base)));
            set_offs(rom::u8(q + 3), base + fxDiffusionClamp(int(a), clampSrc));
        } else {
            set_offs(rom::u8(q + 2), fxDiffusionClamp(int(a), clampSrc) + base);
            set_offs(rom::u8(q + 3), int32_t(a + uint32_t(base)));
        }
    }
}

// 0x39C9E8 fxScaleRow(src, scale, offset, count, stride, steps, slots): per unit, a delay
// offset from a scaled table word and four constants (allpass/feedback words)
void Host::fxScaleRow(int src, int scale, int offset, int count, int stride, uint32_t steps, uint32_t slots)
{
    count &= 0xFF;
    const uint32_t src2 = (uint32_t(src) & 0xFFFF) * 2;
    uint32_t row = ROW_TABLE + (uint32_t(offset) & 0xFFFF) * 0x18;
    uint32_t flags = 0;
    if (count == 6)
        flags = rom::u16(ROW_FLAGS_A + src2) & 0xFF;
    else if (count == 2)
        flags = rom::u16(ROW_FLAGS_2 + src2) & 0xFF;
    else if (count == 4) {
        flags = rom::u16(ROW_FLAGS_4 + src2) & 0xFF;
        row = ROW_TABLE_4 + (uint32_t(offset) & 0xFFFF) * 0x10;
    }
    uint32_t str = uint32_t(stride) & 0xFF;
    const int16_t scale16 = int16_t(scale);
    for (int i = 0; i < count; i++) {
        int32_t w;
        if ((i == 4 || i == 5) && scale16 == 0 && count == 6)
            w = fxMulShift16(rom::s16(row - 8));
        else if (i == 3 && scale16 == 0 && count == 4)
            w = fxMulShift16(rom::s16(row - 4));
        else
            w = fxMulShift16(rom::s16(row));
        uint32_t uw = uint32_t(w) & 0xFFFF;
        if (uw == 0)
            uw = 1;
        set_offs(rom::u8(slots + 2u * uint32_t(i) + 1), int32_t(uw + uint32_t(offs(rom::u8(slots + 2u * uint32_t(i))))));
        const int16_t x = int16_t(rom::s16(row + 2));
        row += 4;
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
        const uint32_t unit = steps + 8u * uint32_t(i);
        set_coef(rom::u16(unit), uint16_t(s28));
        set_coef(rom::u16(unit + 2), uint16_t(s24));
        set_coef(rom::u16(unit + 4), uint16_t(s26));
        set_coef(rom::u16(unit + 6), uint16_t(s22));
    }
}

// 0x39CC78 fxWriteSpan4(indexA, indexB, offset, row, words, ids): four early-reflection
// taps (two offsets and two words each) plus the pre-delay pair, clamped to the area
void Host::fxWriteSpan4(int indexA, int indexB, int offset, int row, uint32_t words, uint32_t ids)
{
    uint32_t rowp = SPAN_ROWS + (uint32_t(row) & 0xFFFF) * 0x24;
    const uint32_t t = uint32_t(fxMsToSamples(rom::s16(rom::FX_DELAY_WORD + 2u * (uint32_t(indexA) & 0xFFFF))));
    const uint32_t limit = ram.u32(FX_BASES + 4) - 1;
    const uint32_t diff = rom::u8(rom::FX_DIFFUSION + (uint32_t(offset) & 0xFFFF));
    const uint32_t ref0 = rom::u8(ER_BASE_SLOTS), ref1 = rom::u8(ER_BASE_SLOTS + 1);
    for (int i = 0; i < 4; i++) {
        const uint32_t w = uint32_t(fxMulShift16(rom::u16(rowp + 2))) & 0xFFFF;
        const uint32_t d = t + w;
        if (uint32_t(offs(ref0)) + d + diff < limit) {
            set_coef(rom::u16(words + 2u * uint32_t(i)), rom::u16(rowp + 0xC));
            set_coef(rom::u16(words + 2u * uint32_t(i) + 10), rom::u16(rowp + 0x18));
        } else {
            set_coef(rom::u16(words + 2u * uint32_t(i)), 0);
            set_coef(rom::u16(words + 2u * uint32_t(i) + 10), 0);
        }
        const uint32_t s0 = rom::u8(ids + 2u * uint32_t(i)), s1 = rom::u8(ids + 2u * uint32_t(i) + 1);
        if ((i & 1) == 0) {
            set_offs(s0, int32_t(uint32_t(offs(ref0)) + d));
            set_offs(s1, int32_t(uint32_t(offs(ref1)) + diff + d));
        } else {
            set_offs(s0, int32_t(uint32_t(offs(ref0)) + diff + d));
            set_offs(s1, int32_t(uint32_t(offs(ref1)) + d));
        }
        rowp += 2;
    }
    const uint32_t tt = uint32_t(fxMsToSamples(rom::s16(rom::FX_DELAY_WORD + 2u * (uint32_t(indexA) & 0xFFFF)))) +
                        uint32_t(fxMsToSamples(rom::s16(rom::FX_DELAY_WORD + 2u * (uint32_t(indexB) & 0xFFFF))));
    const uint32_t s10 = rom::u8(ER_BASE_SLOTS + 10), s11 = rom::u8(ER_BASE_SLOTS + 11);
    set_offs(s10, int32_t(uint32_t(offs(ref0)) + tt));
    set_offs(s11, int32_t(tt + uint32_t(offs(ref1))));
}

// 0x39CEC8 fxCopyScaled12(balance, list): the first ten early-reflection words scaled in
// place by the ER/Rev law (the source list is hard-wired to ER_WORDS), then twelve words of
// the complementary gain
void Host::fxCopyScaled12(int balance, uint32_t list)
{
    const uint32_t idx = uint32_t(balance) & 0xFFFF;
    for (int i = 0; i < 10; i++) {
        const float f1 = sh::itof(int16_t(coef(rom::u16(ER_WORDS + 2u * uint32_t(i)))));
        const float f2 = sh::itof(rom::s16(rom::FX_ER_REV + 2u * ((128u - idx) & 0xFFFFFFFFu)));
        const int32_t x = sh::ftoi(sh::fdiv(32768.0f, sh::fmul(f2, f1)));
        set_coef(rom::u16(list + 2u * uint32_t(i)), uint32_t(int16_t(x) >> 1));
    }
    const uint32_t g = rom::u16(rom::FX_ER_REV + 2u * idx);
    for (int i = 10; i < 22; i++)
        set_coef(rom::u16(list + 2u * uint32_t(i)), g);
}

// 0x39D2C4 fxRoomPairs(clampSrc, index, span, ids): early reflection delay offsets from room table C
void Host::fxRoomPairs(int clampSrc, int index, int span, uint32_t ids)
{
    auto pair = [&](uint32_t tp, uint32_t& a, uint32_t& b) {
        a = uint32_t(fxMulShift16(rom::s16(tp))) & 0xFFFF;
        b = uint32_t(fxMulShift16(int(rom::u16(tp) >> 1))) & 0xFFFF;
    };
    uint32_t a, b;
    const uint32_t tp = rom::FX_ROOM_C + (uint32_t(index) & 0xFFFF) * 2;
    pair(tp, a, b);
    int32_t base = offs(rom::u8(ids));
    set_offs(rom::u8(ids + 1), int32_t(a + uint32_t(base)));
    set_offs(rom::u8(ids + 2), int32_t(b + uint32_t(base)));
    set_offs(rom::u8(ids + 3), base + fxDiffusionClamp(int(b), clampSrc));
    const uint32_t tp2 = rom::FX_ROOM_C + ((uint32_t(index) & 0xFFFF) + (uint32_t(span) & 0xFFFF)) * 2;
    pair(tp2, a, b);
    base = offs(rom::u8(ids + 4));
    set_offs(rom::u8(ids + 5), int32_t(a + uint32_t(base)));
    set_offs(rom::u8(ids + 6), fxDiffusionClamp(int(b), clampSrc) + base);
    set_offs(rom::u8(ids + 7), int32_t(b + uint32_t(base)));
}

// 0x39CF82 fxDesignRooms(time, y0, y1, damp, extra, list): the room's early reflection units:
// the damping LPF, then two comb-like units from the RT60 gain row of the time nearest to
// extra * RT60[time] / 10
void Host::fxDesignRooms(int time, int y0, int y1, int damp, int extra, uint32_t list)
{
    const float target = sh::dtof(sh::ddiv(sh::ftod(sh::fmul(sh::utof(uint32_t(extra) & 0xFFFF),
                                                             rom::f32(rom::FX_RT60_SECONDS + 4u * (uint32_t(time) & 0xFFFF)))), 10.0));
    int32_t k = 0;
    while (sh::fgt(target, rom::f32(rom::FX_RT60_SECONDS + 4u * uint32_t(k) + 4)))
        k++;
    const float mid = sh::fadd(rom::f32(rom::FX_RT60_SECONDS + 4u * uint32_t(k) + 4), rom::f32(rom::FX_RT60_SECONDS + 4u * uint32_t(k)));
    if (sh::fgt(0.0f, sh::fsub(sh::fmul(2.0f, target), mid)))
        k++;
    const uint32_t rowp = rom::FX_TIME_ROW + uint32_t(int16_t(k));
    const int32_t dampf = rom::s8(rom::FX_DAMP_FREQ + (uint32_t(damp) & 0xFFFF));
    const uint32_t ya = rom::FX_ROOM_A + (uint32_t(y0) & 0xFFFF);
    const uint32_t yb = rom::FX_ROOM_A + (uint32_t(y0) & 0xFFFF) + (uint32_t(y1) & 0xFFFF);
    auto unit = [&](uint32_t u, uint32_t src, uint32_t lpf) {
        int32_t v = int16_t(rom::u8(src) + rom::u8(rowp) - 56);
        v = v < 0 ? 0 : v > 255 ? 255 : v;
        const uint32_t entry = rom::FX_COMB_GAIN + 4u * uint32_t(v);
        set_coef(rom::u16(u), rom::u16(entry + 2));
        const float f = sh::fmul(sh::itof(int16_t(coef(rom::u16(lpf)))), sh::utof(rom::u16(entry)));
        const int32_t x = sh::ftoi(sh::fdiv(32768.0f, f));
        set_coef(rom::u16(u + 2), uint32_t(int16_t(x)));
        set_coef(rom::u16(u + 4), uint32_t(int16_t(x)));
        set_coef(rom::u16(u + 6), coef(rom::u16(lpf + 4)));
    };
    fxDesignFilter(1, 1, 0, dampf, 64, 0, list + 10);
    unit(list, ya, list + 10);
    unit(list + 8, yb, list + 10);
}

// ---- the handlers ----
namespace {
// the three (row, class, offset) words of the reverb type's comb selector
inline uint32_t sel_row(const Host& h) { return rom::FX_COMB_SELECT + uint32_t(int8_t(6 * h.ram.s8(FX_TYPE0))); }
}  // namespace

// 0x3A3728 fxRevHall: reverb types 1-8
void Host::fxRevHall(int p)
{
    p &= 0xFF;
    const uint32_t sel = sel_row(*this);
    auto prm = [&](int i) -> int32_t { return ram.s16(FX_PARAMS + 2 * i); };
    switch (p) {
    case 0: case 13:
        fxDesignCombs(prm(0), prm(13), 6, rom::s16(sel + 2), rom::FX_COMB_STEPS);
        writeSweWordListSub(0x18, rom::FX_COMB_STEPS);
        break;
    case 1: {
        fxAddScaledPairs(prm(1), 6, rom::s16(sel + 2), COMB_SLOTS);
        uint8_t ids[12];
        for (int i = 0; i < 6; i++) {
            ids[2 * i] = uint8_t(rom::u8(COMB_SLOTS + 4u * uint32_t(i) + 2));
            ids[2 * i + 1] = uint8_t(rom::u8(COMB_SLOTS + 4u * uint32_t(i) + 3));
        }
        writeSweDeSubIds(12, ids);
        fxWriteSpan4(prm(2), prm(10), prm(1), rom::s16(sel), ER_WORDS, ER_SLOTS);
        fxCopyScaled12(prm(12), ER_WORDS);
        writeSweDeSub(10, ER_SLOTS);
        fxScaleRow(prm(11), prm(1), rom::s16(sel + 4), 6, 20, ROW_STEPS, ROW_SLOTS);
        writeSweDeSub(1, ROW_SLOTS + 9);
        writeSweDeSub(1, ROW_SLOTS + 11);
        break;
    }
    case 2: case 10:
        fxWriteSpan4(prm(2), prm(10), prm(1), rom::s16(sel), ER_WORDS, ER_SLOTS);
        fxCopyScaled12(prm(12), ER_WORDS);
        writeSweWordListSub(10, ER_WORDS);
        writeSweDeSub(10, ER_SLOTS);
        break;
    case 3:
        fxDesignFilter(1, 0, 1, prm(3), 64, 0, HPF_STEPS);
        fxCopyWords3(HPF_STEPS, HPF_COPY);
        break;
    case 4:
        fxDesignFilter(1, 1, 0, prm(4), 64, 0, LPF_STEPS);
        fxCopyWords3(LPF_STEPS, LPF_COPY);
        break;
    case 11:
        fxScaleRow(prm(11), prm(1), rom::s16(sel + 4), 6, 20, ROW_STEPS, ROW_SLOTS);
        writeSweWordListSub(0x18, ROW_STEPS);
        break;
    case 12:
        fxWriteSpan4(prm(2), prm(10), prm(1), rom::s16(sel), ER_WORDS, ER_SLOTS);
        fxCopyScaled12(prm(12), ER_WORDS);
        writeSweWordListSub(10, ER_WORDS);
        writeSweWordListSub(0xC, ER_WORDS + 0x14);
        break;
    case 14:
        set_coef(rom::u16(WIDTH_STEPS), uint32_t(fxBipolar500(prm(14))));
        set_coef(rom::u16(WIDTH_STEPS + 2), uint32_t(fxBipolar500(prm(14))));
        writeSweWordListSub(2, WIDTH_STEPS);
        break;
    case 255: {
        ram.w8(SWE_SUB_HOLD, 1);
        const uint32_t idx = rom::u8(rom::FX_PRESET_INDEX_REVERB + ram.u8(FX_TYPE0));
        if (idx == 1 || idx == 2) {
            const uint32_t src = idx == 1 ? OFFS_PRESET_1 : OFFS_PRESET_2;
            for (uint32_t i = 0; i < 13; i++)
                set_offs(rom::u8(0x35DAE5 + i), int32_t(rom::u16(src + 2 * i)));
        }
        const uint32_t span = ram.u32(FX_BASES + 4) - ram.u32(FX_BASES + 0);
        set_offs(rom::u8(0x35DAE4), int32_t(((span - 28653u) >> 1) + 28652u));
        set_coef(rom::u16(MIX_STEPS), 0x4000);
        set_coef(rom::u16(MIX_STEPS + 2), 0x4000);
        set_coef(rom::u16(MIX_STEPS + 4), 0);
        set_coef(rom::u16(MIX_STEPS + 6), 8192);
        set_coef(rom::u16(MIX_STEPS + 8), 8192);
        for (int q : {0, 1, 2, 3, 4, 11, 12, 14})
            fxRevHall(q);
        fxDesignFromTable(ram.s8(FX_TYPE0), TABLE_STEPS);
        const uint32_t w = uint32_t(fxMulTable6(8192));
        set_coef(rom::u16(LEVEL_STEPS + 2), w);
        set_coef(rom::u16(LEVEL_STEPS), w);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
    }
}

// 0x3A3C54 fxRevRoom: reverb types 9-12
void Host::fxRevRoom(int p)
{
    p &= 0xFF;
    const uint32_t sel = sel_row(*this);
    auto prm = [&](int i) -> int32_t { return ram.s16(FX_PARAMS + 2 * i); };
    const uint32_t roomrow = rom::FX_ROOM_B + uint32_t(int8_t(12 * (ram.s8(FX_TYPE0) - 9)));
    auto pairs = [&](int j) {   // j = 0, 1, 2: the three early reflection groups
        fxRoomPairs(prm(1), prm(5 + j), prm(8), COMB_SLOTS + 8u * uint32_t(j));
    };
    auto rooms = [&](int j) {
        fxDesignRooms(prm(0), prm(5 + j), prm(8), prm(13), rom::s16(roomrow + 2u * uint32_t(j)), rom::FX_COMB_STEPS + 16u * uint32_t(j));
    };
    auto slots18 = [&]() {
        uint8_t ids[18];
        for (int i = 0; i < 6; i++)
            for (int k = 0; k < 3; k++)
                ids[3 * i + k] = uint8_t(rom::u8(COMB_SLOTS + 4u * uint32_t(i) + 1 + uint32_t(k)));
        writeSweDeSubIds(18, ids);
    };
    switch (p) {
    case 0: case 8: case 13:
        pairs(0); pairs(1); pairs(2);
        rooms(0); rooms(1); rooms(2);
        slots18();
        writeSweWordListSub(0x18, rom::FX_COMB_STEPS);
        break;
    case 1:
        pairs(0); pairs(1); pairs(2);
        rooms(0); rooms(1); rooms(2);
        slots18();
        writeSweWordListSub(0x18, rom::FX_COMB_STEPS);
        fxWriteSpan4(prm(2), prm(10), prm(1), rom::s16(sel), ER_WORDS, ER_SLOTS);
        fxCopyScaled12(prm(12), ER_WORDS);
        writeSweDeSub(10, ER_SLOTS);
        fxScaleRow(prm(11), prm(1), rom::s16(sel + 4), 6, 20, ROW_STEPS, ROW_SLOTS);
        writeSweDeSub(1, ROW_SLOTS + 9);
        writeSweDeSub(1, ROW_SLOTS + 11);
        break;
    case 2: case 10:
        fxWriteSpan4(prm(2), prm(10), prm(1), rom::s16(sel), ER_WORDS, ER_SLOTS);
        fxCopyScaled12(prm(12), ER_WORDS);
        writeSweWordListSub(10, ER_WORDS);
        writeSweDeSub(10, ER_SLOTS);
        break;
    case 3:
        fxDesignFilter(1, 0, 1, prm(3), 64, 0, HPF_STEPS);
        fxCopyWords3(HPF_STEPS, HPF_COPY);
        break;
    case 4:
        fxDesignFilter(1, 1, 0, prm(4), 64, 0, LPF_STEPS);
        fxCopyWords3(LPF_STEPS, LPF_COPY);
        break;
    case 5: case 6: case 7: {
        const int j = p - 5;
        pairs(j);
        rooms(j);
        writeSweDeSub(3, COMB_SLOTS + 8u * uint32_t(j) + 1);
        writeSweDeSub(3, COMB_SLOTS + 8u * uint32_t(j) + 5);
        writeSweWordListSub(8, rom::FX_COMB_STEPS + 16u * uint32_t(j));
        break;
    }
    case 11:
        fxScaleRow(prm(11), prm(1), rom::s16(sel + 4), 6, 20, ROW_STEPS, ROW_SLOTS);
        writeSweWordListSub(0x18, ROW_STEPS);
        break;
    case 12:
        fxWriteSpan4(prm(2), prm(10), prm(1), rom::s16(sel), ER_WORDS, ER_SLOTS);
        fxCopyScaled12(prm(12), ER_WORDS);
        writeSweWordListSub(10, ER_WORDS);
        writeSweWordListSub(0xC, ER_WORDS + 0x14);
        break;
    case 14:
        set_coef(rom::u16(WIDTH_STEPS), uint32_t(fxBipolar500(prm(14))));
        set_coef(rom::u16(WIDTH_STEPS + 2), uint32_t(fxBipolar500(prm(14))));
        writeSweWordListSub(2, WIDTH_STEPS);
        break;
    case 255: {
        ram.w8(SWE_SUB_HOLD, 1);
        for (uint32_t i = 0; i < 13; i++)
            set_offs(rom::u8(0x35DAE5 + i), int32_t(rom::u16(OFFS_PRESET_ROOM + 2 * i)));
        const uint32_t span = ram.u32(FX_BASES + 4) - ram.u32(FX_BASES + 0);
        set_offs(rom::u8(0x35DAE4), int32_t(((span - 28397u) >> 1) + 28652u));
        set_coef(rom::u16(MIX_STEPS), 0x4000);
        set_coef(rom::u16(MIX_STEPS + 2), 0x4000);
        set_coef(rom::u16(MIX_STEPS + 4), 0);
        set_coef(rom::u16(MIX_STEPS + 6), 8192);
        set_coef(rom::u16(MIX_STEPS + 8), 8192);
        for (int q : {0, 1, 2, 3, 4, 5, 6, 7, 11, 12, 14})
            fxRevRoom(q);
        fxDesignFromTable(ram.s8(FX_TYPE0), TABLE_STEPS);
        const uint32_t w = uint32_t(fxMulTable6(8192));
        set_coef(rom::u16(LEVEL_STEPS + 2), w);
        set_coef(rom::u16(LEVEL_STEPS), w);
        ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
    }
}

}  // namespace fs1r::vop3host
