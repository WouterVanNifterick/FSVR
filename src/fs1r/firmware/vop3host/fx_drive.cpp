// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The wahs, distortions and dynamics: auto and touch wah, distortion and overdrive, amp simulator,
// enhancer, compressor and noise gate of the variation and the insertion, and the insertion's
// combinations of them with each other and with a delay.
#include "fx_common.h"
#include <cstdint>

namespace fs1r::vop3host {

namespace {

// What the wahs of a block write to
struct Wah {
    int lfo;                                        // which of the window's LFOs sweeps it
    Shelves shelves;
};
const Wah WAHS[2] = {
    {0, {var::WAH_FILTER, var::WAH_FILTER_3, var::WAH_FILTER_2, var::WAH_FILTER_4}},
    {1, {ins::LPF_WAH,    ins::LPF_WAH_C,    ins::HPF_WAH,      ins::HPF_WAH_C}},
};

// What the distortions and the amp simulator of a block write to
struct Drive {
    const uint16_t *drive, *lowShelf, *lowpass, *peak, *edge, *inputFilter, *outputFilter;
    int inputOrder;
};
const Drive DRIVES[2] = {
    {var::DIST_DRIVE, var::DIST_LOW_SHELF, var::DIST_LPF2, var::DIST_PEAK,  var::DIST_BAND, var::DIST_HPF, var::DIST_LPF, 1},
    {ins::DIST_DRIVE, ins::DIST_EQ,        ins::DIST_PRE,  ins::DIST_PRE_C, ins::DIST_FILL, ins::DIST_LPF, ins::DIST_HPF, 2},
};

// What the compressor, the noise gate and the enhancer of a block write to
struct Dynamics {
    const uint16_t *attack, *release, *threshold, *ratio, *level;
    int levelWords;
    const uint16_t *highpass, *highpassCopy, *gain, *gainCopy;
};
const Dynamics DYNAMICS[2] = {
    {var::DYN_ATTACK, var::DYN_RELEASE, var::DYN_THRESHOLD, var::DYN_RATIO, var::DYN_OUT,  4, var::DYN_HPF2, var::DYN_HPF2_COPY, var::DYN_HPF,  var::DYN_HPF_COPY},
    {ins::DYN_THRESH, ins::DYN_PAIR,    ins::DYN_WORD,      ins::DYN_ROW,   ins::DIST_WET, 5, ins::DIST_LPF, ins::DIST_LPF_C,    ins::DIST_HPF, ins::DIST_HPF_C},
};

// the wah's resonance: -327680 / v from 11 on, else the most negative word
void wahSense(EffectHost& h, int v, const uint16_t* steps)
{
    h.coef[steps[1]] = h.coef[steps[0]] = v < 11 ? MINUS_ONE : int16_t(int(-327680.0 / double(float(v))));
    h.writeCoefs(2, steps);
}

// the release byte, 52 up, as a step of 0 to 15; out of range through eight bits
int releaseStep(int byte)
{
    const int v = int8_t(byte) - 52;
    const int8_t c = int8_t(v);
    return c < 0 ? 0 : c > 15 ? 15 : v;
}

// the insertion's distortion mode pair (a, b), then its dry and wet: g, x, 0, g, x
void distortionWet(EffectHost& h, uint32_t a, uint32_t b, bool levelPair)
{
    using namespace ins;
    h.coef[DIST_MODE[0]] = a;
    h.coef[levelPair ? DIST_LEVEL[0] : DIST_MODE[1]] = b;
    if (levelPair) {
        h.writeCoefs(1, DIST_MODE);
        h.writeCoefs(1, DIST_LEVEL);
    } else {
        h.writeCoefs(2, DIST_MODE);
    }
    const uint32_t wet = h.param[INSERTION][9];
    h.coef[DIST_WET[2]] = h.coef[DIST_WET[0]] = rom::PAN_LAW[128 - wet] >> 2;
    h.coef[DIST_WET[4]] = h.coef[DIST_WET[1]] = h.typeGain(INSERTION, rom::PAN_LAW[wet] >> 2);
    h.coef[DIST_WET[3]] = 0;
    h.writeCoefs(5, DIST_WET);
}

// The insertion's wah + distortion: the drive, the two mode pairs, and eight dry and wet words
// laid out by whether the drive is on.
void wahDistortionWet(EffectHost& h, const uint16_t* wetSteps, int drive, int level, int shift,
                      const uint16_t* driveSteps, const uint16_t* mode, const uint16_t* mode2)
{
    const uint16_t* P = h.param[INSERTION];
    h.storeDrive(drive, level, driveSteps);
    h.coef[mode[1]] = h.coef[mode[0]] = SQRT_HALF;
    if (mode2)
        h.coef[mode2[1]] = h.coef[mode2[0]] = 0x5A81;      // one short of SQRT_HALF, as the firmware has it
    h.writeCoefs(2, mode);
    if (mode2)
        h.writeCoefs(2, mode2);
    const int32_t dry = rom::PAN_LAW[128 - P[9]], wet = rom::PAN_LAW[P[9]];
    const uint16_t* s = wetSteps;
    h.coef[s[4]] = h.coef[s[0]] = dry >> 2;
    if (drive == 0) {
        h.coef[s[5]] = h.coef[s[1]] = h.typeGain(INSERTION, wet >> 2);
        h.coef[s[6]] = h.coef[s[2]] = shift == 1 ? HALF : QUARTER;
        h.coef[s[7]] = h.coef[s[3]] = 0;
    } else {
        h.coef[s[6]] = h.coef[s[2]] = dry >> shift;
        h.coef[s[5]] = h.coef[s[1]] = wet >> 2;
        h.coef[s[7]] = h.coef[s[3]] = h.typeGain(INSERTION, wet >> shift);
    }
    h.writeCoefs(8, s);
}

// The insertion's distortion stage after a wah: low and mid gain at the type's frequencies, and
// the lowpass. `second` picks the other of the two types' settings.
void wahDistortionTone(EffectHost& h, int p, bool second)
{
    const uint16_t* P = h.param[INSERTION];
    switch (p) {
        case 11: h.designFilter(2, 4, LOW_SHELF2, second ? 24 : 20, P[11], 0, ins::WAH_LOW_SHELF2); break;
        case 12: h.designFilter(2, 1, PEAK, second ? 41 : 43, P[12], 10, ins::WAH_PEAK); break;
        case 13: h.designFilter(2, 1, LPF2, P[13], 64, 7, ins::WAH_LPF2); break;
    }
}
// and its fixed edge and band filters
void wahDistortionBand(EffectHost& h, bool second)
{
    h.storeEdge(second ? 104 : 120, ins::WAH_BAND);
    h.designFilter(1, 1, HPF1, second ? 8 : 0, 64, 0, ins::WAH_HPF_2);
    h.designFilter(1, 1, LPF1, second ? 44 : 54, 64, 0, ins::WAH_LPF_2);
}

// the insertion's gate ahead of its distortions, fixed
void distortionGate(EffectHost& h, const uint16_t* attack, const uint16_t* release, const uint16_t* threshold)
{
    h.storeGateAttack(0, attack);
    h.storeRelease(11, release);
    h.storeGateThreshold(73, threshold);
}

// the distortion's own parameters; false for the others
bool distortionParam(EffectHost& h, int w, int p, const Drive& b)
{
    enum { DRIVE, EQ_LOW_FREQ, EQ_LOW_GAIN, LPF_CUTOFF, OUTPUT_LEVEL, EQ_MID_FREQ = 6, EQ_MID_GAIN, EQ_MID_Q, EDGE = 10 };
    const uint16_t* P = h.param[w];
    switch (p) {
        case DRIVE: case OUTPUT_LEVEL:
            h.storeDrive(P[DRIVE], P[OUTPUT_LEVEL], b.drive);
            return true;
        case EQ_LOW_FREQ: case EQ_LOW_GAIN:
            h.designFilter(2, 4, LOW_SHELF1, P[EQ_LOW_FREQ], P[EQ_LOW_GAIN], 0, b.lowShelf);
            return true;
        case LPF_CUTOFF:
            h.designFilter(2, 1, LPF2, P[LPF_CUTOFF], 64, 7, b.lowpass);
            return true;
        case EQ_MID_FREQ: case EQ_MID_GAIN: case EQ_MID_Q:
            h.designFilter(2, 1, PEAK, P[EQ_MID_FREQ], P[EQ_MID_GAIN], P[EQ_MID_Q], b.peak);
            return true;
        case EDGE:
            h.storeEdge(P[EDGE], b.edge);
            return true;
    }
    return false;
}

// The compressor's and the noise gate's output level: the level curve at 8192 and the type's
// gain into the level's second and fourth words, nothing into the rest.
void dynamicsLevel(EffectHost& h, int w, int v, const Dynamics& b)
{
    const uint16_t x = h.typeGain(w, int16_t(int(double(rom::LEVEL_CURVE[v]) * 8192.0)));
    for (int i = 0; i < b.levelWords; i++)
        h.coef[b.level[i]] = i == 1 || i == 3 ? x : 0;
    h.writeCoefs(b.levelWords, b.level);
}
// The dynamics' filters: a highpass held open and a plain gain; the insertion also opens the
// distortion's filters it shares the program with.
void dynamicsFilters(EffectHost& h, int w, const Dynamics& b)
{
    h.designFilter(2, 1, HPF1, 0, 64, 0, b.highpass);
    h.copyCoefs(5, b.highpass, b.highpassCopy);
    h.designFilter(1, 1, GAIN_ONLY, 0, 64, 0, b.gain);
    h.copyCoefs(3, b.gain, b.gainCopy);
    if (w == INSERTION) {
        h.designFilter(2, 1, GAIN_ONLY, 0, 64, 0, ins::DIST_PRE);
        h.copyCoefs(5, ins::DIST_PRE, ins::DIST_PRE_C);
        h.designFilter(2, 4, GAIN_ONLY, 0, 64, 0, ins::DIST_EQ);
    }
}

}  // namespace

// ---- the wahs ----

void EffectHost::autoWah(int w, int p)
{
    enum { LFO_FREQ, LFO_DEPTH, CUTOFF, RESONANCE, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, DRY_WET = 9,
           DRIVE, DS_LOW_GAIN, DS_MID_GAIN, LPF_CUTOFF, OUTPUT_LEVEL };   // from DRIVE the insertion's distortion
    const Wah& b = WAHS[w == INSERTION];
    const uint16_t* P = param[w];
    const bool second = code[w].lsb == 4;           // Overdrive
    switch (p) {
        case LFO_FREQ:
            setLfoSpeed(P[LFO_FREQ], lfo(w, b.lfo));
            break;
        case LFO_DEPTH:
        case CUTOFF: {
            const uint16_t* cutoff = w == INSERTION ? ins::WAH_CUTOFF : var::WAH_CUTOFF;
            const uint16_t* depth  = w == INSERTION ? ins::WAH_DEPTH  : var::WAH_DEPTH;
            const int32_t c = wahCutoff(P[CUTOFF], 7837);
            coef[cutoff[0]] = c;
            coef[depth[0]] = P[LFO_DEPTH] * uint32_t(UNITY - int16_t(c)) / 127;
            writeCoefs(1, cutoff);
            writeCoefs(1, depth);
            break;
        }
        case RESONANCE:
            wahSense(*this, P[RESONANCE], w == INSERTION ? ins::WAH_SENSE : var::WAH_SENSE);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case DRY_WET: case DRIVE: case OUTPUT_LEVEL:
            if (w == VARIATION) {
                if (p == DRY_WET)
                    dryWet(w, var::WAH_PAN, true);
            } else {
                wahDistortionWet(*this, ins::WAH_WET, P[DRIVE], P[OUTPUT_LEVEL] & 0xFF, 2, ins::WAH_DRIVE, ins::WAH_MODE, nullptr);
            }
            break;
        case DS_LOW_GAIN: case DS_MID_GAIN: case LPF_CUTOFF:
            if (w == INSERTION)
                wahDistortionTone(*this, p, second);
            break;
        case ALL:
            if (w == VARIATION) {
                applyAll(w, &EffectHost::autoWah, {LFO_FREQ, LFO_DEPTH, RESONANCE, EQ_LOW_FREQ, EQ_HIGH_FREQ, DRY_WET});
            } else {
                hold = true;
                wahDistortionBand(*this, second);
                for (int q : {LFO_FREQ, LFO_DEPTH, RESONANCE, EQ_LOW_FREQ, EQ_HIGH_FREQ, DRY_WET, DS_LOW_GAIN, DS_MID_GAIN, LPF_CUTOFF})
                    autoWah(w, q);
                hold = false;
            }
            break;
    }
}

void EffectHost::touchWah(int w, int p)
{
    enum { SENSITIVITY, CUTOFF, RESONANCE, EQ_LOW_FREQ = 5, EQ_HIGH_FREQ = 7, DRY_WET = 9,
           DRIVE, DS_LOW_GAIN, DS_MID_GAIN, LPF_CUTOFF, OUTPUT_LEVEL, RELEASE };   // from DRIVE the insertion's distortion
    const Wah& b = WAHS[w == INSERTION];
    const uint16_t* P = param[w];
    const bool second = code[w].lsb == 6;           // T-Wah+Dist takes the settings A-Wah+Odrv has
    switch (p) {
        case SENSITIVITY: {
            const uint16_t* attack = w == INSERTION ? ins::WAH_ATTACK : var::WAH_ATTACK;
            coef[attack[0]] = P[SENSITIVITY] * 258;
            writeCoefs(1, attack);
            [[fallthrough]];                        // the firmware runs on into the cutoff
        }
        case CUTOFF: {
            const uint16_t* cutoff = w == INSERTION ? ins::WAH_CUTOFF : var::WAH_CUTOFF;
            coef[cutoff[0]] = wahCutoff(P[CUTOFF], 7837);
            writeCoefs(1, cutoff);
            break;
        }
        case RESONANCE:
            wahSense(*this, P[RESONANCE], w == INSERTION ? ins::WAH_SENSE : var::WAH_SENSE);
            break;
        case EQ_LOW_FREQ: case EQ_LOW_FREQ + 1: case EQ_HIGH_FREQ: case EQ_HIGH_FREQ + 1:
            shelves(w, p, EQ_LOW_FREQ, b.shelves);
            break;
        case DRY_WET: case DRIVE: case OUTPUT_LEVEL:
            if (w == VARIATION) {
                if (p == DRY_WET)
                    dryWet(w, var::WAH_PAN, true);
            } else {
                wahDistortionWet(*this, ins::WAH_WET, P[DRIVE], P[OUTPUT_LEVEL] & 0xFF, 2, ins::WAH_DRIVE, ins::WAH_MODE, ins::WAH_MODE2);
            }
            break;
        case DS_LOW_GAIN: case DS_MID_GAIN: case LPF_CUTOFF:
            if (w == INSERTION)
                wahDistortionTone(*this, p, second);
            break;
        case RELEASE:
            storeWahRelease(releaseStep(P[RELEASE]), w == INSERTION ? ins::WAH_MIRROR : var::WAH_MIRROR);
            break;
        case ALL:
            hold = true;
            if (w == INSERTION)
                wahDistortionBand(*this, second);
            designFilter(1, 1, HPF1, 0, 64, 0, w == INSERTION ? ins::WAH_HPF : var::WAH_HPF);
            designFilter(1, 0, LPF1, 54, 64, 0, w == INSERTION ? ins::WAH_LPF : var::WAH_LPF);
            for (int q : {SENSITIVITY, CUTOFF, RESONANCE, EQ_LOW_FREQ, EQ_HIGH_FREQ, DRY_WET, DS_LOW_GAIN, DS_MID_GAIN, LPF_CUTOFF, RELEASE})
                touchWah(w, q);
            hold = false;
            break;
    }
}

// ---- distortion ----

// Distortion and Overdrive
void EffectHost::distortion(int w, int p)
{
    enum { DRIVE, EQ_LOW_FREQ, LPF_CUTOFF = 3, EQ_MID_FREQ = 6, DRY_WET = 9, EDGE };
    const Drive& b = DRIVES[w == INSERTION];
    const bool overdrive = code[w].lsb != 1;
    switch (p) {
        case DRY_WET:
            if (w == VARIATION) {
                dryWet(w, var::DIST_PAN, true);
            } else {
                distortionWet(*this, 1448, 0, false);
                distortion(w, DRIVE);
            }
            break;
        case ALL:
            hold = true;
            if (w == INSERTION)
                distortionGate(*this, ins::DYN_THRESH, ins::DYN_PAIR, ins::DYN_WORD);
            // the input highpass and the output lowpass, fixed by the type
            designFilter(b.inputOrder, 1, HPF1, overdrive ? 8 : 0, 64, 0, b.inputFilter);
            designFilter(1, 1, LPF1, overdrive ? 44 : 54, 64, 0, b.outputFilter);
            for (int q : {DRIVE, EQ_LOW_FREQ, LPF_CUTOFF, EQ_MID_FREQ, DRY_WET, EDGE})
                if (q != DRIVE || w == VARIATION)   // the insertion's comes with its dry and wet
                    distortion(w, q);
            hold = false;
            break;
        default:
            distortionParam(*this, w, p, b);
            break;
    }
}

void EffectHost::ampSimulator(int w, int p)
{
    enum { DRIVE, AMP_TYPE, LPF_CUTOFF, OUTPUT_LEVEL, DRY_WET = 9, EDGE };
    const Drive& b = DRIVES[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case DRIVE: case OUTPUT_LEVEL:
            storeDrive(P[DRIVE], P[OUTPUT_LEVEL], b.drive);
            break;
        case AMP_TYPE: {
            // four filters by the amp, each row its shape, frequency, gain and Q
            const int8_t* row = rom::AMP_FILTERS + uint32_t(int8_t((P[AMP_TYPE] & 0xFF) * 14));
            designFilter(b.inputOrder, 1, row[0], row[2], row[1], 0, b.inputFilter);
            designFilter(1, 1, row[3], row[5], row[4], 0, b.outputFilter);
            designFilter(2, 1, row[6], row[8], row[7], 0, b.peak);
            designFilter(2, 4, row[9], row[11], row[10], row[12], b.lowShelf);
            ampSimulator(w, LPF_CUTOFF);
            break;
        }
        case LPF_CUTOFF:
            if (P[AMP_TYPE] == 1)
                designFilter(2, 1, LPF2, P[LPF_CUTOFF], 56, 15, b.lowpass);
            else
                designFilter(2, 1, LPF2, P[LPF_CUTOFF], 64, P[AMP_TYPE] == 2 ? 15 : 7, b.lowpass);
            break;
        case DRY_WET:
            if (w == VARIATION) {
                dryWet(w, var::DIST_PAN, true);
            } else {
                distortionWet(*this, 1448, 0, false);
                ampSimulator(w, DRIVE);
            }
            break;
        case EDGE:
            storeEdge(P[EDGE], b.edge);
            break;
        case ALL:
            hold = true;
            if (w == INSERTION)
                distortionGate(*this, ins::DYN_THRESH, ins::DYN_PAIR, ins::DYN_WORD);
            for (int q : {DRIVE, AMP_TYPE, LPF_CUTOFF, DRY_WET, EDGE})
                if (q != DRIVE || w == VARIATION)   // the insertion's comes with its dry and wet
                    ampSimulator(w, q);
            hold = false;
            break;
    }
}

// ---- dynamics ----

void EffectHost::compressor(int w, int p)
{
    enum { ATTACK, RELEASE, THRESHOLD, RATIO, OUTPUT_LEVEL, LEVEL = 9 };   // LEVEL is the insertion's fixed one
    const Dynamics& b = DYNAMICS[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case ATTACK:       storeCompAttack(P[ATTACK], b.attack);          break;
        case RELEASE:      storeRelease(P[RELEASE], b.release);           break;
        case THRESHOLD:    storeCompThreshold(P[THRESHOLD], b.threshold); break;
        case RATIO:        storeRatio(P[RATIO], b.ratio);                 break;
        case OUTPUT_LEVEL: dynamicsLevel(*this, w, P[OUTPUT_LEVEL], b);   break;
        case LEVEL:
            if (w == INSERTION) {
                coef[ins::DIST_LEVEL[1]] = coef[ins::DIST_LEVEL[0]] = 7240;
                writeCoefs(2, ins::DIST_LEVEL);
            }
            break;
        case ALL:
            hold = true;
            dynamicsFilters(*this, w, b);
            for (int q : {ATTACK, RELEASE, THRESHOLD, RATIO, OUTPUT_LEVEL, LEVEL})
                compressor(w, q);
            hold = false;
            break;
    }
}

void EffectHost::noiseGate(int w, int p)
{
    enum { ATTACK, RELEASE, THRESHOLD, OUTPUT_LEVEL };
    const Dynamics& b = DYNAMICS[w == INSERTION];
    const uint16_t* P = param[w];
    switch (p) {
        case ATTACK:       storeGateAttack(P[ATTACK], b.attack);          break;
        case RELEASE:      storeRelease(P[RELEASE], b.release);           break;
        case THRESHOLD:    storeGateThreshold(P[THRESHOLD], b.threshold); break;
        case OUTPUT_LEVEL: dynamicsLevel(*this, w, P[OUTPUT_LEVEL], b);   break;
        case ALL:
            hold = true;
            dynamicsFilters(*this, w, b);
            for (int q : {ATTACK, RELEASE, THRESHOLD, OUTPUT_LEVEL})
                noiseGate(w, q);
            hold = false;
            break;
    }
}

// HM Enhancer
void EffectHost::enhancer(int w, int p)
{
    enum { HPF_CUTOFF, DRIVE, MIX_LEVEL, LEVEL = 9 };   // LEVEL is the insertion's fixed one
    const Dynamics& b = DYNAMICS[w == INSERTION];
    const uint16_t* P = param[w];
    const uint16_t* drive = w == INSERTION ? ins::HM_DRIVE : var::HM_DRIVE;
    switch (p) {
        case HPF_CUTOFF:
            designFilter(2, 1, HPF2, P[HPF_CUTOFF], 64, 7, b.highpass);
            copyCoefs(5, b.highpass, b.highpassCopy);
            designFilter(1, 1, HPF1, P[HPF_CUTOFF], 64, 0, b.gain);
            copyCoefs(3, b.gain, b.gainCopy);
            break;
        case DRIVE:
            coef[drive[1]] = coef[drive[0]] = P[DRIVE] * 258;
            writeCoefs(2, drive);
            break;
        case MIX_LEVEL:
            coef[b.level[3]] = coef[b.level[1]] = typeGain(w, int32_t((P[MIX_LEVEL] * 0x81u) >> 1));
            writeCoefs(1, b.level + 1);
            writeCoefs(1, b.level + 3);
            break;
        case LEVEL:
            if (w == INSERTION) {
                coef[ins::DIST_LEVEL[1]] = coef[ins::DIST_LEVEL[0]] = 1448;
                writeCoefs(2, ins::DIST_LEVEL);
            }
            break;
        case ALL:
            hold = true;
            if (w == INSERTION) {                   // the distortion's filters it shares the program with, open
                designFilter(2, 1, GAIN_ONLY, 0, 64, 0, ins::DIST_PRE);
                copyCoefs(5, ins::DIST_PRE, ins::DIST_PRE_C);
                designFilter(2, 4, GAIN_ONLY, 0, 64, 0, ins::DIST_EQ);
            }
            for (int q : {HPF_CUTOFF, DRIVE, MIX_LEVEL, LEVEL})
                enhancer(w, q);
            hold = false;
            break;
    }
}

// ---- the insertion's combinations ----

// Comp+Dist: the distortion's parameters, then a compressor's
void EffectHost::compressorDistortion(int w, int p)
{
    enum { DRIVE, EQ_LOW_FREQ, LPF_CUTOFF = 3, EQ_MID_FREQ = 6, DRY_WET = 9, EDGE, ATTACK, RELEASE, THRESHOLD, RATIO };
    const uint16_t* P = param[w];
    switch (p) {
        case DRY_WET:
            distortionWet(*this, 5792, 14481, true);
            compressorDistortion(w, DRIVE);
            break;
        case ATTACK:    storeCompAttack(P[ATTACK], ins::DYN_THRESH);       break;
        case RELEASE:   storeRelease(P[RELEASE], ins::DYN_PAIR);           break;
        case THRESHOLD: storeCompThreshold(P[THRESHOLD], ins::DYN_WORD);   break;
        case RATIO:     storeRatio(P[RATIO], ins::DYN_ROW);                break;
        case ALL:
            hold = true;
            designFilter(2, 1, HPF1, 0, 64, 0, ins::DIST_LPF);
            designFilter(1, 1, LPF1, 54, 64, 0, ins::DIST_HPF);
            for (int q : {EQ_LOW_FREQ, LPF_CUTOFF, EQ_MID_FREQ, DRY_WET, EDGE, ATTACK, RELEASE, THRESHOLD, RATIO})
                compressorDistortion(w, q);
            hold = false;
            break;
        default:
            distortionParam(*this, w, p, DRIVES[1]);
            break;
    }
}

// Wah+DS+Dly and Wah+OD+Dly: a touch wah into a distortion into a delay
void EffectHost::wahDistortionDelay(int w, int p)
{
    enum { DELAY, FB_LEVEL, DELAY_MIX, DRIVE, OUTPUT_LEVEL, DS_LOW_GAIN, DS_MID_GAIN, DRY_WET = 9,
           SENSITIVITY, CUTOFF, RESONANCE, RELEASE };
    const uint16_t* P = param[w];
    const bool overdrive = code[w].lsb == 2;
    switch (p) {
        case DELAY: {
            const int32_t d = toSamples(P[DELAY]) + delay[ins::WD_SL[0]];
            for (int i = 1; i <= 4; i++)
                delay[ins::WD_SL[i]] = d;
            writeDelays(4, ins::WD_SL + 1);
            break;
        }
        case FB_LEVEL:
            coef[ins::WD_FB[0]] = bipolar500(P[FB_LEVEL]);
            writeCoefs(1, ins::WD_FB);
            break;
        case DELAY_MIX:
            coef[ins::WD_LEVEL[2]] = coef[ins::WD_LEVEL[0]] = int(8192.0f * rom::LEVEL_CURVE[P[DELAY_MIX]]);
            writeCoefs(1, ins::WD_LEVEL);
            writeCoefs(1, ins::WD_LEVEL + 2);
            break;
        case DRIVE: case OUTPUT_LEVEL: case DS_LOW_GAIN: case DS_MID_GAIN: case DRY_WET: {
            // the distortion's tone with the type's frequencies, then the drive and the dry and wet
            const uint32_t lsb = code[w].lsb;
            if (lsb == 1 || lsb == 2) {
                designFilter(1, 1, HPF1, overdrive ? 8 : 0, 64, 0, ins::WD_HPF_2);
                designFilter(1, 1, LPF1, overdrive ? 44 : 54, 64, 0, ins::WD_LPF_2);
                designFilter(1, 1, LPF1, overdrive ? 45 : 53, 64, 0, ins::WD_LPF_3);
                designFilter(1, 2, LOW_SHELF1, overdrive ? 24 : 20, int8_t(P[DS_LOW_GAIN]), 0, ins::WD_LOW_SHELF_2);
                designFilter(2, 1, PEAK, overdrive ? 41 : 43, int8_t(P[DS_MID_GAIN]), 10, ins::WD_PEAK);
                storeEdge(overdrive ? 104 : 120, ins::WD_BAND);
            }
            wahDistortionWet(*this, ins::WD_WET, P[DRIVE], P[OUTPUT_LEVEL], 1, ins::WD_DRIVE, ins::WD_MODE, ins::WD_MODE2);
            break;
        }
        case SENSITIVITY:
            coef[ins::WD_ATTACK[0]] = P[SENSITIVITY] * 258;
            writeCoefs(1, ins::WD_ATTACK);
            [[fallthrough]];                        // the firmware runs on into the resonance
        case RESONANCE:
            coef[ins::WD_SENSE[0]] = P[RESONANCE] <= 10 ? MINUS_ONE : int16_t(int(-327680.0 / double(float(P[RESONANCE]))));
            writeCoefs(1, ins::WD_SENSE);
            break;
        case CUTOFF:
            coef[ins::WD_CUTOFF[0]] = wahCutoff(P[CUTOFF], 7483);   // the firmware's own base for this wah
            writeCoefs(1, ins::WD_CUTOFF);
            break;
        case RELEASE:
            storeWahRelease(releaseStep(P[RELEASE]), ins::WD_MIRROR);
            break;
        case ALL:
            hold = true;
            for (int q : {DELAY, FB_LEVEL, DELAY_MIX, DRIVE, SENSITIVITY, CUTOFF, RESONANCE, RELEASE})
                wahDistortionDelay(w, q);
            designFilter(1, 1, HPF1, 0, 64, 0, ins::WD_HPF);
            designFilter(1, 0, LPF1, 54, 64, 0, ins::WD_LPF);
            designFilter(1, 1, LOW_SHELF1, 28, 66, 0, ins::WD_LOW_SHELF);
            copyCoefs(3, ins::WD_LOW_SHELF, ins::WD_LOW_SHELF_COPY);
            designFilter(1, 2, HIGH_SHELF1, 46, 64, 0, ins::WD_HIGH_SHELF);
            copyCoefs(3, ins::WD_HIGH_SHELF, ins::WD_HIGH_SHELF_COPY);
            designFilter(1, 1, LPF1, 60, 64, 0, ins::WD_LPF_4);
            hold = false;
            break;
    }
}

// Cmp+DS+Dly and Cmp+OD+Dly: a compressor into a distortion into a delay
void EffectHost::compressorDistortionDelay(int w, int p)
{
    enum { DELAY, FB_LEVEL, DELAY_MIX, DRIVE, OUTPUT_LEVEL, DS_LOW_GAIN, DS_MID_GAIN, DRY_WET = 9,
           ATTACK, RELEASE, THRESHOLD, RATIO };
    const uint16_t* P = param[w];
    const bool overdrive = code[w].lsb == 2;
    switch (p) {
        case DELAY: {
            const int32_t d = toSamples(P[DELAY]) + delay[ins::DD_SLOTS[0]];
            for (int i = 1; i <= 4; i++)
                delay[ins::DD_SLOTS[i]] = d;
            writeDelays(4, ins::DD_SLOTS + 1);
            break;
        }
        case FB_LEVEL:
            coef[ins::DD_FB[0]] = bipolar500(P[FB_LEVEL]);
            writeCoefs(1, ins::DD_FB);
            break;
        case DELAY_MIX:
            coef[ins::DD_LEVEL[2]] = coef[ins::DD_LEVEL[0]] = int(8192.0f * rom::LEVEL_CURVE[P[DELAY_MIX]]);
            writeCoefs(1, ins::DD_LEVEL);
            writeCoefs(1, ins::DD_LEVEL + 2);
            break;
        case DRIVE: case OUTPUT_LEVEL: case DS_LOW_GAIN: case DS_MID_GAIN: {
            const uint32_t lsb = code[w].lsb;
            if (lsb == 1 || lsb == 2) {
                designFilter(2, 1, HPF1, overdrive ? 8 : 0, 64, 0, ins::DD_HPF);
                designFilter(1, 1, LPF1, overdrive ? 44 : 54, 64, 0, ins::DD_LPF);
                designFilter(2, 1, LPF2, overdrive ? 45 : 53, 64, 7, ins::DD_LPF2);
                designFilter(2, 4, LOW_SHELF1, overdrive ? 24 : 20, int8_t(P[DS_LOW_GAIN]), 0, ins::DD_LOW_SHELF);
                designFilter(2, 1, PEAK, overdrive ? 41 : 43, int8_t(P[DS_MID_GAIN]), 10, ins::DD_PEAK);
                storeEdge(overdrive ? 104 : 120, ins::DD_BAND);
            }
            storeDrive(P[DRIVE], P[OUTPUT_LEVEL], ins::DD_DRIVE);
            break;
        }
        case DRY_WET:
            coef[ins::DD_MODE[0]] = 5792;
            coef[ins::DD_MODE[2]] = 14481;
            writeCoefs(1, ins::DD_MODE);
            writeCoefs(1, ins::DD_MODE + 2);
            dryWet(w, ins::DD_WET, true, rom::PAN_LAW, 1);
            compressorDistortionDelay(w, DELAY);
            break;
        case ATTACK:    storeCompAttack(P[ATTACK], ins::DD_THRESH);       break;
        case RELEASE:   storeRelease(P[RELEASE], ins::DD_PAIR);           break;
        case THRESHOLD: storeCompThreshold(P[THRESHOLD], ins::DD_WORD);   break;
        case RATIO:     storeRatio(P[RATIO], ins::DD_ROW);                break;
        case ALL:
            hold = true;
            for (int q : {DELAY, FB_LEVEL, DELAY_MIX, DRIVE, DRY_WET, ATTACK, RELEASE, THRESHOLD, RATIO})
                compressorDistortionDelay(w, q);
            designFilter(1, 1, LPF1, 60, 64, 0, ins::DD_LPF_2);
            hold = false;
            break;
    }
}

// Dist+Delay and Odrv+Delay
void EffectHost::distortionDelay(int w, int p)
{
    enum { LCH_DELAY, RCH_DELAY, FB_DELAY, FB_LEVEL, DELAY_MIX, DRIVE, OUTPUT_LEVEL, DS_LOW_GAIN, DS_MID_GAIN, DRY_WET };
    const uint16_t* P = param[w];
    const uint8_t* s = ins::DD_SLOTS;
    const bool overdrive = code[w].lsb == 4;
    switch (p) {
        case LCH_DELAY:
            delay[s[4]] = delay[s[3]] = toSamples(P[LCH_DELAY]) + delay[s[0]];
            writeDelays(2, s + 3);
            break;
        case RCH_DELAY:
            delay[s[2]] = toSamples(P[RCH_DELAY]) + delay[s[0]];
            writeDelays(1, s + 2);
            break;
        case FB_DELAY:
            delay[s[1]] = toSamples(P[FB_DELAY]) + delay[s[0]];
            writeDelays(1, s + 1);
            break;
        case FB_LEVEL:
            coef[ins::DD_FB[0]] = bipolar500(P[FB_LEVEL]);
            writeCoefs(1, ins::DD_FB);
            break;
        case DELAY_MIX:
            coef[ins::DD_LEVEL[2]] = coef[ins::DD_LEVEL[0]] = int(8192.0f * rom::LEVEL_CURVE[P[DELAY_MIX]]);
            writeCoefs(1, ins::DD_LEVEL);
            writeCoefs(1, ins::DD_LEVEL + 2);
            break;
        case DRIVE: case OUTPUT_LEVEL:
            storeDrive(P[DRIVE], P[OUTPUT_LEVEL], ins::DD_DRIVE);
            break;
        case DS_LOW_GAIN:
            designFilter(2, 4, LOW_SHELF1, overdrive ? 24 : 20, int8_t(P[DS_LOW_GAIN]), 0, ins::DD_LOW_SHELF);
            break;
        case DS_MID_GAIN:
            designFilter(2, 1, PEAK, overdrive ? 41 : 43, int8_t(P[DS_MID_GAIN]), 10, ins::DD_PEAK);
            break;
        case DRY_WET:
            coef[ins::DD_MODE[0]] = 1448;
            coef[ins::DD_MODE[1]] = 0;
            writeCoefs(2, ins::DD_MODE);
            dryWet(w, ins::DD_WET, true, rom::PAN_LAW, 1);
            distortionDelay(w, LCH_DELAY);
            break;
        case ALL:
            hold = true;
            distortionGate(*this, ins::DD_THRESH, ins::DD_PAIR, ins::DD_WORD);
            designFilter(2, 1, HPF1, overdrive ? 8 : 0, 64, 0, ins::DD_HPF);
            designFilter(1, 1, LPF1, overdrive ? 44 : 54, 64, 0, ins::DD_LPF);
            designFilter(2, 1, LPF2, overdrive ? 45 : 53, 64, 7, ins::DD_LPF2);
            storeEdge(overdrive ? 104 : 120, ins::DD_BAND);
            for (int q : {LCH_DELAY, RCH_DELAY, FB_DELAY, FB_LEVEL, DELAY_MIX, DRIVE, DS_LOW_GAIN, DS_MID_GAIN, DRY_WET})
                distortionDelay(w, q);
            designFilter(1, 1, LPF1, 60, 64, 0, ins::DD_LPF_2);
            hold = false;
            break;
    }
}

}  // namespace fs1r::vop3host
