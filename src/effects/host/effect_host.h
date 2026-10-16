// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The FS1R's effect host: what the CPU does to turn a performance into register writes for
// the effect chip. `p` is a parameter's number within its block.
#pragma once
#include <cfloat>
#include <cstdint>
#include <initializer_list>
#include <iterator>

#include "fs1r/rom_tables.h"
#include "dsp/sink.h"

// The arithmetic is the firmware's, bit for bit, as long as nothing is evaluated in a wider
// type and a * b + c is not fused: build with -ffp-contract=off, or on MSVC with /fp:precise
// (VS 2022 17.0 and later contract only under /fp:contract or /fp:fast; older ones need /fp:strict).
static_assert(FLT_EVAL_METHOD == 0, "the effect host needs float and double arithmetic without excess precision");

namespace fsvr::vop3host {

// The five program windows, then the two parameter groups that have none.
enum { REVERB, AUX, INSERTION, REVERB_DELAY, VARIATION, MASTER_EQ, MIXER };
constexpr int OUTPUT = 5;                                               // setMute's sixth flag
constexpr uint32_t AREA[5] = {0, 0xFFFF, 0x10000, 0x20000, 0x30000};   // where each window's delay memory starts
constexpr uint32_t TOP = 0x40000;                                       // and where the last one ends
struct TypeCode { uint8_t msb, lsb; };                                  // a type as the chip programs are filed

// A handler's p for every parameter at once
enum { ALL = 255 };
constexpr int DRY_WET = 9;                                             // a type's Dry/Wet, where it has one

// What can be posted to the effect task. Every post carries WAKE.
enum : uint32_t {
    WAKE                   = 0x0001,
    INSERTION_CHANGED      = 0x0002,     // parameters, with their controller offsets
    INSERTION_TYPE_CHANGED = 0x0004,
    VARIATION_CHANGED      = 0x0008,
    VARIATION_TYPE_CHANGED = 0x0010,
    REVERB_CHANGED         = 0x0020,
    REVERB_TYPE_CHANGED    = 0x0040,
    EQ_CHANGED             = 0x0100,
    MIXER_CHANGED          = 0x0200,
    LOADED                 = 0x8000,     // a new performance: every window, the mixer and the master EQ
};

// The performance's common bytes the host reads, of COMMON_BYTES.
constexpr uint32_t COMMON_BYTES = 192;
enum : uint32_t {
    VOLUME                = 16,
    INDIVIDUAL_OUT        = 20,
    REVERB_PARAM_BYTES    = 0x50,        // reverb parameters 1-8, two bytes each, high first
    REVERB_PARAM_BYTES_9  = 0x60,        // reverb parameters 9-16, one byte each
    VARIATION_PARAM_BYTES = 0x68,        // two bytes each
    INSERTION_PARAM_BYTES = 136,         // two bytes each
    REVERB_TYPE           = 168, REVERB_PAN, REVERB_RETURN,
    VARIATION_TYPE, VARIATION_PAN, VARIATION_RETURN, VARIATION_TO_REVERB,
    INSERTION_TYPE, INSERTION_PAN, INSERTION_TO_REVERB, INSERTION_TO_VARIATION, INSERTION_LEVEL,
    EQ_LOW_GAIN, EQ_LOW_FREQ, EQ_LOW_Q, EQ_LOW_SHAPE, EQ_MID_GAIN, EQ_MID_FREQ, EQ_MID_Q,
    EQ_HIGH_GAIN, EQ_HIGH_FREQ, EQ_HIGH_Q, EQ_HIGH_SHAPE,
};

// The mixer's parameters (mixParam) that have a name. Past them are the flags only mixerChanged
// has: the insertion's two sends, its level and the individual outputs.
enum {
    MIX_REVERB_PAN          = 0,
    MIX_REVERB_RETURN       = 1,
    MIX_AUX_PAN             = 2,
    MIX_DELAY_RETURN        = 3,     // the reverb's when its type runs in the delay window
    MIX_INSERTION_PAN       = 5,
    MIX_VARIATION_RETURN    = 6,
    MIX_VARIATION_TO_REVERB = 8,
    MIX_DELAY_PAN           = 10,
    MIX_VARIATION_PAN       = 11,
    MIX_VOLUME              = 14,
    MIX_INSERTION_TO_REVERB = 16, MIX_INSERTION_TO_VARIATION, MIX_INSERTION_LEVEL, MIX_INDIVIDUAL_OUT,
};

// designFilter's shapes
enum Shape { LPF1, HPF1, LOW_SHELF1, HIGH_SHELF1, LPF2, HPF2, LOW_SHELF2, HIGH_SHELF2, PEAK = 10, GAIN_ONLY = 11 };

// Individual out
enum { OUT_OFF, OUT_PRE_INSERTION, OUT_POST_INSERTION };

// One window: the steps of its constants, its delay slots and its LFOs.
struct Window {
    const uint16_t* steps;  int nsteps;
    const uint8_t* slots;  int nslots;
    const uint8_t* lfos = nullptr;  uint32_t lfoMask = 0;    // four LFOs and their phase reset bits; the reverb has none
    int clearEntry = 0;                                      // the program that silences the window
};
extern const Window WINDOWS[5];

// A pair of shelving filters, a low and a high one, and the other channel's copies of their words
struct Shelves { const uint16_t *low, *lowCopy, *high, *highCopy; int order = 1, highScale = 2; };

struct EffectHost : ChipHost {
    void init();                                                        // the chip and every window as the firmware starts them
    // A performance byte enters here: seven bits, and no effect type past the last one.
    void store(uint32_t index, uint32_t value)
    {
        if (index >= COMMON_BYTES) return;
        value &= 0x7F;
        if (index == REVERB_TYPE    && value > 16) value = 16;
        if (index == VARIATION_TYPE && value > 28) value = 28;
        if (index == INSERTION_TYPE && value > 40) value = 40;
        perf[index] = uint8_t(value);
    }
    // A performance's common bytes, loaded as the firmware loads a performance.
    void load(const uint8_t* bytes)
    {
        for (uint32_t i = 0; i < COMMON_BYTES; i++)
            store(i, bytes[i]);
        setSendVariationOffset(0);
        setSendReverbOffset(0);
        applyChanges(WAKE | LOADED);
        runPosted();
    }
    // One byte changed. For a two-byte parameter: its first byte, after both were written.
    void setByte(uint32_t index, uint32_t value)
    {
        store(index, value);
        byteChanged(index);
        runPosted();
    }
    void byteChanged(uint32_t index);                                   // reads the byte and posts what has to run
    bool runPosted();                                                   // one turn of the effect task
    void setControlOffset(uint32_t i, uint32_t v);                      // a controller's offset on insertion parameter i
    void setSendVariationOffset(uint32_t v);
    void setSendReverbOffset(uint32_t v);
    int32_t readParameter(uint32_t index);                              // a performance byte's value, clamped to its range
    const rom::ParamRange& descriptor(uint32_t index) const;            // its range under the current types

    // ---- state ----
    uint8_t  perf[COMMON_BYTES] = {};    // the performance's common bytes
    uint32_t posted         = 0;     // what was posted to the effect task and not handled yet
    bool     hold           = false; // while set, changes go to the shadows and nothing is written to the chip
    uint16_t coef[512]      = {};    // each step's COEF
    int32_t  delay[128]     = {};    // each delay slot's offset inside its area
    TypeCode code[5]        = {};    // each window's type
    uint8_t  mute[6]        = {};    // nonzero silences the five windows and the OUTPUT
    uint16_t masterLevel    = 0;     // the master volume's scale
    uint16_t param[5][16]   = {};    // each window's parameters, as its handler reads them
    uint16_t eqParam[20]    = {};    // the master EQ's
    uint16_t mixParam[16]   = {};    // and the mixer's

    // The parameters as last read from the performance, and which of them are waiting to be applied.
    uint16_t reverbCache[16] = {}, insertionCache[16] = {}, variationCache[16] = {}, eqCache[10] = {};
    uint8_t  reverbChanged[16] = {}, variationChanged[16] = {}, insertionChanged[16] = {}, eqChanged[11] = {}, mixerChanged[21] = {};
    int16_t  controlOffset[16] = {};                          // a controller's offset on each insertion parameter
    int16_t  sendVariationOffset = 0, sendReverbOffset = 0;   // and on the insertion's two sends

    // ---- chip registers ----
    void writeCoefs(uint32_t n, const uint16_t* steps);                 // the constants of n listed steps
    void writeCoefsBurst(uint32_t n, const uint16_t* steps);            // of n steps in a row, from the first listed
    void writeDelays(uint32_t n, const uint8_t* slots);                 // the offsets of n listed delay slots
    void writeLfo(Reg reg, uint32_t lfo);                               // ChipHost's, unless held
    void uploadProgram(int window, int clear);                          // a window's program, or the one that silences it
    void uploadWindow(int kind);                                        // its program, constants, delays and LFOs
    void uploadAll();
    void initChip();
    void setMute(int window, uint32_t on);                              // a window or the OUTPUT; anything else all six
    void setOutputGain();

    // ---- the performance into the chip ----
    void applyChanges(uint32_t mask);                                   // what was posted
    void post(uint32_t mask);
    void applyMixer(uint32_t i);                                        // one mixer change, by its flag
    void applyEq(uint32_t i);                                           // one master EQ change, by its flag
    void setMixer(int m, uint32_t v);                                   // a mixer parameter, applied
    void setReverbPanReturn(bool delay, uint32_t pan, uint32_t ret);
    void copyEqParams();
    // The firmware keeps ten EQ words where it uses eleven: the eleventh lies on the first two reverb flags.
    int eqHighShape() const { return reverbChanged[0] << 8 | reverbChanged[1]; }
    void setInsertionSend(int to, uint32_t v);                          // to REVERB or VARIATION
    void setInsertionLevel(uint32_t v);
    void setIndividualOut(uint32_t mode);
    void setMasterLevel();
    void loadTypeDefaults(uint32_t window);                             // the default parameters of its current type
    void reverbTypeChanged();
    void variationTypeChanged();
    void insertionTypeChanged();

    // ---- parameters to their handlers ----
    void applyParam(int kind, int p);                                   // one parameter, or ALL, to the kind's handler
    void resetWindow(int kind, bool defaults);
    const rom::PresetGroup& presets(int window) const;                  // of its current type msb
    void clearWindow(int window);
    void masterEq(int p);
    void mixerParam(int p);

    // ---- filter, comb and room design ----
    // designFilter writes b0, b1(, b2), -a1(, -a2) into the constants of the listed steps.
    void designFilter(int order, int scale, int kind, int freq, int gain, int q, const uint16_t* steps);
    void designCombs(int time, int damp, int count, int sel, const uint16_t* list);
    void designRoom(int time, int y0, int y1, int damp, int extra, const uint16_t* list);
    void designTypeShelf(int index, const uint16_t* steps);
    void roomDelays(int clampSrc, int index, int span, const uint8_t* ids);
    void combDelays(int clampSrc, int count, int sel, const uint8_t* ids);
    void combDelays2(int clampSrc, int sel, const uint8_t* ids);
    void erTaps(int indexA, int indexB, int offset, int row);           // the reverb's taps
    void erBalance(int balance);                                        // and its ER/Rev balance
    void diffusers(int src, int scale, int offset, int count, int stride, const uint16_t* steps, const uint8_t* slots);

    // ---- shared by the handlers ----
    using Handler = void (EffectHost::*)(int w, int p);
    int lfo(int w, int i) const { return WINDOWS[w].lfos[i]; }        // the window's i-th LFO
    void applyAll(int w, Handler handler, std::initializer_list<int> ps);   // into the shadows only, for uploadWindow
    void dryWet(int w, const uint16_t* steps, bool interleaved = false, const int16_t* law = rom::PAN_LAW, int shift = 2);
    void shelves(int w, int p, int first, const Shelves& s);           // parameters first..first + 3
    void setLfoWave(int lfo, int wave);                                 // into the shadow only

    // ---- helpers ----
    int32_t bipolar500(int v);
    int32_t bipolar376(int v);
    int32_t lessDiffusion(int x, int v);
    int32_t toSamples(int v);
    int32_t scaleDelay(int v);
    int32_t typeGain(int window, int x);                                // x at the level the window's type runs at
    void copyCoefs(int n, const uint16_t* src, const uint16_t* dst);
    void setLfoSpeed(int v, int lfo);
    void setLfoPhase(int v, int lfo);
    void setLfoDepth(int v, int lfo);
    // one parameter into its steps
    void storeBipolar(int scale, int v, const uint16_t* step);          // a phase difference
    void storeModePair(int mode, const uint16_t* steps);
    void storeDepth(int v, const uint16_t* step);
    void storeNegDepth(int v, const uint16_t* step);
    void storeHighDamp(int scale, int v, const uint16_t* steps);
    void storeDrive(int v, int level, const uint16_t* steps);
    void storeEdge(int v, const uint16_t* steps);
    void storeCompAttack(int v, const uint16_t* steps);
    void storeGateAttack(int v, const uint16_t* steps);
    void storeRelease(int v, const uint16_t* steps);
    void storeCompThreshold(int v, const uint16_t* step);
    void storeGateThreshold(int v, const uint16_t* step);
    void storeRatio(int row, const uint16_t* steps);
    void storeWahRelease(int v, const uint16_t* steps);

    // ---- a handler per effect, for each window it runs in ----
    // modulation
    void chorus(int w, int p);         void flanger(int w, int p);        void symphonic(int w, int p);
    void phaser1(int w, int p);        void phaser2(int w, int p);        void ensembleDetune(int w, int p);
    void rotarySpeaker(int w, int p);  void tremolo(int w, int p);        void autoPan(int w, int p);
    void twoWayRotary(int w, int p);   void ambience(int w, int p);       void pitchChange(int w, int p);
    // delay
    void delayLCR(int w, int p);       void delayLR(int w, int p);        void echo(int w, int p);
    void crossDelay(int w, int p);     void karaoke(int w, int p);
    // reverb
    void hall(int w, int p);           void room(int w, int p);           void variationReverb(int w, int p);
    void earlyReflections(int w, int p);
    // drive
    void autoWah(int w, int p);        void touchWah(int w, int p);       void distortion(int w, int p);
    void ampSimulator(int w, int p);   void enhancer(int w, int p);       void compressor(int w, int p);
    void noiseGate(int w, int p);      void compressorDistortion(int w, int p);
    void wahDistortionDelay(int w, int p);   void compressorDistortionDelay(int w, int p);   void distortionDelay(int w, int p);
    // tone
    void threeBandEq(int w, int p);    void loFi(int w, int p);
};

}  // namespace fsvr::vop3host
