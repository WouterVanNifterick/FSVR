// license:BSD-3-Clause
// copyright-holders:Wouter van Nifterick

// The performance into the effect chip: applyChanges, which the effect task runs for
// every message posted to it, and the functions it reaches.
//
// The caches hold the parameters as read from the performance; the blocks the per-type
// handlers read (param[kind]) are filled from them here.
#include "effects/host/effect_host.h"
#include <algorithm>

namespace fs1r::vop3host {

using namespace rom::mix;

namespace {

constexpr int SEND_LEVEL = 2896;    // an insertion send or an individual output at full level

// the reverb's type map says which window a type runs in: the reverb's, or the delay's
bool isDelay(uint32_t type) { return rom::TYPE_MAP_REVERB[type][0] != 0; }
TypeCode codeOf(const uint8_t (&map)[2]) { return {map[0], map[1]}; }

// the first byte of reverb parameter i: two bytes each for 1-8, one each for 9-16
uint32_t reverbByte(uint32_t i) { return i < 8 ? REVERB_PARAM_BYTES + 2 * i : REVERB_PARAM_BYTES_9 + i - 8; }
// a parameter word as two 7-bit bytes, the high one first
void putWord(uint8_t* at, uint16_t w)
{
    at[0] = (w & 0x3F80) >> 7;
    at[1] = w & 0x7F;
}
// cached EQ word i's master EQ parameter: the low band is 0-3, the mid 8-10, the high 16-19
uint32_t eqParamOf(uint32_t i) { return i < 4 ? i : i < 7 ? i + 4 : i + 9; }
// cached EQ word i into its master EQ parameter
uint32_t storeEq(EffectHost& h, uint32_t i)
{
    const uint32_t q = eqParamOf(i);
    h.eqParam[q] = i < 10 ? h.eqCache[i] : h.eqHighShape();
    return q;
}
// The reverb's type runs in the reverb window or, for the four delays, in the delay window, and the
// other one gets its silent type.
void reverbCode(EffectHost& h, uint32_t rev)
{
    h.code[REVERB].lsb = isDelay(rev) ? 0 : rom::TYPE_MAP_REVERB[rev][1];
}
void reverbDelayCode(EffectHost& h, uint32_t rev)
{
    h.code[REVERB_DELAY] = isDelay(rev) ? TypeCode{8, rom::TYPE_MAP_REVERB[rev][1]} : TypeCode{0, 1};
}

}  // namespace

void EffectHost::applyChanges(uint32_t mask)
{
    // both reverb windows for the type; `keep` keeps the parameters of the one it runs in
    auto reverbType = [&](uint32_t rev, bool keep) {
        reverbCode(*this, rev);
        resetWindow(REVERB, !keep || isDelay(rev));
        reverbDelayCode(*this, rev);
        resetWindow(REVERB_DELAY, !keep || !isDelay(rev));
    };

    if (mask & LOADED) {
        for (uint32_t i = 0; i < 16; i++) {
            reverbCache[i]    = uint32_t(readParameter(reverbByte(i)));
            variationCache[i] = uint32_t(readParameter(VARIATION_PARAM_BYTES + 2 * i));
            insertionCache[i] = uint32_t(readParameter(INSERTION_PARAM_BYTES + 2 * i));
        }
        for (uint32_t i = 0; i < 10; i++)
            eqCache[i] = uint32_t(readParameter(EQ_LOW_GAIN + i));
        reverbCache[DRY_WET]    = 127;                // fully wet
        variationCache[DRY_WET] = 127;
        for (int w : {REVERB, INSERTION, REVERB_DELAY, VARIATION})
            setMute(w, 1);
        std::fill_n(controlOffset, 16, 0);
        const uint32_t rev = uint32_t(readParameter(REVERB_TYPE));
        setMixer(4, 90);
        std::copy_n(reverbCache, 16, param[isDelay(rev) ? REVERB_DELAY : REVERB]);
        reverbType(rev, true);

        std::copy_n(variationCache, 16, param[VARIATION]);
        code[VARIATION] = codeOf(rom::TYPE_MAP_VARIATION[perf[VARIATION_TYPE]]);
        resetWindow(VARIATION, false);

        std::copy_n(insertionCache, 16, param[INSERTION]);
        code[INSERTION] = codeOf(rom::TYPE_MAP_INSERTION[perf[INSERTION_TYPE]]);
        resetWindow(INSERTION, false);

        for (int m : {MIX_REVERB_PAN, MIX_VARIATION_PAN, MIX_VARIATION_RETURN, MIX_VARIATION_TO_REVERB, MIX_INSERTION_PAN})
            applyMixer(m);
        mixParam[MIX_VOLUME] = perf[VOLUME];
        setMasterLevel();
        setInsertionSend(REVERB, perf[INSERTION_TO_REVERB]);
        setInsertionSend(VARIATION, perf[INSERTION_TO_VARIATION]);
        setInsertionLevel(perf[INSERTION_LEVEL]);
        setIndividualOut(perf[INDIVIDUAL_OUT]);
        copyEqParams();
        for (int band : {0, 8, 16})                  // the three the performance has bytes for
            applyParam(MASTER_EQ, band);
        for (int w : {REVERB, INSERTION, REVERB_DELAY, VARIATION})
            uploadWindow(w);
        setMute(REVERB, 0);
        setMute(INSERTION, 0);
        setMute(REVERB_DELAY, 0);
        if (readParameter(VARIATION_TYPE) != 0)
            setMute(VARIATION, 0);
    }

    if (mask & REVERB_TYPE_CHANGED) {
        setMute(REVERB, 1);
        setMute(REVERB_DELAY, 1);
        const uint32_t rev = uint32_t(readParameter(REVERB_TYPE));
        setMixer(4, 90);
        reverbType(rev, false);
        setReverbPanReturn(isDelay(rev), readParameter(REVERB_PAN), readParameter(REVERB_RETURN));
        uploadWindow(REVERB);
        uploadWindow(REVERB_DELAY);
        setMute(REVERB, 0);
        setMute(REVERB_DELAY, 0);
    }

    if (mask & REVERB_CHANGED) {
        const int w = isDelay(readParameter(REVERB_TYPE)) ? REVERB_DELAY : REVERB;
        for (uint32_t i = 0; i < 16; i++)
            if (reverbChanged[i] == 1) {
                reverbChanged[i] = 0;
                param[w][i] = reverbCache[i];
                applyParam(w, int(i));
            }
    }

    if (mask & VARIATION_TYPE_CHANGED) {
        setMute(VARIATION, 1);
        const uint32_t var = uint32_t(readParameter(VARIATION_TYPE));
        code[VARIATION] = codeOf(rom::TYPE_MAP_VARIATION[var]);
        resetWindow(VARIATION, true);
        uploadWindow(VARIATION);
        if (var != 0)
            setMute(VARIATION, 0);
    }

    if (mask & VARIATION_CHANGED) {
        for (uint32_t i = 0; i < 16; i++)
            if (variationChanged[i] == 1) {
                variationChanged[i] = 0;
                param[VARIATION][i] = variationCache[i];
                applyParam(VARIATION, int(i));
            }
    }

    if (mask & INSERTION_TYPE_CHANGED) {
        setMute(INSERTION, 1);
        std::fill_n(controlOffset, 16, 0);
        code[INSERTION] = codeOf(rom::TYPE_MAP_INSERTION[readParameter(INSERTION_TYPE)]);
        resetWindow(INSERTION, true);
        uploadWindow(INSERTION);
        setMute(INSERTION, 0);
    }

    if (mask & INSERTION_CHANGED) {
        // a changed insertion parameter is its cached value plus the controller offset scaled
        // to the range of the parameter, offset * (max - min) / 127, clamped to that range
        for (uint32_t i = 0; i < 16; i++) {
            if (insertionChanged[i] != 1)
                continue;
            const rom::ParamRange& range = rom::INSERTION_PARAMS[perf[INSERTION_TYPE]][2 * i];
            const int32_t lo = uint16_t(range.min), hi = uint16_t(range.max);
            const int32_t add = int16_t(controlOffset[i] * (hi - lo) / 127);
            const int32_t v = int16_t(std::max<int32_t>(int16_t(insertionCache[i] + add), lo));
            insertionChanged[i] = 0;
            param[INSERTION][i] = uint32_t(std::min(v, hi));
            applyParam(INSERTION, int(i));
        }
    }

    if (mask & MIXER_CHANGED) {
        // The firmware clears the insertion flags here, and past the sixteenth the EQ's, never the mixer's.
        for (uint32_t i = 0; i < 21; i++) {
            if (mixerChanged[i] != 1)
                continue;
            applyMixer(i);
            if (i < 16) insertionChanged[i] = 0; else eqChanged[i - 16] = 0;
        }
    }

    if (mask & EQ_CHANGED) {
        for (uint32_t i = 0; i < 11; i++)
            if (eqChanged[i] == 1)
                applyEq(i);
    }
}

// one mixer change, indexed like mixerChanged
void EffectHost::applyMixer(uint32_t i)
{
    switch (i) {
        case MIX_REVERB_PAN:
        case MIX_REVERB_RETURN:          setReverbPanReturn(isDelay(perf[REVERB_TYPE]), perf[REVERB_PAN], perf[REVERB_RETURN]); break;
        case MIX_INSERTION_PAN:          setMixer(MIX_INSERTION_PAN,       perf[INSERTION_PAN]);       break;
        case MIX_VARIATION_RETURN:       setMixer(MIX_VARIATION_RETURN,    perf[VARIATION_RETURN]);    break;
        case MIX_VARIATION_TO_REVERB:    setMixer(MIX_VARIATION_TO_REVERB, perf[VARIATION_TO_REVERB]); break;
        case MIX_VARIATION_PAN:          setMixer(MIX_VARIATION_PAN,       perf[VARIATION_PAN]);       break;
        case MIX_VOLUME:                 setMixer(MIX_VOLUME,              perf[VOLUME]);              break;
        // the sends with their controller offsets, clamped to 0..127
        case MIX_INSERTION_TO_REVERB:    setInsertionSend(REVERB,    std::clamp<int32_t>(int16_t(perf[INSERTION_TO_REVERB]    + uint32_t(sendReverbOffset)),    0, 127)); break;
        case MIX_INSERTION_TO_VARIATION: setInsertionSend(VARIATION, std::clamp<int32_t>(int16_t(perf[INSERTION_TO_VARIATION] + uint32_t(sendVariationOffset)), 0, 127)); break;
        case MIX_INSERTION_LEVEL:        setInsertionLevel(perf[INSERTION_LEVEL]); break;
        case MIX_INDIVIDUAL_OUT:         setIndividualOut(perf[INDIVIDUAL_OUT]);    break;
    }
}

void EffectHost::setMixer(int m, uint32_t v)
{
    mixParam[m] = v;
    applyParam(MIXER, m);
}

// The reverb's pan and return go to the reverb window's words or the delay window's, and the
// other window's are centred and silent.
void EffectHost::setReverbPanReturn(bool delay, uint32_t pan, uint32_t ret)
{
    setMixer(MIX_REVERB_PAN,    delay ? 64 : pan);
    setMixer(MIX_DELAY_PAN,     delay ? pan : 64);
    setMixer(MIX_REVERB_RETURN, delay ? 0 : ret);
    setMixer(MIX_DELAY_RETURN,  delay ? ret : 0);
}

// one master EQ change, indexed like eqChanged (EQ_LOW_GAIN..EQ_HIGH_SHAPE)
void EffectHost::applyEq(uint32_t i)
{
    applyParam(MASTER_EQ, int(storeEq(*this, i)));
}

void EffectHost::copyEqParams()
{
    for (uint32_t i = 0; i < 11; i++)
        storeEq(*this, i);
}

// the insertion's send to the reverb or to the variation, v * 2896 / 127, into two mixer steps
void EffectHost::setInsertionSend(int to, uint32_t v)
{
    const int a = to == REVERB ? 17 : 28, b = to == REVERB ? 23 : 33;
    const int32_t w = int16_t(int(float(v & 0xFF) * float(SEND_LEVEL) / 127.0f));
    coef[MIXER_C[a]] = w;
    coef[MIXER_C[b]] = w;
    writeCoefs(1, MIXER_C + a);
    writeCoefs(1, MIXER_C + b);
}

// the insertion's level, (v * 8192 / 127) >> 2, into two mixer steps
void EffectHost::setInsertionLevel(uint32_t v)
{
    const int32_t w = int16_t(int(float(v & 0xFF) * 8192.0f / 127.0f)) >> 2;
    coef[MIXER_B[8]] = w;
    coef[MIXER_B[9]] = w;
    writeCoefs(1, MIXER_B + 8);
    writeCoefs(1, MIXER_B + 9);
}

// Individual out: a pair of steps at full level or none. Other values write the four as they are.
void EffectHost::setIndividualOut(uint32_t mode)
{
    const uint16_t* s = rom::STEPS_INDIVIDUAL_OUT;
    switch (mode & 0xFF) {
        case OUT_OFF:            coef[s[0]] = 0;          coef[s[1]] = 0;          coef[s[2]] = 0;          coef[s[3]] = 0;          break;
        case OUT_PRE_INSERTION:  coef[s[0]] = SEND_LEVEL; coef[s[1]] = SEND_LEVEL; coef[s[2]] = 0;          coef[s[3]] = 0;          break;
        case OUT_POST_INSERTION: coef[s[0]] = 0;          coef[s[1]] = 0;          coef[s[2]] = SEND_LEVEL; coef[s[3]] = SEND_LEVEL; break;
    }
    writeCoefs(4, s);
}

// the master level word, then the master volume step
void EffectHost::setMasterLevel()
{
    masterLevel = 0x1FFF;
    applyParam(MIXER, MIX_VOLUME);
}

// ---- changes after the load ----
//
// The effect task sleeps on an event flag. Whoever changes something posts a mask (the bits
// accumulate) and the task takes the whole pattern at once, waiting on WAKE, which is why
// every posted mask carries it. Here post keeps the pattern in posted and runPosted is one
// turn of the task.

void EffectHost::post(uint32_t mask)
{
    posted |= mask;
}

// one turn: false when nothing was posted
bool EffectHost::runPosted()
{
    if (!(posted & WAKE))
        return false;
    const uint32_t mask = posted;
    posted = 0;
    applyChanges(mask);
    return true;
}

// the 16 default parameters of a window's current type
void EffectHost::loadTypeDefaults(uint32_t kind)
{
    std::copy_n(presets(int(kind)).defaults[code[kind].lsb], 16, param[kind]);
}

// The three type changes: the new type's code, its default parameters into the block, and
// from there back into the performance and into the cache, with the change flags cleared.

void EffectHost::reverbTypeChanged()
{
    const uint32_t rev = uint32_t(readParameter(REVERB_TYPE));
    const bool delay = isDelay(rev);
    reverbCode(*this, rev);
    loadTypeDefaults(REVERB);
    reverbDelayCode(*this, rev);
    loadTypeDefaults(REVERB_DELAY);
    const uint16_t* block = param[delay ? REVERB_DELAY : REVERB];
    for (uint32_t i = 0; i < 16; i++) {
        if (i < 8) putWord(perf + reverbByte(i), block[i]);
        else perf[reverbByte(i)] = block[i] & 0xFF;
        reverbCache[i] = block[i];
        reverbChanged[i] = 0;
    }
    if (delay) {
        perf[reverbByte(DRY_WET)] = 0;
        reverbCache[DRY_WET] = 127;
    }
}

void EffectHost::variationTypeChanged()
{
    code[VARIATION] = codeOf(rom::TYPE_MAP_VARIATION[readParameter(VARIATION_TYPE)]);
    loadTypeDefaults(VARIATION);
    for (uint32_t i = 0; i < 16; i++) {
        putWord(perf + VARIATION_PARAM_BYTES + 2 * i, param[VARIATION][i]);
        variationCache[i] = param[VARIATION][i];
        variationChanged[i] = 0;
    }
    perf[VARIATION_PARAM_BYTES + 2 * DRY_WET] = 0;
    variationCache[DRY_WET] = 127;
}

void EffectHost::insertionTypeChanged()
{
    code[INSERTION] = codeOf(rom::TYPE_MAP_INSERTION[readParameter(INSERTION_TYPE)]);
    loadTypeDefaults(INSERTION);
    for (uint32_t i = 0; i < 16; i++) {
        putWord(perf + INSERTION_PARAM_BYTES + 2 * i, param[INSERTION][i]);
        insertionCache[i] = param[INSERTION][i];
        insertionChanged[i] = 0;
    }
}

// A performance common byte changed: it goes into its cache or marks its mixer flag, and the
// mask that makes the task apply it is posted. Either byte of a two-byte parameter stands for it.
void EffectHost::byteChanged(uint32_t id)
{
    uint32_t n = id & 0xFFFF;
    if ((n >= REVERB_PARAM_BYTES && n < REVERB_PARAM_BYTES_9) || (n >= VARIATION_PARAM_BYTES && n < REVERB_TYPE))
        n &= ~1u;
    auto mixer = [&](int flag) {
        mixerChanged[flag] = 1;
        post(WAKE | MIXER_CHANGED);
    };
    if (n >= REVERB_PARAM_BYTES && n < REVERB_PARAM_BYTES_9) {
        const uint32_t i = (n - REVERB_PARAM_BYTES) / 2;
        reverbChanged[i] = 1;
        reverbCache[i] = uint32_t(readParameter(n));
        post(WAKE | REVERB_CHANGED);
    }
    if (n >= REVERB_PARAM_BYTES_9 && n < VARIATION_PARAM_BYTES) {
        const uint32_t i = 8 + n - REVERB_PARAM_BYTES_9;
        reverbChanged[i] = 1;
        reverbCache[i] = uint32_t(readParameter(n));
        post(WAKE | REVERB_CHANGED);
    }
    if (n >= VARIATION_PARAM_BYTES && n < INSERTION_PARAM_BYTES) {
        const uint32_t i = (n - VARIATION_PARAM_BYTES) / 2;
        variationChanged[i] = 1;
        variationCache[i] = uint32_t(readParameter(n));
        post(WAKE | VARIATION_CHANGED);
    }
    if (n >= INSERTION_PARAM_BYTES && n < REVERB_TYPE) {
        const uint32_t i = (n - INSERTION_PARAM_BYTES) / 2;
        insertionChanged[i] = 1;
        insertionCache[i] = uint32_t(readParameter(n));
        post(WAKE | INSERTION_CHANGED);
    }
    switch (n) {
        case REVERB_TYPE:            reverbTypeChanged();    post(WAKE | REVERB_TYPE_CHANGED);    break;
        case VARIATION_TYPE:         variationTypeChanged(); post(WAKE | VARIATION_TYPE_CHANGED); break;
        case INSERTION_TYPE:         insertionTypeChanged(); post(WAKE | INSERTION_TYPE_CHANGED); break;
        case REVERB_PAN:             mixer(MIX_REVERB_PAN);             break;
        case REVERB_RETURN:          mixer(MIX_REVERB_RETURN);          break;
        case VARIATION_PAN:          mixer(MIX_VARIATION_PAN);          break;
        case VARIATION_RETURN:       mixer(MIX_VARIATION_RETURN);       break;
        case VARIATION_TO_REVERB:    mixer(MIX_VARIATION_TO_REVERB);    break;
        case INSERTION_PAN:          mixer(MIX_INSERTION_PAN);          break;
        case VOLUME:                 mixer(MIX_VOLUME);                 break;
        case INSERTION_TO_REVERB:    mixer(MIX_INSERTION_TO_REVERB);    break;
        case INSERTION_TO_VARIATION: mixer(MIX_INSERTION_TO_VARIATION); break;
        case INSERTION_LEVEL:        mixer(MIX_INSERTION_LEVEL);        break;
        case INDIVIDUAL_OUT:         mixer(MIX_INDIVIDUAL_OUT);         break;
    }
    if (n >= EQ_LOW_GAIN && n <= EQ_HIGH_SHAPE) {
        // The firmware keeps ten EQ words where it uses eleven: the eleventh, the high band's
        // shape, lands on the first two reverb flags and is read back from them.
        const uint32_t i = n - EQ_LOW_GAIN;
        eqChanged[i] = 1;
        const int v = readParameter(n);
        if (i < 10) eqCache[i] = uint16_t(v);
        else { reverbChanged[0] = uint8_t(v >> 8); reverbChanged[1] = uint8_t(v); }
        post(WAKE | EQ_CHANGED);
    }
}

// a controller's offset for insertion parameter i
void EffectHost::setControlOffset(uint32_t i, uint32_t v)
{
    controlOffset[i & 15] = int16_t(v);
    insertionChanged[i & 15] = 1;
    post(WAKE | INSERTION_CHANGED);
}
// a controller's offset for the insertion to variation send
void EffectHost::setSendVariationOffset(uint32_t v)
{
    sendVariationOffset = int16_t(v);
    mixerChanged[MIX_INSERTION_TO_VARIATION] = 1;
    post(WAKE | MIXER_CHANGED);
}
// the same for the insertion to reverb send
void EffectHost::setSendReverbOffset(uint32_t v)
{
    sendReverbOffset = int16_t(v);
    mixerChanged[MIX_INSERTION_TO_REVERB] = 1;
    post(WAKE | MIXER_CHANGED);
}

}  // namespace fs1r::vop3host
