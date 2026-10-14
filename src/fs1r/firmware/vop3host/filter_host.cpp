// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The filter chip's host. Slot s has a page of the program, B = 16 + 124 * (((s >> 2) + 1) & 3), and a
// lane in it, k = s & 3. Where its parameters land:
//   type         program step B+120+k: the register it reads, which is the filter's tap, and its shift
//   cutoff       constant B+3k+2: 3085 + 169 * cutoff + key scaling, up to 24576; 169 is a semitone
//   resonance    constants B+32+k and B+54+4k
//   input gain   constant B+52+4k
//   EG depth     constant B+3k: 288 * depth
//   LFO2         registers 0x24..0x27 of the slot's channel, and its depth in constant B+3k+1
//   output ramp  constant B+116+k
// The EG runs in the chip, a segment at a time: register 0x2B is the rate, 0x2D the level it heads for and
// 0x2C the level at which it reports. A level byte L is (L - 50) * 256 / 50. The aim lies half the swing
// past the target, so the curve arrives with a slope.
#include "filter_host.h"
#include <algorithm>
#include <initializer_list>

#include "rom_tables.h"

namespace fs1r::vop3host {

namespace {

namespace flt = rom::flt;
using Host = FilterHost;

const uint8_t NONE[112] = {};                               // what a free slot's part reads as
constexpr int RUN = 682, IDLE = -379, MUTE = -1137;         // the output ramp's step a sample: a fade in and two fades out
constexpr int HOLD = 255;                                   // the EG rate that holds
constexpr int GLIDE = 2, JUMP = 14;                         // a constant's tag: glide to a new value, or jump

int page(int s) { return 16 + 124 * (((s >> 2) + 1) & 3); }
int lane(int s) { return s & 3; }
int channel(int s) { return (s + 2) & 15; }
const uint8_t* voiceOf(const Host& h, int p) { return p < 4 ? h.voice[p] : NONE; }
const uint8_t* partOf(const Host& h, int p) { return p < 4 ? h.part[p] : NONE; }

// The firmware's clamp: through sixteen bits first.
int clamp16(int v, int lo, int hi)
{
    v = int16_t(v);
    return v <= lo ? lo : v >= hi ? hi : v;
}
int level(int byte) { return (byte - 50) * 256 / 50; }
int keyScaled(int time, int note, int scale) { return scale * time * (note - 60) / 1407; }

// ---- registers ----
void reg(Host& h, int r, int v) { h.sink->write(uint32_t(r), uint32_t(v) & 0xFFFF); }
void config(Host& h, int index, int v) { reg(h, 0, index + 1); reg(h, 0x16, v); }
void egRate(Host& h, int ch, int v) { reg(h, 0, ch); reg(h, 0x2B, v & 0xFF); }
void egTarget(Host& h, int ch, int v) { reg(h, 0, ch); reg(h, 0x2C, v); }
void egAim(Host& h, int ch, int v) { reg(h, 0, ch); reg(h, 0x2D, v); }
void egInterrupts(Host& h, bool on) { reg(h, 0x21, on ? 0xFFFF : 0); }
void writeLfo(Host& h, int r, int ch) { reg(h, 0, ch); reg(h, r, h.lfoReg[r - 0x24][ch]); }
void writeProgram(Host& h, int step)
{
    reg(h, 0, step);
    for (int w = 0; w < 5; w++)
        reg(h, 10 - w, h.prog[step][w]);
}
void setCoef(Host& h, int step, int v)
{
    h.coef[step] = uint16_t(v);
    reg(h, 0, step);
    reg(h, 0x0B, h.coef[step]);
}
void setTag(Host& h, int step, int v)
{
    h.tag[step] = uint8_t(v);
    reg(h, 0, step);
    reg(h, 0x0C, v);
}

// ---- a slot's parameters ----

// the input gain by the voice's gain byte, or silence. The second serial bank's only ever gets silence.
void setGain(Host& h, int s, bool second, int v) { setCoef(h, page(s) + 52 + 4 * lane(s) + second, v < 0 ? 0 : flt::GAIN[v]); }
void setRamp(Host& h, int s, int step) { setCoef(h, page(s) + 116 + lane(s), step); }
void setEgDepth(Host& h, int s, int depth) { setCoef(h, page(s) + 3 * lane(s), 288 * depth); }

// the tag of the slot's EG depth, LFO2 depth, cutoff and first resonance constants. A jump also zeroes them.
void slotTags(Host& h, int s, int tag)
{
    const int B = page(s), k = lane(s);
    for (int step : {B + 3 * k, B + 3 * k + 1, B + 3 * k + 2, B + 32 + k}) {
        setTag(h, step, tag);
        if (tag == JUMP)
            setCoef(h, step, 0);
    }
}
void gainTags(Host& h, int s, int tag)
{
    setTag(h, page(s) + 52 + 4 * lane(s), tag);
    setTag(h, page(s) + 53 + 4 * lane(s), tag);
}

// the slot's output step reads the tap of the type and shifts it. LPF24, LPF18, LPF12, HPF, BPF, BEF.
void setType(Host& h, int s, int type)
{
    const int r = 6 * ((s + 4) & 15), k = lane(s), step = page(s) + 120 + k;
    int tap = s, shift = type;                              // what the firmware is left with past the last type
    switch (type) {
    case 0: tap = r + 5; shift = 1; break;
    case 1: tap = r + 4; shift = 1; break;
    case 2: tap = r + 3; shift = 1; break;
    case 3: tap = 99 + 3 * k; shift = 1; break;
    case 4: tap = r + 2; shift = 0; break;
    case 5: tap = 98 + 3 * k; shift = 1; break;
    }
    h.type[s] = uint8_t(type);
    h.prog[step][4] = uint16_t(tap << 7 | (h.prog[step][4] & 0xC07F));
    h.prog[step][3] = uint16_t(shift << 12 | (h.prog[step][3] & 0xCFFF));
    writeProgram(h, step);
    setCoef(h, page(s) + 28 + k, type <= 5 ? 0 : type);
}

// the cutoff word from a cutoff byte and the slot's key scaling
void setCutoff(Host& h, int s, int cutoff)
{
    const int keys = int8_t(int8_t(h.note[s]) - h.ksPoint[s]);
    const int v = 3085 + 169 * cutoff + keys * (h.ksDepth[s] * 2048 / 384);
    setCoef(h, page(s) + 3 * lane(s) + 2, std::clamp(v, 3085, 24576));
}
// both resonance constants. The highpass and the notch leave the second at 0.
void setResonance(Host& h, int s, int r)
{
    setCoef(h, page(s) + 32 + lane(s), flt::RESONANCE_A[r]);
    setCoef(h, page(s) + 54 + 4 * lane(s), h.type[s] == 3 || h.type[s] == 5 ? 0 : flt::RESONANCE_B[r]);
}

// The cutoff byte of a slot: the voice's and the part's, and the slot's modulation at the voice's depth.
int cutoffByte(const Host& h, int s, const uint8_t* V, const uint8_t* R, int p)
{
    const int depth = clamp16(V[0x59] + 99 * (p < 4 ? h.partCutoffMod[p] : 0) / 127, 0, 99);
    return clamp16(V[0x57] + R[0x18] - 64 + depth * h.slotMod[s] * 12288 / 2127609, 0, 127);
}
// The resonance byte, with its velocity sensitivity less 7.
int resonance(const Host& h, int s, const uint8_t* V, const uint8_t* R, int sens)
{
    const int v = h.vel[s];
    return clamp16(V[0x55] + 2 * (R[0x19] - 64) + sens * (116 * (sens > 0 ? v - 127 : v) / 889), 0, 116);
}
int egDepth(const Host& h, int s, const uint8_t* V, const uint8_t* R)
{
    const int sens = V[0x58] - 7, base = R[0x1F] + V[0x64] - 128, v = h.vel[s];
    return clamp16(base + sens * base * (sens > 0 ? v - 127 : v) / 889, -64, 64);
}

// ---- LFO2 ----
void lfoWave(Host& h, int ch, int wave)
{
    const int bits = wave <= 5 ? wave << 5 : wave == 6 ? 224 : wave;    // past 6 the firmware ORs the byte in as it came
    h.lfoReg[1][ch] = uint16_t((h.lfoReg[1][ch] & 0xFF1F) | bits);
    writeLfo(h, 0x25, ch);
}
void lfoSpeed(Host& h, int ch, int v)
{
    h.lfoReg[0][ch] = flt::LFO2_SPEED[v] | (h.lfoReg[0][ch] & 0x0C00);
    writeLfo(h, 0x24, ch);
}
void lfoPhase(Host& h, int ch, int v)
{
    h.lfoReg[1][ch] = (flt::LFO2_PHASE[v] & 0x1F) | (h.lfoReg[1][ch] & 0xFFE0);
    writeLfo(h, 0x25, ch);
    reg(h, 0x20, 1 << ch);                                  // back to that phase
    reg(h, 0x1C, 0);
}
// the two sawtooths run at full amount with an offset word, the others at half; then the slot's depth
void lfoDepth(Host& h, int ch, int wave, int depth)
{
    h.lfoReg[2][ch] = wave == 1 || wave == 2 ? 0x7FFF : (64 << 15) / 127;
    writeLfo(h, 0x26, ch);
    h.lfoReg[3][ch] = wave == 1 ? 0x4000 : wave == 2 ? 0xC000 : 0;
    writeLfo(h, 0x27, ch);
    const int s = (ch + 14) & 15;
    setCoef(h, page(s) + 3 * lane(s) + 1, 124 * depth);
}
void setLfo(Host& h, int s, const uint8_t* V, const uint8_t* R, int p)
{
    const int ch = channel(s), wave = V[0x18];
    lfoWave(h, ch, wave);
    lfoSpeed(h, ch, clamp16(V[0x19] + R[0x2E] + h.partLfo2Speed[p] - 64, 0, 127));
    lfoDepth(h, ch, wave, clamp16(V[0x5A] + R[0x2F] + 99 * h.partLfo2Depth[p] / 127 - 64, 0, 99));
}

}  // namespace

// the program, tags and constants, then every slot muted with its EG held
void FilterHost::boot()
{
    Host& h = *this;
    reg(h, 1, 1);
    reg(h, 5, 0x1004);
    for (int i = 0; i < 15; i++)
        config(h, i, flt::CFG16[i]);
    for (int r : {0x28, 0x29, 0x2A})
        for (int i = 0; i < 15; i++) {
            reg(h, 0, i);
            reg(h, r, r == 0x2A ? 0 : 21);
        }
    for (int i = 0; i < 15; i++)
        lfoReg[1][i] = 0x1000;
    reg(h, 1, 0);
    config(h, 0, 0x40);
    reg(h, 0x17, 0x23);
    std::copy_n(flt::TAGS, 512, tag);
    reg(h, 0, 0x8000);
    for (int i = 0; i < 512; i++)
        reg(h, 0x0C, tag[i]);
    for (int w = 0; w < 5; w++) {
        reg(h, 0, 0x8000);
        for (int i = 0; i < 512; i++)
            reg(h, 10 - w, prog[i][w] = flt::PROGRAM[i][w]);
    }
    for (int i = 0; i < 512; i++)
        setCoef(h, i, flt::CONSTANTS[i]);
    config(h, 0, flt::CFG16[0]);
    reg(h, 0x17, 0);
    for (int s = 0; s < 16; s++) {
        setGain(h, s, false, 0);
        setGain(h, s, true, -1);
        setRamp(h, s, MUTE);
    }
    egInterrupts(h, true);
    for (int i = 0; i < 16; i++) {
        egRate(h, i, HOLD);
        egAim(h, i, 511);
        gainTags(h, i, GLIDE);
        slotTags(h, i, GLIDE);
    }
}

void FilterHost::freeSlots()
{
    for (int s = 0; s < 16; s++) {
        setGain(*this, s, false, -1);
        setGain(*this, s, true, -1);
        setRamp(*this, s, IDLE);
    }
    for (int s = 0; s < 16; s++) {
        slotPart[s] = 16;
        stage[s] = 0;
        slotMod[s] = 0;
    }
}

// steps 5 to 9 load 0 into five registers once the chip is triggered. They are pointed
// at each slot's five state registers in turn.
void FilterHost::clearStates()
{
    for (int s = 0; s < 16; s++) {
        for (int j = 0; j < 5; j++) {
            prog[5 + j][1] = uint16_t((prog[5 + j][1] & 0xFF80) | (6 * ((s + 4) & 15) + 1 + j));
            writeProgram(*this, 5 + j);
        }
        reg(*this, 0x14, 1);
    }
}

void FilterHost::noteOn(int s, int p, int n, int v)
{
    Host& h = *this;
    slotPart[s] = uint8_t(p);
    note[s] = uint8_t(n);
    vel[s] = uint8_t(v);
    const uint8_t* V = voiceOf(h, p);
    const uint8_t* R = partOf(h, p);
    const int ch = channel(s);
    stage[s] = 0;
    slotTags(h, s, JUMP);

    // the EG jumps to its start level, then runs the attack: to L1 in T1, shortened by key and velocity
    egRate(h, ch, 0);
    egTarget(h, ch, level(V[0x65]));
    egRate(h, ch, HOLD);
    egInterrupts(h, false);
    stage[s] = 1;
    const int swing = V[0x66] - V[0x65], end = level(V[0x66]);
    const int time = clamp16(V[0x69] + R[0x1A] - 64, 0, 99), scaling = V[0x6E];
    const int index = clamp16(time - keyScaled(time, n & 0xFF, scaling & 7) - (scaling >> 3 & 7) * time * (vel[s] - 127) / 1778, 0, 99);
    egRate(h, ch, flt::EG_RATE[index]);
    egTarget(h, ch, clamp16(swing ? end : end + 2, -511, 511));             // without a swing, far enough to end at all
    egAim(h, ch, clamp16(swing ? end + swing * 256 / 100 : end + 3, -511, 511));
    egInterrupts(h, true);

    setType(h, s, V[0x54]);
    ksDepth[s] = int8_t(V[0x5B] - 64);
    ksPoint[s] = V[0x5C];
    const int cutoff = cutoffByte(h, s, V, R, p);
    const int reso = resonance(h, s, V, R, std::min<int>(V[0x56], 14) - 7);
    setCutoff(h, s, cutoff);
    setResonance(h, s, reso);
    setLfo(h, s, V, R, p);
    if (V[0x1D] == 1)
        lfoPhase(h, ch, V[0x1C]);
    else if (V[0x05] == 1)
        lfoPhase(h, ch, ch);                                // the firmware's own slip: the channel as the phase
    setEgDepth(h, s, egDepth(h, s, V, R));
    gainTags(h, s, GLIDE);
    slotTags(h, s, GLIDE);
    setGain(h, s, false, V[0x5D]);
    setRamp(h, s, RUN);
}

// every channel the chip reports gets held, then the segment after its slot's stage.
void FilterHost::egInterrupt()
{
    Host& h = *this;
    int index = 0, target = 0, aim = 0;
    for (uint32_t status; (status = sink->read(0x21)) & 0x10;) {
        const int ch = status & 15, s = (ch + 14) & 15, p = slotPart[s];
        egRate(h, ch, HOLD);
        const uint8_t* V = voiceOf(h, p);
        const uint8_t* R = partOf(h, p);
        auto decay = [&](int byte) {                        // a decay time's rate, key scaled
            const int time = clamp16(V[byte] + R[0x1B] - 64, 0, 99);
            return clamp16(time - keyScaled(time, note[s], V[0x6E] & 7), 0, 99);
        };
        switch (stage[s]) {
        case 0:                                             // it fell into the note on: hold L1
            aim = target = level(std::min<int>(V[0x66], 100));
            index = 101;
            break;
        case 1: {                                           // the attack ended: decay 1, to L2
            stage[s] = 2;
            const int swing = V[0x67] - V[0x66], end = level(V[0x67]);
            target = swing ? end : end - 2;
            aim = swing ? end + swing * 256 / 100 : end - 3;
            index = decay(0x6A);
            break;
        }
        case 2:                                             // decay 1 ended: decay 2 approaches L3
            stage[s] = 3;
            aim = level(V[0x68]) + (V[0x68] == V[0x67] ? 3 : 0);
            target = -512;
            index = decay(0x6B);
            break;
        case 3:
            stage[s] = 4;
            aim = level(V[0x68]);
            target = -512;
            index = decay(0x6B);
            break;
        case 4:                                             // hold L3
            aim = level(V[0x68]);
            target = -512;
            index = 101;
            break;
        case 5:                                             // the release ended: hold the start level
            stage[s] = 6;
            aim = level(V[0x65]) + 3;
            target = -512;
            index = 101;
            break;
        }
        egRate(h, ch, flt::EG_RATE[index]);
        egTarget(h, ch, target = clamp16(target, -511, 511));
        egAim(h, ch, aim = clamp16(aim, -511, 511));
    }
}

// the release, to the start level in T4
void FilterHost::noteOff(int s)
{
    Host& h = *this;
    const int ch = channel(s), p = slotPart[s];
    const uint8_t* V = voiceOf(h, p);
    egRate(h, ch, HOLD);
    egInterrupts(h, false);
    const int time = V[0x6C] + partOf(h, p)[0x1C] - 64;
    stage[s] = 5;
    egTarget(h, ch, -512);
    egAim(h, ch, clamp16(level(V[0x65]) - (V[0x68] == V[0x65] ? 3 : 0), -511, 511));
    egInterrupts(h, true);
    egRate(h, ch, flt::EG_RATE[clamp16(time - keyScaled(time, note[s], V[0x6E] & 7), 0, 99)]);
}

void FilterHost::kill(int s)
{
    gainTags(*this, s, JUMP);
    setRamp(*this, s, MUTE);
    setGain(*this, s, false, -1);
}

void FilterHost::mark(int id, int p)
{
    if (id == 0x1054)
        typeChanged[p] = true;
    for (int i : {0x1055, 0x1056, 0x1057, 0x1059, 0x105B, 0x105C, 0x105D, 0x8018, 0x8019})
        if (id == i) cutoffChanged[p] = true;
    for (int i : {0x1018, 0x1019, 0x105A, 0x802E, 0x802F})
        if (id == i) lfoChanged[p] = true;
    for (int i : {0x1056, 0x1064, 0x801F})
        if (id == i) egChanged[p] = true;
}

void FilterHost::setSlotCutoffMod(int s, int v) { slotMod[s] = int8_t(v); slotChanged[s] = true; }
void FilterHost::setPartCutoffMod(int p, int v) { partCutoffMod[p] = int8_t(v); cutoffChanged[p] = true; }
void FilterHost::setPartLfo2Depth(int p, int v) { partLfo2Depth[p] = int8_t(v); lfoChanged[p] = true; }
void FilterHost::setPartLfo2Speed(int p, int v) { partLfo2Speed[p] = int8_t(v); lfoChanged[p] = true; }

// a slot's cutoff after its modulation moved, then for every slot a part plays: the type,
// cutoff, resonance and gain, LFO2, the EG's depth
void FilterHost::flush()
{
    Host& h = *this;
    for (int s = 0; s < 16; s++)
        if (slotChanged[s]) {
            slotChanged[s] = false;
            const int p = slotPart[s];
            setCutoff(h, s, cutoffByte(h, s, voiceOf(h, p), partOf(h, p), p));
        }
    for (int p = 0; p < 4; p++) {
        const uint8_t* V = voice[p];
        const uint8_t* R = part[p];
        auto slots = [&](auto&& each) {
            for (int s = 0; s < 16; s++)
                if (slotPart[s] == p) each(s);
        };
        if (typeChanged[p]) {                               // muted first: the tap changes under the sound
            slots([&](int s) { setRamp(h, s, MUTE); });
            slots([&](int s) { setType(h, s, V[0x54]); });
        }
        if (cutoffChanged[p])
            slots([&](int s) {
                const int cutoff = cutoffByte(h, s, V, R, p), reso = resonance(h, s, V, R, V[0x56] - 7);
                ksDepth[s] = int8_t(V[0x5B] - 64);
                ksPoint[s] = V[0x5C];
                setCutoff(h, s, cutoff);
                setResonance(h, s, reso);
                setGain(h, s, false, V[0x5D]);
            });
        if (lfoChanged[p])
            slots([&](int s) { setLfo(h, s, V, R, p); });
        if (egChanged[p])
            slots([&](int s) { setEgDepth(h, s, egDepth(h, s, V, R)); });
        typeChanged[p] = cutoffChanged[p] = lfoChanged[p] = egChanged[p] = false;
    }
}

}  // namespace fs1r::vop3host
