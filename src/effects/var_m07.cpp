// effects/var_m07.cpp - the variation window's 3-Band EQ: three cascaded biquads a channel (low shelf, peak,
// high shelf), designed by EffectHost::threeBandEq() into the words this reads. In mono both outputs carry the
// left chain.
//
// The parameters are the words the effect host writes, k[] by the host's step numbering (rom_tables.h names
// them); the registers r[], cells m[] and t[] and the delay memory are the effect State (programs.h).

#include "effects/programs.h"

namespace fsvr::effects {

namespace {

using word::sat;
using word::sum;

// One channel's three bands, cascaded, in direct form I. `w` is the channel's fifteen words, five a band in
// step order (b1, b2, -a1, -a2, b0); `h` its eight history registers: the input's last two samples, then
// each band's last two outputs, which are also the next band's input history. The high band's route (x16)
// restores the scale its designer took out (designFilter(2, 4, ...)); the lower two are designed at x2.
double eq3(double const* w, double* h, double x)
{
	constexpr double ROUTE[3] = {2.0, 2.0, 16.0};
	double y[3];
	double in = x;
	for (int b = 0; b < 3; b++) {
		double const* c = w + 5 * b;
		double const* z = h + 2 * b;
		in = y[b] = sat((c[4] * in + sum(c[0] * z[0], c[1] * z[1], c[2] * z[2], c[3] * z[3])) * ROUTE[b]);
	}
	// every history pair moves one sample on
	for (int b = 3; b-- > 0;) { h[2 * b + 3] = h[2 * b + 2]; h[2 * b + 2] = y[b]; }
	h[1] = h[0];
	h[0] = x;
	return in;
}

}  // namespace

void var_m07(State& s)
{
	auto& r = s.r;
	auto const& k = s.k;

	// The window moves cells for the other programs: two serial inputs, two LFOs and six delay taps.
	constexpr int TAPS[6] = {0x56, 0x57, 0x58, 0x63, 0x64, 0x65};
	double tap[6];
	for (int i = 0; i < 6; i++) tap[i] = s.ram[s.at(TAPS[i])];

	// The inputs: left is EQ3_A's mix of the two sends (mono: their average), right the right send
	double const left = sat((k[0x159] * r[6] + sum(k[0x158] * r[5])) * 2.0);
	double const right = sat((k[0x169] * r[6] + 0.0) * 2.0);

	double const l = eq3(&k[0x15a], &r[103], left);
	double const rr = eq3(&k[0x16a], &r[111], right);

	// EQ3_B: the two outputs at the type's level; in mono both carry the left chain
	r[15] = sat((k[0x179] * l + 0.0) * 4.0);
	r[16] = sat((k[0x17b] * rr + sum(k[0x17a] * l)) * 4.0);

	s.m[2] = s.in[0][10];
	s.m[6] = s.in[1][10];
	s.m[11] = s.lfo[11];
	s.m[12] = s.lfo[12];
	for (int i = 0; i < 6; i++) s.m[28 + i] = tap[i];
}

}  // namespace fsvr::effects
