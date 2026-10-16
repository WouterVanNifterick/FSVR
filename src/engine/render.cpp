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
        varS[p] = sendlvl(ctrl_part(p, 20, q[0x12])); revS[p] = sendlvl(ctrl_part(p, 19, q[0x13]));
    }
    sense_tick(frames / (double)SR);
    for (int i = 0; i < frames; i++) {
        tickAcc += TICK_HZ / SR; while (tickAcc >= 1) { tickAcc -= 1; tick(); }
        // The channels, each panned onto its part's pair. While a part has its filter on the first sixteen
        // pass through the filter chip first, sounding or not, so that a filter rings out after its note.
        // With fltWide the other sixteen have filter voices too, and a channel of an unfiltered part goes straight on.
        const int nf = flt_banks() * FILTER_CHANNELS;
        double pl[4] = {}, pr[4] = {}, v[NCHAN] = {};
        for (int n = 0; n < NCHAN; n++) {
            Chan& c = ch[n];
            if (!c.active) continue;
            const double m = render_chan(c, perf.part[c.part].voice, fseq_on(c.part), gateDb, ctlDecimation);
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
    }
}
