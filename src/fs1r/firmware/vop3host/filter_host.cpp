// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The filter's host. Slot s has a page of constants, B = 16 + 124 * (((s >> 2) + 1) & 3), and a lane in
// it, k = s & 3. Where its parameters land:
//   type         the slot's tap (WriteSink::setType)
//   cutoff       constant B+3k+2: 3085 + 169 * cutoff + key scaling, up to 24576; 169 is a semitone
//   resonance    constants B+32+k and B+54+4k
//   input gain   constant B+52+4k
//   EG depth     constant B+3k: 288 * depth
//   LFO2         the LFO registers of the slot's channel, and its depth in constant B+3k+1
//   output ramp  constant B+116+k
// The EG runs a segment at a time: EG_RATE, the level it heads for (EG_AIM) and the
// level at which it reports (EG_TARGET). A level byte L is (L - 50) * 256 / 50. The aim lies half the
// swing past the target, so the curve arrives with a slope.
#include "filter_host.h"
#include <algorithm>
#include <initializer_list>

#include "rom_tables.h"

namespace fs1r::vop3host {

namespace {

namespace flt = rom::flt;
using Host = FilterHost;
using namespace filter;

const uint8_t NONE[Host::VOICE_BYTES] = {};                 // what a free slot's part reads as
constexpr int RUN = 682, IDLE = -379, MUTE = -1137;         // the output ramp's step a sample: a fade in and two fades out
constexpr int HOLD = 255, HELD = 101;                       // the EG rate that holds, and its entry in EG_RATE

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
void egRate(Host& h, int ch, int v) { h.writeReg(SELECT, ch); h.writeReg(EG_RATE, v & 0xFF); }
void egTarget(Host& h, int ch, int v) { h.writeReg(SELECT, ch); h.writeReg(EG_TARGET, v); }
void egAim(Host& h, int ch, int v) { h.writeReg(SELECT, ch); h.writeReg(EG_AIM, v); }
void egInterrupts(Host& h, bool on) { h.writeReg(EG_STATUS, on ? 0xFFFF : 0); }
void setCoef(Host& h, int step, int v)
{
    h.writeReg(SELECT, step);
    h.writeReg(COEF, v);
}

// ---- a slot's parameters ----

// the input gain by the voice's gain byte, or silence. The second serial bank's only ever gets silence.
void setGain(Host& h, int s, bool second, int v) { setCoef(h, page(s) + 52 + 4 * lane(s) + second, v < 0 ? 0 : flt::GAIN[v]); }
void setRamp(Host& h, int s, int step) { setCoef(h, page(s) + 116 + lane(s), step); }
void setEgDepth(Host& h, int s, int depth) { setCoef(h, page(s) + 3 * lane(s), 288 * depth); }

// the slot's EG depth, LFO2 depth, cutoff and first resonance constants, zeroed when a slot is taken over
void zeroSlot(Host& h, int s)
{
    const int B = page(s), k = lane(s);
    for (int step : {B + 3 * k, B + 3 * k + 1, B + 3 * k + 2, B + 32 + k})
        setCoef(h, step, 0);
}

// the slot's output: which of the filter's taps the type reads (filters/filter.h)
void setType(Host& h, int s, int type)
{
    h.type[s] = uint8_t(type);
    h.sink->setType(s, type);
    setCoef(h, page(s) + 28 + lane(s), type <= BEF ? 0 : type);
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
    setCoef(h, page(s) + 54 + 4 * lane(s), h.type[s] == HPF || h.type[s] == BEF ? 0 : flt::RESONANCE_B[r]);
}

// The cutoff byte of a slot: the voice's and the part's, and the slot's modulation at the voice's depth.
int cutoffByte(const Host& h, int s, const uint8_t* V, const uint8_t* R, int p)
{
    const int depth = clamp16(V[CUTOFF_LFO1] + 99 * (p < 4 ? h.partCutoffMod[p] : 0) / 127, 0, 99);
    return clamp16(V[CUTOFF] + R[PART_CUTOFF] - 64 + depth * h.slotMod[s] * 12288 / 2127609, 0, 127);
}
// The resonance byte, with its velocity sensitivity less 7.
int resonance(const Host& h, int s, const uint8_t* V, const uint8_t* R, int sens)
{
    const int v = h.vel[s];
    return clamp16(V[RESONANCE] + 2 * (R[PART_RESONANCE] - 64) + sens * (116 * (sens > 0 ? v - 127 : v) / 889), 0, 116);
}
int egDepth(const Host& h, int s, const uint8_t* V, const uint8_t* R)
{
    const int sens = V[EG_DEPTH_VEL] - 7, base = R[PART_EG_DEPTH] + V[EG_DEPTH] - 128, v = h.vel[s];
    return clamp16(base + sens * base * (sens > 0 ? v - 127 : v) / 889, -64, 64);
}

// ---- LFO2 ----
void lfoWave(Host& h, int ch, int wave)
{
    const int bits = wave <= 5 ? wave << 5 : wave == 6 ? 224 : wave;    // past 6 the firmware ORs the byte in as it came
    h.lfoReg[1][ch] = uint16_t((h.lfoReg[1][ch] & 0xFF1F) | bits);
    h.writeLfo(LFO_WAVE, ch);
}
void lfoSpeed(Host& h, int ch, int v)
{
    h.lfoReg[0][ch] = flt::LFO2_SPEED[v] | (h.lfoReg[0][ch] & 0x0C00);
    h.writeLfo(LFO_SPEED, ch);
}
void lfoPhase(Host& h, int ch, int v)
{
    h.lfoReg[1][ch] = (flt::LFO2_PHASE[v] & 0x1F) | (h.lfoReg[1][ch] & 0xFFE0);
    h.writeLfo(LFO_WAVE, ch);
    h.resetLfoPhase(1 << ch);                               // back to that phase
}
// the two sawtooths run at full amount with an offset word, the others at half; then the slot's depth
void lfoDepth(Host& h, int ch, int wave, int depth)
{
    h.lfoReg[2][ch] = wave == SAW_DOWN || wave == SAW_UP ? 0x7FFF : (64 << 15) / 127;
    h.writeLfo(LFO_DEPTH, ch);
    h.lfoReg[3][ch] = wave == SAW_DOWN ? 0x4000 : wave == SAW_UP ? 0xC000 : 0;
    h.writeLfo(LFO_DIRECTION, ch);
    const int s = (ch + 14) & 15;
    setCoef(h, page(s) + 3 * lane(s) + 1, 124 * depth);
}
void setLfo(Host& h, int s, const uint8_t* V, const uint8_t* R, int p)
{
    const int ch = channel(s), wave = V[LFO2_WAVE];
    lfoWave(h, ch, wave);
    lfoSpeed(h, ch, clamp16(V[LFO2_SPEED] + R[PART_LFO2_SPEED] + h.partLfo2Speed[p] - 64, 0, 127));
    lfoDepth(h, ch, wave, clamp16(V[CUTOFF_LFO2] + R[PART_LFO2_DEPTH] + 99 * h.partLfo2Depth[p] / 127 - 64, 0, 99));
}

}  // namespace

// the constants, then every slot muted with its EG held. Registers 1 and 5
// and the three tables of fifteen at 0x28..0x2A are written here only.
void FilterHost::boot()
{
    Host& h = *this;
    h.writeReg(1, 1);
    h.writeReg(5, 0x1004);
    for (int i = 0; i < 15; i++)
        h.writeConfig(i, flt::CFG16[i]);
    for (int r : {0x28, 0x29, 0x2A})
        for (int i = 0; i < 15; i++) {
            h.writeReg(SELECT, i);
            h.writeReg(r, r == 0x2A ? 0 : 21);
        }
    for (int i = 0; i < 15; i++)
        lfoReg[1][i] = 0x1000;
    h.writeReg(1, 0);
    h.writeConfig(0, 0x40);
    h.writeReg(UPLOADING, 0x23);
    for (int i = 0; i < 512; i++)
        setCoef(h, i, flt::CONSTANTS[i]);
    h.writeConfig(0, flt::CFG16[0]);
    h.writeReg(UPLOADING, 0);
    for (int s = 0; s < 16; s++) {
        setGain(h, s, false, 0);
        setGain(h, s, true, -1);
        setRamp(h, s, MUTE);
    }
    egInterrupts(h, true);
    for (int i = 0; i < 16; i++) {
        egRate(h, i, HOLD);
        egAim(h, i, 511);
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
        stage[s] = START;
        slotMod[s] = 0;
    }
}

// every slot's filter state back to zero
void FilterHost::clearStates()
{
    for (int s = 0; s < 16; s++)
        sink->clear(s);
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
    stage[s] = START;
    zeroSlot(h, s);

    // the EG jumps to its start level, then runs the attack: to L1 in T1, shortened by key and velocity
    egRate(h, ch, 0);
    egTarget(h, ch, level(V[EG_L4]));
    egRate(h, ch, HOLD);
    egInterrupts(h, false);
    stage[s] = ATTACK;
    const int swing = V[EG_L1] - V[EG_L4], end = level(V[EG_L1]);
    const int time = clamp16(V[EG_T1] + R[PART_ATTACK] - 64, 0, 99), scaling = V[EG_SCALING];
    const int index = clamp16(time - keyScaled(time, n & 0xFF, scaling & 7) - (scaling >> 3 & 7) * time * (vel[s] - 127) / 1778, 0, 99);
    egRate(h, ch, flt::EG_RATE[index]);
    egTarget(h, ch, clamp16(swing ? end : end + 2, -511, 511));             // without a swing, far enough to end at all
    egAim(h, ch, clamp16(swing ? end + swing * 256 / 100 : end + 3, -511, 511));
    egInterrupts(h, true);

    setType(h, s, V[FILTER_TYPE]);
    ksDepth[s] = int8_t(V[KEY_SCALE_DEPTH] - 64);
    ksPoint[s] = V[KEY_SCALE_POINT];
    const int cutoff = cutoffByte(h, s, V, R, p);
    const int reso = resonance(h, s, V, R, std::min<int>(V[RESONANCE_VEL], 14) - 7);
    setCutoff(h, s, cutoff);
    setResonance(h, s, reso);
    setLfo(h, s, V, R, p);
    if (V[LFO2_KEY_SYNC] == 1)
        lfoPhase(h, ch, V[LFO2_PHASE]);
    else if (V[5] == 1)                                     // a byte of the voice's name: the firmware's own slip,
        lfoPhase(h, ch, ch);                                // with the channel as the phase
    setEgDepth(h, s, egDepth(h, s, V, R));
    setGain(h, s, false, V[INPUT_GAIN]);
    setRamp(h, s, RUN);
}

// Every channel the chip reports gets held, then the segment after its slot's stage.
void FilterHost::egInterrupt()
{
    Host& h = *this;
    int index = 0, target = 0, aim = 0;
    for (uint32_t status; (status = sink->read(EG_STATUS)) & 0x10;) {
        const int ch = status & 15, s = (ch + 14) & 15, p = slotPart[s];
        egRate(h, ch, HOLD);
        const uint8_t* V = voiceOf(h, p);
        const uint8_t* R = partOf(h, p);
        auto decay = [&](int byte) {                        // a decay time's rate, key scaled
            const int time = clamp16(V[byte] + R[PART_DECAY] - 64, 0, 99);
            return clamp16(time - keyScaled(time, note[s], V[EG_SCALING] & 7), 0, 99);
        };
        switch (stage[s]) {
            case START:                                     // it fell into the note on: hold L1
                aim = target = level(std::min<int>(V[EG_L1], 100));
                index = HELD;
                break;
            case ATTACK: {                                  // the attack ended: decay 1, to L2
                stage[s] = DECAY1;
                const int swing = V[EG_L2] - V[EG_L1], end = level(V[EG_L2]);
                target = swing ? end : end - 2;
                aim = swing ? end + swing * 256 / 100 : end - 3;
                index = decay(EG_T2);
                break;
            }
            case DECAY1:                                    // decay 1 ended: decay 2 approaches L3
                stage[s] = DECAY2;
                aim = level(V[EG_L3]) + (V[EG_L3] == V[EG_L2] ? 3 : 0);
                target = -512;
                index = decay(EG_T3);
                break;
            case DECAY2:
                stage[s] = SUSTAIN;
                aim = level(V[EG_L3]);
                target = -512;
                index = decay(EG_T3);
                break;
            case SUSTAIN:                                   // hold L3
                aim = level(V[EG_L3]);
                target = -512;
                index = HELD;
                break;
            case RELEASE:                                   // the release ended: hold the start level
                stage[s] = DONE;
                aim = level(V[EG_L4]) + 3;
                target = -512;
                index = HELD;
                break;
            case DONE:                                      // held, so it does not report; were it to, it would get
                break;                                      // what the channel before it got, as on the unit
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
    const int time = V[EG_T4] + partOf(h, p)[PART_RELEASE] - 64;
    stage[s] = RELEASE;
    egTarget(h, ch, -512);
    egAim(h, ch, clamp16(level(V[EG_L4]) - (V[EG_L3] == V[EG_L4] ? 3 : 0), -511, 511));
    egInterrupts(h, true);
    egRate(h, ch, flt::EG_RATE[clamp16(time - keyScaled(time, note[s], V[EG_SCALING] & 7), 0, 99)]);
}

void FilterHost::kill(int s)
{
    setRamp(*this, s, MUTE);
    setGain(*this, s, false, -1);
}

void FilterHost::setByte(int id, int p, int v)
{
    uint8_t& b = id >= PART ? part[p][id - PART] : voice[p][id - VOICE];
    v &= 0x7F;
    if (v != b) {
        b = uint8_t(v);
        mark(id, p);
    }
}

void FilterHost::mark(int id, int p)
{
    auto any = [&](std::initializer_list<int> ids) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
    if (id == VOICE + FILTER_TYPE)
        typeChanged[p] = true;
    if (any({VOICE + RESONANCE, VOICE + RESONANCE_VEL, VOICE + CUTOFF, VOICE + CUTOFF_LFO1, VOICE + KEY_SCALE_DEPTH,
             VOICE + KEY_SCALE_POINT, VOICE + INPUT_GAIN, PART + PART_CUTOFF, PART + PART_RESONANCE}))
        cutoffChanged[p] = true;
    if (any({VOICE + LFO2_WAVE, VOICE + LFO2_SPEED, VOICE + CUTOFF_LFO2, PART + PART_LFO2_SPEED, PART + PART_LFO2_DEPTH}))
        lfoChanged[p] = true;
    if (any({VOICE + RESONANCE_VEL, VOICE + EG_DEPTH, PART + PART_EG_DEPTH}))
        egChanged[p] = true;
}

void FilterHost::setSlotCutoffMod(int s, int v) { slotMod[s] = int8_t(v); slotChanged[s] = true; }
void FilterHost::setPartCutoffMod(int p, int v) { partCutoffMod[p] = int8_t(v); cutoffChanged[p] = true; }
void FilterHost::setPartLfo2Depth(int p, int v) { partLfo2Depth[p] = int8_t(v); lfoChanged[p] = true; }
void FilterHost::setPartLfo2Speed(int p, int v) { partLfo2Speed[p] = int8_t(v); lfoChanged[p] = true; }

// A slot's cutoff after its modulation moved, then for every slot a part plays: the type,
// cutoff, resonance and gain, LFO2, the EG's depth.
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
            slots([&](int s) { setType(h, s, V[FILTER_TYPE]); });
        }
        if (cutoffChanged[p])
            slots([&](int s) {
                const int cutoff = cutoffByte(h, s, V, R, p), reso = resonance(h, s, V, R, V[RESONANCE_VEL] - 7);
                ksDepth[s] = int8_t(V[KEY_SCALE_DEPTH] - 64);
                ksPoint[s] = V[KEY_SCALE_POINT];
                setCutoff(h, s, cutoff);
                setResonance(h, s, reso);
                setGain(h, s, false, V[INPUT_GAIN]);
            });
        if (lfoChanged[p])
            slots([&](int s) { setLfo(h, s, V, R, p); });
        if (egChanged[p])
            slots([&](int s) { setEgDepth(h, s, egDepth(h, s, V, R)); });
        typeChanged[p] = cutoffChanged[p] = lfoChanged[p] = egChanged[p] = false;
    }
}

}  // namespace fs1r::vop3host
