// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The FS1R's effect host: what the CPU does to turn a performance into register writes for
// the effect chip. Each function carries its address in the firmware; `p` is a parameter index.
#pragma once
#include <cfloat>
#include <cstdint>
#include <initializer_list>

#include "rom_tables.h"

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
    const uint8_t* lfos;  uint32_t lfoMask;                  // four LFOs and their phase reset bits; the reverb has none
    const uint16_t (*programs)[5];  const uint8_t* tags;     // per type msb: five words and a tag for each step
    int clearEntry;                                          // the program that silences the window
};
extern const Window WINDOWS[5];

struct WriteSink {              // where the register writes go
    virtual void write(uint32_t reg, uint32_t value) = 0;
};

class Host {
public:
    WriteSink* sink = nullptr;
    uint32_t fx_flags = 0;      // what was posted to the effect task and not handled yet
    bool hold = false;          // while set, changes go to the shadows and nothing is written to the chip
    uint16_t coef[512] = {};    // each step's constant, register 0x0B
    int32_t delay[128] = {};    // each delay slot's offset inside its area
    uint8_t tag[512] = {};      // each step's tag, register 0x0C
    uint16_t lfoReg[4][16] = {};   // registers 0x24..0x27 of the sixteen LFOs: speed, wave and phase, depth, direction
    TypeCode code[5] = {};      // each window's type
    uint8_t mute[6] = {};       // nonzero silences the five windows and the output
    uint16_t masterLevel = 0;   // the master volume's scale
    uint8_t perf[192] = {};     // the performance's common bytes
    uint16_t param[5][16] = {}; // each window's parameters, as its handler reads them
    uint16_t eqParam[20] = {}, mixParam[16] = {};   // the master EQ's and the mixer's
    // The parameters as last read from the performance, and which of them are waiting to be applied.
    uint16_t reverbCache[16] = {}, insertionCache[16] = {}, variationCache[16] = {}, eqCache[10] = {};
    uint8_t reverbChanged[16] = {}, variationChanged[16] = {}, insertionChanged[16] = {}, eqChanged[11] = {}, mixerChanged[21] = {};
    int16_t controlOffset[16] = {};                 // a controller's offset on each insertion parameter
    int16_t sendVariationOffset = 0, sendReverbOffset = 0;   // and on the insertion's two sends

    // ---- chip registers ----
    void writeSweWordSub(uint32_t reg, uint32_t value);                 // 0x39648
    void writeSweWordListSub(uint32_t n, const uint16_t* steps);        // 0x39652: constants of n listed steps
    void writeSweWordListLatchSub(uint32_t n, const uint16_t* steps);   // 0x396A6: same with the auto-increment latch
    void writeSwe0CSub(uint32_t n, const uint16_t* steps);              // 0x396FC: tags of n listed steps
    void writeSweBytesSub(uint32_t n, const uint16_t* steps);           // 0x39764: tags, latched
    void writeSweIndexedSub(uint32_t index, uint32_t value);            // 0x397BA: register 0x16 entry
    void writeSweDeSub(uint32_t n, const uint8_t* slots);               // 0x397F4: delay offsets of n listed slots
    void loadSubEffectType(int kind);                                   // 0x398C2: upload a window with its LFOs
    void writeSwe24Sub(uint32_t index);                                 // 0x39B24..0x39BB4: LFO registers
    void writeSwe25Sub(uint32_t index);
    void writeSwe26Sub(uint32_t index);
    void writeSwe27Sub(uint32_t index);
    void writeSwe20Sub(uint32_t mask);                                  // 0x39C74: LFO phase reset
    void loadSubCoeffBlock(int kind, int clear);                        // 0x39C8A: program words and tags of a window
    void initSubDefaults();                                             // 0x3A064
    void loadAllSubEffects();                                           // 0x3A110
    void setSubFlag(int effect, uint32_t enabled);                      // 0x3A140
    void initSubEngine();                                               // 0x3AAC8
    void setSubGain8();                                                 // 0x3BCD0

    // ---- the performance into the chip ----
    void applySubDirtyMask(uint32_t mask);                              // 0x3AC88: 0x8000 loads the whole performance
    void subMixerHandler(uint32_t i);                                   // the table at flash 0x3F0B0
    void subEqHandler(uint32_t i);                                      // the table at flash 0x3F104
    void applySubReverbPanReturn();                                        // 0x3B766: reverb pan and return
    void applySubVarPan();                                              // 0x3B81C
    void applySubVarReturn();                                           // 0x3B82E
    void applySubVarToReverb();                                         // 0x3B840
    void applySubInsPan();                                              // 0x3B852
    void applySubPerfVolume();                                          // 0x3B864
    void applySubInsToReverb();                                             // 0x3B8BC
    void applySubInsToVariation();                                             // 0x3B8EC
    void applySubInsLevel();                                            // 0x3B91C
    void applySubIndividualOut();                                       // 0x3B92C
    void copySubEqLow(uint32_t i);                                      // 0x3B940
    void copySubEqMid(uint32_t i);                                     // 0x3B95C
    void copySubEqHigh(uint32_t i);                                     // 0x3B97E
    void copySubEqWords();                                              // 0x3B9CC
    int eqHighShape() const { return reverbChanged[0] << 8 | reverbChanged[1]; }
    void setSubInsToReverb(uint32_t v);                                     // 0x3BA2C: insertion to reverb send
    void setSubInsToVariation(uint32_t v);                                     // 0x3BA8E: insertion to variation send
    void setSubInsLevel(uint32_t v);                                   // 0x3BB30: insertion level
    void setSubIndividualOut(uint32_t mode);                                // 0x3BBBA: individual out
    void setSubMasterShift();                                           // 0x3BCA0
    // changes after the load
    void postSubMask(uint32_t mask);                                    // the set_flg(6, mask) call sites
    bool pollSubDirtyLoop();                                            // 0x3C994: one turn of the effect task
    void loadSubTypeDefaults(uint32_t kind);                               // 0x3C1D2: default parameters of the current type
    void syncSubReverbType();                                        // 0x3BDFC: reverb type change
    void syncSubVariationType();                                        // 0x3C018: variation type change
    void syncSubInsertionType();                                           // 0x3C118: insertion type change
    void markSubParamDirty(uint32_t id);                                // 0x3C4A8: a performance common byte changed
    void setSubInsCtlOffset(uint32_t i, uint32_t v);                         // 0x3C8D4: controller offset of insertion parameter i
    void setSubSendBOffset(uint32_t v);                                 // 0x3C8EC
    void setSubSendAOffset(uint32_t v);                                 // 0x3C8FE

    // ---- parameter dispatch ----
    void fxParamEntry(int kind, int p);                                 // 0x39B09C
    const rom::PresetGroup& presets(int kind) const;                    // of a window's current type msb
    void fxDispatchKind(int kind, int p);                               // 0x39C0B2
    void fxDispatchReverb(int p);                                       // 0x39C144
    void fxDispatchInsertion(int p);                                    // 0x39C1AC
    void fxDispatchReverbDelay(int p);                                  // 0x39C48A
    void fxDispatchVariation(int p);                                    // 0x39C51C
    void fxResetWindow(int kind);                                       // 0x3A3308
    void fxMasterEq(int p);                                             // 0x3A356C
    void fxSystemParam(int p);                                          // 0x3A2EC (flash)

    // ---- filter, comb and room design ----
    // fxDesignFilter(order, scale, kind, freq, gain, q, steps): writes b0, b1(, b2), -a1(, -a2)
    // into the constant shadow at the listed steps.
    void fxDesignFilter(int order, int scale, int kind, int freq, int gain, int q, const uint16_t* steps);  // flash 0x37414
    void fxDesignCombs(int time, int damp, int count, int sel, const uint16_t* list);  // 0x39C796
    void fxDesignRooms(int time, int y0, int y1, int damp, int extra, const uint16_t* list);  // 0x39CF82
    void fxRoomPairs(int clampSrc, int index, int span, const uint8_t* ids);  // 0x39D2C4
    void fxDesignFromTable(int index, const uint16_t* steps);           // 0x39C758
    void fxFloatCurveB(int scale, int v, int gain, const uint16_t* steps);  // flash 0x39108

    // ---- helpers ----
    int32_t fxBipolar500(int v);                        // 0x39CF62
    int32_t fxBipolar376(int v);                        // 0x39CF72
    int32_t fxDiffusionClamp(int x, int v);             // 0x39D468
    int32_t fxMsToSamples(int v);                       // 0x39D47C
    int32_t fxMulShift16(int v);                        // 0x39D490
    int32_t fxMulTable6(int x);                         // 0x39D4A2: x * level[type0] >> 6
    int32_t fxMulTypeTableB(int x);                     // 0x39D500
    int32_t fxMulTypeTableC(int x);                     // 0x39D53E
    int32_t fxMulTypeTableD(int x);                     // 0x39D598
    void fxAddScaledPairs(int clampSrc, int count, int sel, const uint8_t* ids);  // 0x39C8F4
    void fxAddScaledPairs2(int clampSrc, int sel, const uint8_t* ids);  // 0x3A259C: two slot triples
    void fxScaleRow(int src, int scale, int offset, int count, int stride, const uint16_t* steps, const uint8_t* slots);  // 0x39C9E8
    void fxWriteSpan4(int indexA, int indexB, int offset, int row, const uint16_t* words, const uint8_t* ids);  // 0x39CC78
    void fxCopyScaled12(int balance, const uint16_t* list);             // 0x39CEC8
    void fxLfoSpeed(int v, int lfo);                    // 0x3A3070
    void fxLfoPhase(int v, int lfo);                    // 0x3A30C2
    void fxLfoDepth(int v, int lfo);                    // 0x3A3128
    void fxStoreBipolar376(int v, const uint16_t* step);                // 0x3A31CC
    void fxStoreBipolar500(int v, const uint16_t* step);                // 0x3A31EA
    void fxStoreModePair(int mode, const uint16_t* steps);              // 0x3A3208
    void fxStoreUdiv(int v, const uint16_t* step);                      // 0x3A328E
    void fxStoreNegMul(int v, const uint16_t* step);                    // 0x3A32B0
    void fxStoreScaled(int v, int index, const uint16_t* steps);        // 0x3A4F00
    void fxStoreMirrorHalf(int v, const uint16_t* steps);               // 0x3A5090
    void fxStoreMirrorX32(int v, const uint16_t* steps);                // 0x3A50D8
    void fxStoreMirror3(int v, const uint16_t* steps);                  // 0x3ACA08
    void fxStoreSpan5(int v);                           // 0x3A9D58: the pitch change's second voice slots
    void fxLfoDirection(int lfo, int v);                // 0x3A9B8A: a pitch change voice's LFO words and direction
    int32_t fxInsPitchRatio(int v);                     // 0x3A99FC
    void fxInsPitchSlots();                             // 0x3A9CC6: the first voice's slots
    void fxInsErPattern();                              // 0x3A6354: the 20 early reflections
    int32_t fxInsErGain(int x);                         // 0x3A6338: comb-gain table lookup
    void fxWriteComplementPair(int v, const uint16_t* steps);           // 0x3A5124
    void fxWriteWord(int v, const uint16_t* step);                      // 0x3A5168
    void fxWriteNegated(int v, const uint16_t* step);                   // 0x3A51E4
    void fxWriteRow8(int row, const uint16_t* steps);                   // 0x3A522E
    void fxCopyWords3(const uint16_t* src, const uint16_t* dst);        // flash 0x39390
    void fxCopyWords5(const uint16_t* src, const uint16_t* dst);        // flash 0x3943C
    void fxCopyWords2(const uint16_t* src, const uint16_t* dst);        // flash 0x394BA
    void fxFillBand(int v, const uint16_t* steps);                      // flash 0x394F8

    // ---- per-type handlers: reverb 1-12 ----
    void fxRevHall(int p);                              // 0x3A3728
    void fxRevRoom(int p);                              // 0x3A3C54
    // reverb 13-16
    void fxRevDelayLCR(int p);                          // 0x39D5E0
    void fxRevDelayLR(int p);                           // 0x39D964
    void fxRevEcho(int p);                              // 0x39DC98
    void fxRevCrossDelay(int p);                        // 0x39E074
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

    // ---- parameter store ----
    int32_t readParameter(uint32_t id);                                 // 0x30390: a performance byte, clamped to its range
    const rom::ParamRange& descriptor(uint32_t index) const;            // 0x34478: its range under the current types

    // A performance byte enters here: seven bits, and no effect type past the last one.
    void store(uint32_t index, uint32_t value)
    {
        value &= 0x7F;
        if (index == 168 && value > 16) value = 16;
        if (index == 171 && value > 28) value = 28;
        if (index == 175 && value > 40) value = 40;
        perf[index] = uint8_t(value);
    }
    // A performance's 192 common bytes, loaded as the firmware loads a performance.
    void load_performance(const uint8_t* perf)
    {
        for (uint32_t i = 0; i < 192; i++)
            store(i, perf[i]);
        setSubSendBOffset(0);
        setSubSendAOffset(0);
        applySubDirtyMask(0x8001);
        pollSubDirtyLoop();
    }
    // One byte changed. For a two-byte parameter: its first byte, after both were written.
    void set_performance_byte(uint32_t index, uint32_t value)
    {
        store(index, value);
        markSubParamDirty(index);
        pollSubDirtyLoop();
    }
};

}  // namespace fs1r::vop3host
