// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The delay family: reverb types 13-16 and the variation types Delay L,C,R / Delay L,R /
// Echo / Cross Delay. Both are the same code over different step and slot lists, parameter
// blocks, type-scale tables and delay areas, so each handler is written once over a Block.
#include "host.h"

namespace fs1r::vop3host {

using namespace ram;

namespace {

constexpr uint32_t FX_LEVEL_CURVE = 0x3717D0;   // f32 x 128: feedback/level law 0..1

struct Block {
    uint32_t W;            // u16 x 14: the block's step words
    uint32_t S;            // u8 x 6: its delay slots
    uint32_t lpf, lpf_copy, hpf, hpf_copy, curve, curve_copy;
    uint32_t params;       // the kind's parameter block
    bool variation;        // fxMulTypeTableD and the memory-flag area top instead of table C and area 3..4
};

constexpr Block REVERB_BLOCK{0x35D8C8, 0x35D8E4, 0x35E69C, 0x35E6A8, 0x35E6A2, 0x35E6AE, 0x35E6B4, 0x35E6B8, FX_PARAMS + 0x60, false};
constexpr Block VARIATION_BLOCK{0x35D92C, 0x35D948, 0x35E776, 0x35E782, 0x35E77C, 0x35E788, 0x35E78E, 0x35E792, FX_PARAMS + 0x80, true};

struct Delay {
    Host& h;
    const Block& b;
    int32_t P(int i) const { return h.ram.s16(b.params + 2 * i); }
    uint32_t slot(int i) const { return rom::u8(b.S + uint32_t(i)); }
    uint32_t word(int i) const { return rom::u16(b.W + 2u * uint32_t(i)); }

    // offs[S[dst]] = samples(p) + offs[S[ref]] (unsigned 16-bit sample count), then the slot
    void delay_slot(int dst, int ref, int p) const
    {
        h.set_offs(slot(dst), int32_t((uint32_t(h.fxMsToSamples(P(p))) & 0xFFFF) + uint32_t(h.offs(slot(ref)))));
        h.writeSweDeSub(1, b.S + uint32_t(dst));
    }
    // the same with a 16-bit base, selected by the routing mode (cross delay)
    void delay_slot16(int dst, uint32_t refslot, int p) const
    {
        h.set_offs(slot(dst), int32_t((uint32_t(h.fxMsToSamples(P(p))) & 0xFFFF) + (uint32_t(h.offs(refslot)) & 0xFFFF)));
        h.writeSweDeSub(1, b.S + uint32_t(dst));
    }
    // the feedback gain words (parameter 9 of every type)
    void gain_words() const
    {
        const uint32_t g = uint32_t(rom::s16(rom::FX_ER_REV + 2) >> 2);
        h.set_coef(word(1), g);
        h.set_coef(word(0), g);
        const int32_t scaled = rom::s16(rom::FX_ER_REV + 254) >> 2;
        const uint32_t x = uint32_t(b.variation ? h.fxMulTypeTableD(scaled) : h.fxMulTypeTableC(scaled));
        h.set_coef(word(3), x);
        h.set_coef(word(2), x);
        h.writeSweWordListSub(4, b.W);
    }
    void filters(int p) const
    {
        if (p == 12 || p == 13) {
            h.fxDesignFilter(1, 1, 2, P(12), P(13), 0, b.lpf);
            h.fxCopyWords3(b.lpf, b.lpf_copy);
        } else {
            h.fxDesignFilter(1, 2, 3, P(14), P(15), 0, b.hpf);
            h.fxCopyWords3(b.hpf, b.hpf_copy);
        }
    }
    void curve(int p, bool copy) const
    {
        h.fxFloatCurveB(0, P(p), 64, b.curve);
        if (copy)
            h.fxCopyWords2(b.curve, b.curve_copy);
    }
    // half the block's delay area plus one: reverb uses area 3..4, variation the top of the
    // half of the block's delay memory; the variation's runs up to 0x40000
    uint32_t half_area() const
    {
        if (!b.variation)
            return ((h.ram.u32(FX_BASES + 16) - h.ram.u32(FX_BASES + 12)) >> 1) + 1;
        return ((0x40000u - h.ram.u32(FX_BASES + 16)) >> 1) + 1;
    }
    void bipolar_word(int wi, int p) const
    {
        h.set_coef(word(wi), uint32_t(h.fxBipolar500(P(p))));
        h.writeSweWordListSub(1, b.W + 2u * uint32_t(wi));
    }
};

void delay_lcr(Host& h, const Block& b, int p, void (Host::*self)(int))
{
    const Delay d{h, b};
    switch (p) {
    case 0: d.delay_slot(2, 0, 0); break;
    case 1: d.delay_slot(4, 0, 1); break;
    case 2: d.delay_slot(5, 0, 2); break;
    case 3: d.delay_slot(1, 0, 3); break;
    case 4: d.bipolar_word(7, 4); break;
    case 5: {
        const int32_t x = sh::dtoi(sh::dmul(sh::ftod(rom::f32(FX_LEVEL_CURVE + 4u * (uint32_t(d.P(5)) & 0xFFFF))), 32767.0));
        h.set_coef(d.word(6), uint32_t(sh::s16(uint32_t(x))));
        h.set_coef(d.word(5), uint32_t(sh::s16(uint32_t(x))));
        h.writeSweWordListSub(1, b.W + 10);
        h.writeSweWordListSub(1, b.W + 12);
        break;
    }
    case 6: d.curve(6, false); break;
    case 9: d.gain_words(); break;
    case 12: case 13: case 14: case 15: d.filters(p); break;
    case 255:
        h.ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 4, 5, 6, 9, 12, 14})
            (h.*self)(q);
        h.ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

void delay_lr(Host& h, const Block& b, int p, void (Host::*self)(int))
{
    const Delay d{h, b};
    switch (p) {
    case 0: d.delay_slot(2, 0, 0); break;
    case 1: d.delay_slot(5, 0, 1); break;
    case 2: d.delay_slot(1, 0, 2); break;
    case 3: d.delay_slot(4, 0, 3); break;
    case 4: {
        const int32_t x = h.fxBipolar500(d.P(4));
        h.set_coef(d.word(9), uint32_t(sh::s16(uint32_t(x)) >> 1));
        h.set_coef(d.word(7), uint32_t(sh::s16(uint32_t(x)) >> 1));
        h.writeSweWordListSub(1, b.W + 14);
        h.writeSweWordListSub(1, b.W + 18);
        break;
    }
    case 5: d.curve(5, true); break;
    case 9: d.gain_words(); break;
    case 12: case 13: case 14: case 15: d.filters(p); break;
    case 255:
        h.ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 4, 5, 9, 12, 14})
            (h.*self)(q);
        h.ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

void echo(Host& h, const Block& b, int p, void (Host::*self)(int))
{
    const Delay d{h, b};
    switch (p) {
    case 0: d.delay_slot(1, 0, 0); break;
    case 1: d.bipolar_word(7, 1); break;
    case 2: d.delay_slot(4, 3, 2); break;
    case 3: d.bipolar_word(8, 3); break;
    case 4: d.curve(4, true); break;
    case 5: d.delay_slot(2, 0, 5); break;
    case 6: d.delay_slot(5, 3, 6); break;
    case 7: {
        const int32_t x = sh::ftoi(sh::fmul(32767.0f, rom::f32(FX_LEVEL_CURVE + 4u * (uint32_t(d.P(7)) & 0xFFFF))));
        h.set_coef(d.word(5), uint32_t(x));
        h.set_coef(d.word(4), uint32_t(x));
        h.writeSweWordListSub(2, b.W + 8);
        break;
    }
    case 9: d.gain_words(); break;
    case 12: case 13: case 14: case 15: d.filters(p); break;
    case 255:
        h.set_offs(d.slot(3), int32_t(d.half_area()));
        h.ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {0, 1, 2, 3, 4, 5, 6, 7, 9, 12, 14})
            (h.*self)(q);
        h.ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

// the cross delay's routing mode is parameter 3; the reverb version references slots 5/0,
// the variation version slots 3/0
void cross_delay(Host& h, const Block& b, int p, void (Host::*self)(int))
{
    const Delay d{h, b};
    const int32_t mode = d.P(3);
    const int alt = 3;
    switch (p) {
    case 0: d.delay_slot16(4, mode == 1 ? d.slot(alt) : d.slot(0), 0); break;
    case 1: d.delay_slot16(1, mode == 1 ? d.slot(0) : d.slot(alt), 1); break;
    case 2:
        if (mode == 1) {
            h.set_coef(d.word(8), 0);
            h.set_coef(d.word(7), 0);
            const uint32_t x = uint32_t(h.fxBipolar500(d.P(2)));
            h.set_coef(d.word(10), x);
            h.set_coef(d.word(9), x);
            h.writeSweWordListSub(4, b.W + 14);
        } else {
            const uint32_t x = uint32_t(h.fxBipolar500(d.P(2)));
            h.set_coef(d.word(8), x);
            h.set_coef(d.word(7), x);
            h.set_coef(d.word(10), 0);
            h.set_coef(d.word(9), 0);
            h.writeSweWordListSub(2, b.W + 18);
            h.writeSweWordListSub(2, b.W + 14);
        }
        break;
    case 3: {
        const uint32_t half = d.half_area();
        if (mode == 0) {
            h.set_coef(d.word(11), 0x4000); h.set_coef(d.word(12), 0x4000); h.set_coef(d.word(13), 0);
            h.set_offs(d.slot(0), 1);
            h.set_offs(d.slot(3), int32_t(half));
        } else if (mode == 1) {
            h.set_coef(d.word(11), 0x4000); h.set_coef(d.word(12), 0x4000); h.set_coef(d.word(13), 0);
            h.set_offs(d.slot(0), int32_t(half));
            h.set_offs(d.slot(3), 1);
        } else {
            h.set_coef(d.word(11), 0x7FFF); h.set_coef(d.word(12), 0); h.set_coef(d.word(13), 0x7FFF);
            h.set_offs(d.slot(0), 1);
            h.set_offs(d.slot(3), int32_t(half));
        }
        h.writeSweWordListSub(3, b.W + 22);
        h.writeSweDeSub(1, b.S);
        h.writeSweDeSub(1, b.S + 3);
        (h.*self)(2);
        break;
    }
    case 4: d.curve(4, true); break;
    case 9: d.gain_words(); break;
    case 12: case 13: case 14: case 15: d.filters(p); break;
    case 255:
        h.ram.w8(SWE_SUB_HOLD, 1);
        for (int q : {3, 0, 1, 4, 9, 12, 14})
            (h.*self)(q);
        h.ram.w8(SWE_SUB_HOLD, 0);
        break;
    }
}

}  // namespace

void Host::fxRevDelayLCR(int p) { delay_lcr(*this, REVERB_BLOCK, p & 0xFF, &Host::fxRevDelayLCR); }      // 0x39D5E0
void Host::fxRevDelayLR(int p) { delay_lr(*this, REVERB_BLOCK, p & 0xFF, &Host::fxRevDelayLR); }         // 0x39D964
void Host::fxRevEcho(int p) { echo(*this, REVERB_BLOCK, p & 0xFF, &Host::fxRevEcho); }                   // 0x39DC98
void Host::fxRevCrossDelay(int p) { cross_delay(*this, REVERB_BLOCK, p & 0xFF, &Host::fxRevCrossDelay); } // 0x39E074
void Host::fxVarDelayLCR(int p) { delay_lcr(*this, VARIATION_BLOCK, p & 0xFF, &Host::fxVarDelayLCR); }   // 0x39E52C
void Host::fxVarDelayLR(int p) { delay_lr(*this, VARIATION_BLOCK, p & 0xFF, &Host::fxVarDelayLR); }      // 0x39E8B0
void Host::fxVarEcho(int p) { echo(*this, VARIATION_BLOCK, p & 0xFF, &Host::fxVarEcho); }                // 0x39EBE4
void Host::fxVarCrossDelay(int p) { cross_delay(*this, VARIATION_BLOCK, p & 0xFF, &Host::fxVarCrossDelay); } // 0x39EFD8

}  // namespace fs1r::vop3host
