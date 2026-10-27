// engine/render.cpp - the sample loop: channels, filter voices, part buses, effects.
#include "engine/synth.h"

void Synth::render(float* outL, float* outR, int frames) {
    std::lock_guard<std::mutex> lk(mtx);
    const tuning::NoDenormals flush;
    fx.sync((uint8_t*)&perf);
    filter_latch();
    for (int p = 0; p < 4; p++) if (fltSw >> p & 1) filter_sync(p);
    // Part routing: a part with its insertion switch on feeds the insertion bus only, any other the dry bus and both sends.
    double dry[4], varS[4], revS[4]; bool insSw[4];
    for (int p = 0; p < 4; p++) {
        const uint8_t* q = perf.part[p].p;
        insSw[p] = (q[0x14] & 1) != 0;
        dry[p] = sendlvl(q[0x11]);
        varS[p] = sendlvl(q[0x12]); revS[p] = sendlvl(q[0x13]);
    }
    sense_tick(frames / (double)SR);
    // Ours: an engine with nothing to play sleeps. Once no channel has been active and the output has held one
    // value for a second, which is every reverb tail and filter ring-down gone under the DAC's last step, each
    // block computes its first sample in full and repeats it for the rest, and wakes the moment that sample
    // moves or a channel starts. An idle instance was 12 % of a desktop core (A001 Zap!, A020 Vox Morph), the
    // effect section and the filter chip running on silence; asleep it is 1/256 of that. The unit runs its
    // chips regardless. What sleeping changes is invisible at the output: the effect LFOs and the filter's
    // modulators advance one sample a block rather than a block, so they come back at another phase.
    bool skipping = false;
    for (int i = 0; i < frames; i++, now++) {
        if (asleep && i > 0) {
            // The rest of the block repeats the held sample, from wherever in it the engine fell asleep. A silent
            // channel's last envelopes still count down over those samples to the one the allocator frees it on.
            if (!skipping) { skipping = true; for (auto& c : ch) if (c.active && c.silent && (c.freezeLeft -= frames - i) <= 0) c.active = false; }
            outL[i] = lastL; outR[i] = lastR; continue;
        }
        bool any = false;
        tickAcc += TICK_HZ / SR; while (tickAcc >= 1) { tickAcc -= 1; tick(); }
        // The channels, each panned onto its part's pair. While a part has its filter on the first sixteen
        // pass through the filter chip first, sounding or not, so that a filter rings out after its note.
        // With fltWide the other sixteen have filter voices too, and a channel of an unfiltered part goes straight on.
        const int nf = flt_banks() * FILTER_CHANNELS;
        double pl[4] = {}, pr[4] = {}, v[NCHAN] = {};
        for (int n = 0; n < NCHAN; n++) {
            Chan& c = ch[n];
            if (!c.active) continue;
            if (!c.silent || c.freezeLeft < 0) any = true;   // a silent channel counting down plays nothing more
            const bool steps = !c.silent;
            const double m = render_chan(c, perf.part[c.part].voice, fseq_on(c.part), gateDb, ctlDecimation, egDecimation);
            if (steps) c.ran = now + 1;
            if (n < nf && (c.filtered || !fltWide)) v[n] = m; else { pl[c.part] += m * c.panL; pr[c.part] += m * c.panR; }
        }
        flt.run(flt_banks(), v);
        for (int n = 0; n < nf; n++) { pl[ch[n].part] += v[n] * ch[n].panL; pr[ch[n].part] += v[n] * ch[n].panR; }
        // Four stereo buses into the effects: dry, insertion, variation send, reverb send.
        double bus[8] = {}, l, r;
        for (int p = 0; p < 4; p++) {
            if (insSw[p]) { bus[2] += pl[p]; bus[3] += pr[p]; continue; }
            bus[0] += pl[p] * dry[p]; bus[1] += pr[p] * dry[p];
            bus[4] += pl[p] * varS[p]; bus[5] += pr[p] * varS[p];
            bus[6] += pl[p] * revS[p]; bus[7] += pr[p] * revS[p];
        }
        fx.run(bus, l, r);
        // Ours: whatever happens upstream, the host never gets a NaN. gain stands in for the analogue volume
        // pot, after the DAC.
        if (!std::isfinite(l) || !std::isfinite(r)) l = r = 0.0;
        outL[i] = (float)(l * gain); outR[i] = (float)(r * gain);
        const bool held = outL[i] == lastL && outR[i] == lastR;
        lastL = outL[i]; lastR = outR[i];
        if (any || !held) { quiet = 0; asleep = false; } else if (++quiet >= SR) asleep = true;
    }
}
