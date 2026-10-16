// fs1r/native/voice_filter.h - one voice of the FS1R's filter: a resonant 4-pole ladder with taps. Ours.
//
// This is what the filter chip's program computes for one voice, written as the DSP it is. The program
// runs sixteen of these interleaved in one 512-step pass; nothing in it couples two voices, so here a
// voice is a struct and a filter is as many of them as the engine wants (fs1r/filter.h).
//
// The arithmetic is the chip's, operation for operation, in its own number format: values are doubles
// that a register wraps or saturates at +-128, a t register holds as a 1.15 word, and a cell clips at
// +-128 (word:: below). Keep every operation in this order, including the `+ 0.0`s, which the chip does
// and which turn a -0 into +0: the voice is checked bit for bit against the chip's program
// (tools/vop3_native_check flt).
//
// The coefficients are the words the filter host writes (FilterHost, in the firmware's layout), as
// fractions: k = int16(word) / 32768. Six of them are constants of the program the host never changes.
#pragma once

#include <cmath>

namespace fs1r::native {

// The chip's number format
namespace word {

// A register: wraps two's complement at +-128 (mode 0), saturates (1), or saturates and holds the
// negatives at zero (2). The running sum wraps at +-256 instead. The wraps add and take back the range
// rather than testing it, because that is what rounds the way the chip's program was measured to.
inline double floored_mod(double a, double m)
{
	double v = std::fmod(a, m);
	if (v != 0 && ((m < 0) != (v < 0))) v += m;
	return v;
}
inline double wrap_in(double y, double half)
{
	double const a = y + half;
	if (a >= 0 && a < 2 * half) return a - half;
	return floored_mod(a, 2 * half) - half;
}
inline double clip(double v, double lo, double hi) { double const m = v < hi ? v : hi; return lo < m ? m : lo; }
inline double wrap(double y) { return wrap_in(y, 128.0); }
inline double wrap_sum(double y) { return wrap_in(y, 256.0); }
inline double sat(double y) { return clip(y, -128.0, 128.0 - 1.0 / 256); }
inline double pos(double y) { y = sat(y); return 0.0 < y ? y : 0.0; }
inline double cell(double y) { return clip(y, -128.0, 128.0); }
// A value as a t register holds it: a 1.15 multiplier of y / 128
inline double q15(double y)
{
	double const w = y * 256.0;
	return (w < -32768.0 ? -32768.0 : 32767.0 < w ? 32767.0 : w) / 32768.0;
}

}  // namespace word

struct VoiceFilter
{
	// What the output reads, by filter type: one of the ladder's stages, or one of its two difference
	// terms for the highpass and the notch. NONE is a type byte past BEF (see Filter::write).
	enum Tap { LPF24, LPF18, LPF12, HPF, BPF, BEF, NONE };

	// ---- the coefficients, as the host writes them ----
	double eg_depth = 0, lfo_depth = 0, cutoff = 0;   // the cutoff word and what moves it
	double gain = 0;                                  // the input gain
	double type_k = 0;                                // 0 for the six types, the type byte past them
	double reso_a = 0, reso_b = 0;                    // the two resonance words (FUN_0000C3D0's tables A and B)
	double ramp = 0;                                  // the output ramp's step a sample: up while the note sounds
	double exp_slope = 0, reso_bias = 0, fb_scale = 0;   // the program's own constants
	double stage_k = 0, damp_k = 0, pole_k = 0;
	Tap tap = LPF24;
	double route = 2;                                 // the output's scale, x1 for the bandpass and x2 otherwise

	// ---- the state ----
	double x1 = 0;              // the input a sample ago
	double s2 = 0, s3 = 0;      // the resonant two-pole section
	double s4 = 0, s5 = 0;      // the two one-poles after it
	double level = 0;           // the output ramp

	// The program's global constants, the cells it loads in its first sixteen steps
	struct Shared { double eg_zero, c61, c62, c63; };

	// The cutoff word's coefficients: the pole gain kf of the two-pole section, kb on its feedback state and
	// kp of the two one-poles. memo_fc is the word they were made for; anything that changes a word under
	// them (Filter's coef(), a new Shared) sets it to NaN, which no word equals.
	double kf = 0, kb = 0, kp = 0, memo_fc = NAN;

	void coefficients(double fc, Shared const& g)
	{
		using namespace word;
		// the pole gain, an exponential of the cutoff word by three squarings
		double const a = pos((exp_slope * fc + g.c62) * 2);
		double const b = pos(q15(a) * a + g.c61);
		double const c = pos(q15(b) * b + 0.0);
		double const pole = q15(c) * c + 0.0;
		double const g1 = pos(pole);

		// the resonance: how much of the ladder's own output comes back, from the two resonance words
		double const r1 = sat(type_k * g1 - g.c63);
		double const r2 = sat(reso_a * r1 + g.c63);
		double const fb = q15(sat(fb_scale * sat((128 * reso_bias + -r2) * 2) + 0.0));
		double const g2 = cell(sat(q15(pos(pole)) * g1 + 0.0));          // the pole gain squared
		double const fg = pos(fb * g2 + g1);
		kf = q15(fg);
		kb = q15(sat(cell(fg) + sat(r2 - cell(fg))));
		kp = q15(pos((pole_k * g2 + g1) * 4));
	}

	// One sample: in is the voice's input, eg its filter EG's level and lfo its LFO2, all in register units
	double run(double in, double eg, double lfo, Shared const& g)
	{
		using namespace word;

		// the input, by its gain, x4. The chip's second serial bank adds its silence first.
		double const x = sat((0.0 + wrap_sum(gain * in + 0.0)) * 4);

		// the cutoff word: the EG by its depth over the EG's own zero, LFO2 by its depth, x2, on the cutoff
		double const fc = pos(128 * cutoff + wrap_sum((lfo_depth * lfo + wrap_sum(eg_depth * eg + g.eg_zero)) * 2));

		// The coefficients follow the cutoff word alone, and it holds still whenever the EG holds and LFO2 is
		// off, which is most of a note: they are recomputed only when it moves (or a word under them did).
		if (fc != memo_fc) { memo_fc = fc; coefficients(fc, g); }

		// the two-pole section: the input's difference against its last sample by the second resonance
		// word, the feedback from both stages, then its two states
		double const diff = wrap_sum(reso_b * x1 - wrap_sum(reso_b * x - x));
		double const bp = sat(kb * s2 + diff);                                           // BEF's tap
		double const hp = sat(stage_k * s3 + bp);                                       // HPF's tap
		double const n2 = sat((kf * cell(hp) + wrap_sum(damp_k * s2 + 0.0)) * 4);
		double const n3 = sat(kf * cell(n2) + s3);

		// the two one-poles after it, which make the 18 and 24 dB slopes
		double const n4 = sat(kp * n3 - wrap_sum(kp * s4 - s4));
		double const n5 = sat(kp * n4 - wrap_sum(kp * s5 - s5));

		// the output ramp, and the type's tap through it
		double const lv = pos(128 * ramp + level);
		double t;
		switch (tap)
		{
			case LPF24: t = n5; break;
			case LPF18: t = n4; break;
			case LPF12: t = n3; break;
			case HPF:   t = hp; break;
			case BPF:   t = n2; break;
			case BEF:   t = bp; break;
			default:    t = 0.0; break;
		}
		double const out = cell(sat((q15(lv) * t + 0.0) * route));

		x1 = x; s2 = n2; s3 = n3; s4 = n4; s5 = n5; level = lv;
		return out;
	}
};

}  // namespace fs1r::native
