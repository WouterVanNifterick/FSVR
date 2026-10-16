// effects/dly_m00.cpp - the reverb delay window's first program.
//
// rom::REVERB_DELAY_PROGRAMS[0], the program the window holds for reverb types 0 to 12 (No Effect,
// Hall 1 and 2, Room 1 to 3, Stage 1 and 2, Plate, White Room, Tunnel, Basement, Canyon). Types 13 to
// 16 of the reverb block (Delay L,C,R, Delay L,R, Echo, Cross Delay) run their programs here instead,
// under the keys dly-m08 to dly-m11, and fx_delay.cpp is the host that feeds those. Under types 0 to
// 12 no DELAY_ROUTES entry matches, so nothing rewrites the window's constants or delay offsets when a
// parameter moves: they are the reverb type's preset (rom::REVERB_DELAY_0_BLOCKS) and they are the
// same thirteen times over, which a read of c.coef[] and c.slot[] under each type confirms. The code
// reads them where they live all the same, as step() reads them, because that is the contract and not
// because the numbers are known here.
//
// What the 72 steps compute, in these groups:
//
//   0x110-0x126  the send into the block and the host's four tone sections, rev::DELAY_LPF,
//                rev::DELAY_HPF and their two copies, three steps each and each leaving a
//                coefficient in t[12] or t[13]. Three of the twenty steps, 0x113, 0x117 and 0x11b,
//                read the block's own three lines (the slots 0x44, 0x45 and 0x46, in the reverb's
//                area), and those reads go out in m[22], m[23] and m[24] five steps later.
//   0x127-0x14d  the window's own line, read as four pairs with the index added, each read taken up
//                by a cell five steps later and each pair differenced and scaled by t[9], t[11], t[11]
//                or t[10] onto the running value. Those three coefficients and the three index loads
//                are all zero under every type that loads this program, so the four products are zero
//                and what the group adds is the running value on its own.
//   0x14e-0x157  three more reads of the block's own area, published in m[25], m[26] and m[27], the
//                two writes of the window's line (the value 0x146 latched, and 0x157's own result),
//                and the block's output, the cell m[41] to the serial out at 0x156.
//
// The registers: r[123] and r[2] the two inputs of the filter groups, r[124], r[126] and r[1] their
// three states, r[127] the first group's output, r[119], r[121], r[122] and r[120] the four tap sums,
// r[13] and r[14] the running value scaled out. The cells: m[12] the send, written by a window outside
// this one, m[56] to m[63] the eight tap captures, m[22] to m[27] what this window publishes, m[8] to
// m[10] the three LFO values it hands on, m[1] and m[5] the two serial inputs. The t registers:
// t[12] and t[13] the two filter coefficients, t[9], t[10] and t[11] the three tap coefficients.
//
// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.

#include "effects/programs.h"

namespace fs1r::effects {

using namespace word;

void dly_m00(State& s)
{
	auto& r = s.r;
	auto& m = s.m;
	auto& t = s.t;
	auto& out = s.out;
	auto const& k = s.k;
	auto const& in = s.in;
	auto const& lfo = s.lfo;
	double const d0 = s.ram[s.at(0x44)];
	double const d1 = s.ram[s.at(0x45)];
	double const d2 = s.ram[s.at(0x46)];
	double const v0 = ((k[0x111] * m[12]) + 0.0) * 16.0;
	double const v1 = sat(((128.0 * k[0x114]) + mag(wrap(((128.0 * k[0x110]) + m[12]) * 2.0) + 0.0)) * 2.0);
	double const v2 = q15(v1);
	double const v3 = pos((k[0x124] * pos((128.0 * k[0x11e]) + sum_on(sat(((k[0x11c] * v1) + 0.0) * 16.0), k[0x11d] * sat((v2 * sat((v2 * v1) + 0.0)) + 0.0)))) + mag((k[0x123] * wrap(((128.0 * k[0x112]) + wrap_sum(v0)) * 2.0)) + 0.0));
	double const v4 = index(v3);
	double const d3 = s.ram[s.at(0x49, int(v4))];
	double const d4 = s.ram[s.at(0x4a, 1 + int(v4))];
	double const v5 = wrap(v0);
	double const v6 = wrap(((128.0 * k[0x116]) + v5) * 2.0);
	double const v7 = sat(((128.0 * k[0x119]) + mag(wrap(((128.0 * k[0x115]) + m[12]) * 2.0) + 0.0)) * 2.0);
	double const v8 = q15(v7);
	double const v9 = pos((128.0 * k[0x127]) + sum_on(sat(((k[0x125] * v7) + 0.0) * 16.0), k[0x126] * sat((v8 * sat((v8 * v7) + 0.0)) + 0.0)));
	double const v10 = pos((k[0x12b] * v9) + mag((k[0x12a] * v6) + 0.0));
	double const v11 = index(v10);
	double const d5 = s.ram[s.at(0x4b, int(v11))];
	double const d6 = s.ram[s.at(0x4c, 1 + int(v11))];
	double const d7 = s.ram[s.at(0x4d, int(v11))];
	double const d8 = s.ram[s.at(0x4e, 1 + int(v11))];
	double const v12 = wrap(((128.0 * k[0x129]) + v5) * 2.0);
	double const v13 = sat(((128.0 * k[0x121]) + mag(wrap(((128.0 * k[0x11b]) + m[12]) * 2.0) + 0.0)) * 2.0);
	double const v14 = q15(v13);
	double const v15 = pos((128.0 * k[0x136]) + sum_on(sat(((k[0x134] * v13) + 0.0) * 16.0), k[0x135] * sat((v14 * sat((v14 * v13) + 0.0)) + 0.0)));
	double const v16 = pos((k[0x13a] * v15) + mag((k[0x139] * v12) + 0.0));
	double const v17 = index(v16);
	double const d9 = s.ram[s.at(0x4f, int(v17))];
	double const d10 = s.ram[s.at(0x50, 1 + int(v17))];
	double const d11 = s.ram[s.at(0x51)];
	double const d12 = s.ram[s.at(0x52)];
	double const d13 = s.ram[s.at(0x53)];
	double const v18 = frac(v3);
	double const v19 = sat((v18 * d4) - wrap_sum((v18 * d3) - d3));
	double const v20 = sat(((k[0x12d] * r[3]) + 0.0) * 2.0);
	double const v21 = sat(((k[0x130] * v20) + sum(k[0x12e] * r[88], k[0x12f] * r[87])) * 2.0);
	double const v22 = sat(((k[0x133] * v21) + sum(k[0x131] * r[89], k[0x132] * r[88])) * 4.0);
	double const v23 = sat(((k[0x13d] * r[4]) + 0.0) * 2.0);
	double const v24 = sat(((k[0x140] * v23) + sum(k[0x13e] * r[91], k[0x13f] * r[90])) * 2.0);
	double const v25 = sat(((k[0x143] * v24) + sum(k[0x141] * r[92], k[0x142] * r[91])) * 4.0);
	s.ram[s.at(0x54)] = clip32(sat((k[0x146] * v25) + sum(k[0x144] * v19, k[0x145] * v22)));
	double const v26 = frac(v16);
	double const v27 = sat((v26 * d10) - wrap_sum((v26 * d9) - d9));
	s.ram[s.at(0x55)] = clip32(sat((k[0x157] * v27) + sum(k[0x156] * v25)));
	double const v28 = frac(v10);
	double const v29 = sat((v28 * d6) - wrap_sum((v28 * d5) - d5));
	double const v30 = sat((v28 * d8) - wrap_sum((v28 * d7) - d7));
	double const v31 = sat((k[0x14b] * v30) + sum(k[0x149] * v19, k[0x14a] * v29));
	double const v32 = sat((k[0x152] * v27) + sum(k[0x150] * v29, k[0x151] * v30));
	r[1] = v9;
	r[2] = v6;
	r[13] = sat(((k[0x14f] * v31) + sum(k[0x14e] * v22)) * 4.0);
	r[14] = sat(((k[0x155] * v32) + sum(k[0x154] * v25)) * 4.0);
	r[87] = v20;
	r[88] = v21;
	r[89] = v22;
	r[90] = v23;
	r[91] = v24;
	r[92] = v25;
	r[119] = v19;
	r[120] = v27;
	r[121] = v29;
	r[122] = v30;
	r[123] = v5;
	r[124] = v31;
	r[125] = v32;
	r[126] = v15;
	r[127] = v12;
	m[1] = in[0][8];
	m[5] = in[1][8];
	m[8] = lfo[8];
	m[9] = lfo[9];
	m[10] = lfo[10];
	m[22] = d0;
	m[23] = d1;
	m[24] = d2;
	m[25] = d11;
	m[26] = d12;
	m[27] = d13;
	m[56] = d3;
	m[57] = d4;
	m[58] = d9;
	m[59] = d10;
	m[60] = d5;
	m[61] = d6;
	m[62] = d7;
	m[63] = d8;
	t[9] = v18;
	t[10] = v26;
	t[11] = v28;
	t[12] = v14;
	t[13] = v8;
	out[10] = m[41];
}

}  // namespace fs1r::effects
