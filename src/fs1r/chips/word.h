// fs1r/chips/word.h - the FS1R DSP's number format, shared by the filters and the effects. Ours.
//
// Values are doubles in register units: a register wraps or saturates at +-128, the running sum wraps at
// +-256, a cell clips at +-128, a delay RAM word at +-32 and a t register holds a 1.15 word. These are the
// operations yss236_core::step() applies, by name; a DSP built from them in the chip's order is bit for bit
// the chip. Keep the `+ 0.0`s a lifted program carries: they turn a -0 into +0, as the chip does.
#pragma once

#include <algorithm>
#include <cmath>

namespace fs1r::word {

inline double floored_mod(double a, double m)
{
	double v = std::fmod(a, m);
	if (v != 0 && ((m < 0) != (v < 0))) v += m;
	return v;
}
// Wraps add and take back the range rather than testing it, which is how the chip was measured to round.
inline double wrap_in(double y, double half)
{
	double const a = y + half;
	if (a >= 0 && a < 2 * half) return a - half;
	return floored_mod(a, 2 * half) - half;
}
inline double clip(double v, double lo, double hi) { double const m = v < hi ? v : hi; return lo < m ? m : lo; }

inline double wrap(double y) { return wrap_in(y, 128.0); }        // a register, clamp mode 0
inline double wrap_sum(double y) { return wrap_in(y, 256.0); }    // the running sum
inline double sat(double y) { return clip(y, -128.0, 128.0 - 1.0 / 256); }   // mode 1
inline double pos(double y) { y = sat(y); return 0.0 < y ? y : 0.0; }        // mode 2
inline double mag(double y) { return std::fabs(sat(y)); }                     // mode 3
inline double cell(double y) { return clip(y, -128.0, 128.0); }              // an m cell
inline double clip32(double y) { return clip(y, -32.0, 32.0); }              // a delay RAM word

// A t register: a 1.15 multiplier of y / 128, or the fraction below one delay sample (y * 256 samples)
inline double q15(double y)
{
	double const w = y * 256.0;
	return (w < -32768.0 ? -32768.0 : 32767.0 < w ? 32767.0 : w) / 32768.0;
}
inline double frac(double y) { double const w = y * 256.0; return w - std::floor(w); }
inline double index(double y) { return std::floor(y * 256.0); }   // a delay index, in samples

// The DAC word the unit plays for a cell value: floor to 18 bits of value / 4, clipped at +-8. Full scale is 8.
inline double dac(double v) { return std::floor(std::clamp(v / 4.0, -8.0, 8.0) * 16384.0) / 16384.0; }

// A run of products into the running sum, in the order the chip adds them: sum(a, b, c) is
// wrap_sum(c + wrap_sum(b + wrap_sum(a + 0.0))). sum_on carries on from a sum already taken.
inline double sum_on(double s) { return s; }
template <class... Rest> inline double sum_on(double s, double a, Rest... rest) { return sum_on(wrap_sum(a + s), rest...); }
template <class... Rest> inline double sum(double a, Rest... rest) { return sum_on(wrap_sum(a + 0.0), rest...); }

}  // namespace fs1r::word
