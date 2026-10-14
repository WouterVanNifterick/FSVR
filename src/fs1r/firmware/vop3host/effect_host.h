// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The FS1R's effect host: what the CPU does to turn a performance into register writes for
// the effect chip. Each function carries its address in the firmware. `p` is a parameter's
// number within its block, and 255 stands for all of them.
#pragma once
#include <cfloat>
#include <cstdint>
#include <initializer_list>

#include "rom_tables.h"
#include "sink.h"

// The arithmetic is the firmware's, bit for bit, as long as nothing is evaluated in a wider
// type and a * b + c is not fused: build with /fp:strict or -ffp-contract=off.
static_assert(FLT_EVAL_METHOD == 0, "the effect host needs float and double arithmetic without excess precision");

namespace fs1r::vop3host {

// The chip's five program windows, then the two parameter groups that have none.
enum { REVERB, AUX, INSERTION, REVERB_DELAY, VARIATION, MASTER_EQ, MIXER };
constexpr uint32_t AREA[5] = {0, 0xFFFF, 0x10000, 0x20000, 0x30000};   // where each window's delay memory starts
struct TypeCode { uint8_t msb, lsb; };                                  // a type as the chip programs are filed

// One window of the chip's program: its steps, its delay slots and its LFOs, and what the EPROM holds for it.
struct Window {
    const uint16_t* steps;  int nsteps;
    const uint8_t* slots;  int nslots;
    const uint8_t* lfos = nullptr;  uint32_t lfoMask = 0;    // four LFOs and their phase reset bits; the reverb has none
    const uint16_t (*programs)[5] = nullptr;                 // per type msb: five words for each step,
    const uint8_t* tags = nullptr;                           // and a tag
    int clearEntry = 0;                                      // the program that silences the window
};
extern const Window WINDOWS[5];

struct EffectHost {
    WriteSink* sink = nullptr;

    void init();                                                        // 0x3AAC8: the chip and every window as the firmware starts them
    // A performance byte enters here: seven bits, and no effect type past the last one.
    void store(uint32_t index, uint32_t value)
    {
        if (index >= 192) return;
        value &= 0x7F;
        if (index == 168 && value > 16) value = 16;
        if (index == 171 && value > 28) value = 28;
        if (index == 175 && value > 40) value = 40;
        perf[index] = uint8_t(value);
    }
    // A performance's 192 common bytes, loaded as the firmware loads a performance.
    void load(const uint8_t* bytes)
    {
        for (uint32_t i = 0; i < 192; i++)
            store(i, bytes[i]);
        setSendVariationOffset(0);
        setSendReverbOffset(0);
        applyChanges(0x8001);
        runPosted();
    }
    // One byte changed. For a two-byte parameter: its first byte, after both were written.
    void setByte(uint32_t index, uint32_t value)
    {
        store(index, value);
        byteChanged(index);
        runPosted();
    }
    void byteChanged(uint32_t index);                                   // 0x3C4A8: reads the byte and posts what has to run
    bool runPosted();                                                   // 0x3C994: one turn of the effect task
    void setControlOffset(uint32_t i, uint32_t v);                      // 0x3C8D4: a controller's offset on insertion parameter i
    void setSendVariationOffset(uint32_t v);                            // 0x3C8EC
    void setSendReverbOffset(uint32_t v);                               // 0x3C8FE
    int32_t readParameter(uint32_t index);                              // 0x30390: a performance byte's value, clamped to its range
    const rom::ParamRange& descriptor(uint32_t index) const;            // 0x34478: its range under the current types

    // ---- state ----
    uint8_t perf[192] = {};         // the performance's common bytes
    uint32_t posted = 0;            // what was posted to the effect task and not handled yet
    bool hold = false;              // while set, changes go to the shadows and nothing is written to the chip
    uint16_t coef[512] = {};        // each step's constant, register 0x0B
    uint8_t tag[512] = {};          // each step's tag, register 0x0C
    int32_t delay[128] = {};        // each delay slot's offset inside its area
    uint16_t lfoReg[4][16] = {};    // registers 0x24..0x27 of the sixteen LFOs: speed, wave and phase, depth, direction
    TypeCode code[5] = {};          // each window's type
    uint8_t mute[6] = {};           // nonzero silences the five windows and the output
    uint16_t masterLevel = 0;       // the master volume's scale
    uint16_t param[5][16] = {};     // each window's parameters, as its handler reads them
    uint16_t eqParam[20] = {}, mixParam[16] = {};   // the master EQ's and the mixer's
    // The parameters as last read from the performance, and which of them are waiting to be applied.
    uint16_t reverbCache[16] = {}, insertionCache[16] = {}, variationCache[16] = {}, eqCache[10] = {};
    uint8_t reverbChanged[16] = {}, variationChanged[16] = {}, insertionChanged[16] = {}, eqChanged[11] = {}, mixerChanged[21] = {};
    int16_t controlOffset[16] = {};                          // a controller's offset on each insertion parameter
    int16_t sendVariationOffset = 0, sendReverbOffset = 0;   // and on the insertion's two sends

    // ---- chip registers ----
    void writeReg(uint32_t reg, uint32_t value);                        // 0x39648
    void writeCoefs(uint32_t n, const uint16_t* steps);                 // 0x39652: the constants of n listed steps
    void writeCoefsBurst(uint32_t n, const uint16_t* steps);            // 0x396A6: of n steps in a row, from the first listed
    void writeTags(uint32_t n, const uint16_t* steps);                  // 0x396FC: the tags of n listed steps
    void writeTagsBurst(uint32_t n, const uint16_t* steps);             // 0x39764
    void writeConfig(uint32_t index, uint32_t value);                   // 0x397BA: register 0x16, entry `index`
    void writeDelays(uint32_t n, const uint8_t* slots);                 // 0x397F4: the offsets of n listed delay slots
    void writeLfo(uint32_t reg, uint32_t lfo);                          // 0x39B24..0x39BB4: an LFO's register 0x24..0x27
    void resetLfoPhase(uint32_t mask);                                  // 0x39C74
    void uploadProgram(int window, int clear);                          // 0x39C8A: a window's program words and tags
    void uploadWindow(int kind);                                        // 0x398C2: its program, constants, delays and LFOs
    void uploadAll();                                                   // 0x3A110
    void initChip();                                                    // 0x3A064
    void setMute(int window, uint32_t on);                              // 0x3A140: 5 is the output, anything else all six
    void setOutputGain();                                               // 0x3BCD0

    // ---- the performance into the chip ----
    void applyChanges(uint32_t mask);                                   // 0x3AC88: what was posted; 0x8000 is a whole performance
    void post(uint32_t mask);                                           // 0x9FA4
    void applyMixer(uint32_t i);                                        // the handler table at 0x3F0B0
    void applyEq(uint32_t i);                                           // the handler table at 0x3F104
    void applyReverbPanReturn();                                        // 0x3B766
    void applyVariationPan();                                           // 0x3B81C
    void applyVariationReturn();                                        // 0x3B82E
    void applyVariationToReverb();                                      // 0x3B840
    void applyInsertionPan();                                           // 0x3B852
    void applyVolume();                                                 // 0x3B864
    void applyInsertionToReverb();                                      // 0x3B8BC
    void applyInsertionToVariation();                                   // 0x3B8EC
    void applyInsertionLevel();                                         // 0x3B91C
    void applyIndividualOut();                                          // 0x3B92C
    void applyEqLow(uint32_t i);                                        // 0x3B940
    void applyEqMid(uint32_t i);                                        // 0x3B95C
    void applyEqHigh(uint32_t i);                                       // 0x3B97E
    void copyEqParams();                                                // 0x3B9CC
    // The firmware keeps ten EQ words where it uses eleven: the eleventh lies on the first two reverb flags.
    int eqHighShape() const { return reverbChanged[0] << 8 | reverbChanged[1]; }
    void setInsertionToReverb(uint32_t v);                              // 0x3BA2C
    void setInsertionToVariation(uint32_t v);                           // 0x3BA8E
    void setInsertionLevel(uint32_t v);                                 // 0x3BB30
    void setIndividualOut(uint32_t mode);                               // 0x3BBBA
    void setMasterLevel();                                              // 0x3BCA0
    void loadTypeDefaults(uint32_t window);                             // 0x3C1D2: the default parameters of its current type
    void reverbTypeChanged();                                           // 0x3BDFC
    void variationTypeChanged();                                        // 0x3C018
    void insertionTypeChanged();                                        // 0x3C118

    // ---- parameters to their handlers ----
    void applyParam(int kind, int p);                                   // 0x39B09C: p from 252 reloads the type's preset first
    const rom::PresetGroup& presets(int window) const;                  // of its current type msb
    void dispatch(int kind, int p);                                     // 0x39C0B2
    void dispatchReverb(int p);                                         // 0x39C144
    void dispatchInsertion(int p);                                      // 0x39C1AC
    void dispatchReverbDelay(int p);                                    // 0x39C48A
    void dispatchVariation(int p);                                      // 0x39C51C
    void clearWindow(int window);                                       // 0x3A3308
    void masterEq(int p);                                               // 0x3A356C
    void mixerParam(int p);                                             // 0x3A2EC

    // ---- filter, comb and room design ----
    // designFilter writes b0, b1(, b2), -a1(, -a2) into the constants of the listed steps.
    void designFilter(int order, int scale, int kind, int freq, int gain, int q, const uint16_t* steps);   // 0x37414
    void designCombs(int time, int damp, int count, int sel, const uint16_t* list);                        // 0x39C796
    void designRoom(int time, int y0, int y1, int damp, int extra, const uint16_t* list);                  // 0x39CF82
    void designTypeShelf(int index, const uint16_t* steps);                                                // 0x39C758
    void roomDelays(int clampSrc, int index, int span, const uint8_t* ids);                                // 0x39D2C4
    void combDelays(int clampSrc, int count, int sel, const uint8_t* ids);                                 // 0x39C8F4
    void combDelays2(int clampSrc, int sel, const uint8_t* ids);                                           // 0x3A259C
    void diffusers(int src, int scale, int offset, int count, int stride, const uint16_t* steps, const uint8_t* slots);   // 0x39C9E8
    void earlyReflections(int indexA, int indexB, int offset, int row, const uint16_t* words, const uint8_t* ids);        // 0x39CC78
    void erBalance(int balance, const uint16_t* list);                  // 0x39CEC8
    void erPattern();                                                   // 0x3A6354: the insertion's 20 early reflections
    int32_t erGain(int x);                                              // 0x3A6338

    // ---- helpers ----
    int32_t bipolar500(int v);                                          // 0x39CF62
    int32_t bipolar376(int v);                                          // 0x39CF72
    int32_t lessDiffusion(int x, int v);                                // 0x39D468
    int32_t toSamples(int v);                                           // 0x39D47C
    int32_t scaleDelay(int v);                                          // 0x39D490
    int32_t typeGain(int window, int x);                                // 0x39D4A2..0x39D598: x at the level the window's type runs at
    void copyCoefs(int n, const uint16_t* src, const uint16_t* dst);    // 0x39390, 0x3943C, 0x394BA
    void setLfoSpeed(int v, int lfo);                                   // 0x3A3070
    void setLfoPhase(int v, int lfo);                                   // 0x3A30C2
    void setLfoDepth(int v, int lfo);                                   // 0x3A3128
    // one parameter into its steps
    void storeBipolar(int scale, int v, const uint16_t* step);          // 0x3A31CC, 0x3A31EA: a phase difference
    void storeModePair(int mode, const uint16_t* steps);                // 0x3A3208
    void storeDepth(int v, const uint16_t* step);                       // 0x3A328E
    void storeNegDepth(int v, const uint16_t* step);                    // 0x3A32B0
    void storeHighDamp(int scale, int v, int gain, const uint16_t* steps);   // 0x39108
    void storeDrive(int v, int level, const uint16_t* steps);           // 0x3A4F00
    void storeEdge(int v, const uint16_t* steps);                       // 0x394F8
    void storeCompAttack(int v, const uint16_t* steps);                 // 0x3A5090
    void storeGateAttack(int v, const uint16_t* steps);                 // 0x3A50D8
    void storeRelease(int v, const uint16_t* steps);                    // 0x3A5124
    void storeCompThreshold(int v, const uint16_t* step);               // 0x3A5168
    void storeGateThreshold(int v, const uint16_t* step);               // 0x3A51E4
    void storeRatio(int row, const uint16_t* steps);                    // 0x3A522E
    void storeWahRelease(int v, const uint16_t* steps);                 // 0x3ACA08
    // the pitch change's two voices
    int32_t pitchRatio(int v);                                          // 0x3A99FC
    void pitchVoice(int lfo, int v);                                    // 0x3A9B8A
    void pitchSlots1();                                                 // 0x3A9CC6
    void pitchSlots2(int v);                                            // 0x3A9D58

    // ---- a handler per type: reverb 1-12, then 13-16 ----
    void fxRevHall(int p);                                              // 0x3A3728
    void fxRevRoom(int p);                                              // 0x3A3C54
    void fxRevDelayLCR(int p);                                          // 0x39D5E0
    void fxRevDelayLR(int p);                                           // 0x39D964
    void fxRevEcho(int p);                                              // 0x39DC98
    void fxRevCrossDelay(int p);                                        // 0x39E074
    // variation
    void fxVarChorus(int p); void fxVarFlanger(int p); void fxVarSymphonic(int p);
    void fxVarPhaser1(int p); void fxVarPhaser2(int p); void fxVarEnsDetune(int p);
    void fxVarRotarySp(int p); void fxVarTremolo(int p); void fxVarAutoPan(int p);
    void fxVarAutoWah(int p); void fxVarTouchWah(int p); void fxVar3BandEq(int p);
    void fxVarHmEnhancer(int p); void fxVarCompressor(int p); void fxVarNoiseGate(int p);
    void fxVarDistortion(int p); void fxVarAmpSim(int p);
    void fxVarDelayLCR(int p); void fxVarDelayLR(int p); void fxVarEcho(int p); void fxVarCrossDelay(int p);
    void fxVarKaraoke(int p); void fxVarReverb(int p);
    // insertion
    void fxInsChorus(int p); void fxInsFlanger(int p); void fxInsSymphonic(int p);
    void fxInsDelayLR(int p); void fxInsDelayLCR(int p); void fxInsEcho(int p); void fxInsEr1(int p);
    void fxInsRotarySp(int p); void fxIns2WayRotary(int p); void fxInsTremolo(int p); void fxInsAutoPan(int p); void fxInsAmbience(int p);
    void fxInsPhaser1(int p); void fxInsPhaser2(int p);
    void fxInsCrossDelayEr(int p);
    void fxInsAutoWahDist(int p); void fxInsTouchWahDist(int p);
    void fxInsDistortion(int p); void fxInsAmpSim(int p); void fxInsCompDist(int p); void fxInsCompDistDelay(int p);
    void fxInsHmEnhancer(int p); void fxInsNoiseGate(int p);
    void fxInsPitchChange(int p); void fxInsEnsDetune(int p); void fxIns3BandEq(int p);
    void fxInsLoFi(int p); void fxInsWahDistDelay(int p); void fxInsCompressor(int p); void fxInsDistDelay(int p);
};

}  // namespace fs1r::vop3host
