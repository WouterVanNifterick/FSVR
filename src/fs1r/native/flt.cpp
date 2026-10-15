// fs1r/native/flt.cpp - VOP3-1, the filter chip's program: all 512 steps, written out.
//
// The chip takes sixteen channels in and gives the same sixteen back filtered, and the program is a
// round robin over them: the work for one channel is spread through the pass and the registers and m
// cells it uses are the ones its page names. A page is 124 steps and holds four channels, at
// 16 + 124 * (((s >> 2) + 1) & 3) for slot s, which is FilterHost's own page formula, and a page's
// constants are its four cutoff words (step B + 3k + 2), EG depths (B + 3k), LFO2 depths (B + 3k + 1),
// resonance (B + 32 + k), input gains (B + 52 + 4k and B + 54 + 4k) and output ramps (B + 116 + k).
// So the constants are read from the core every pass and are never numbers here; the program words are
// the firmware's own and are folded into the arithmetic step by step.
//
// docs/vop3_native/README.md is the brief, docs/filter.md what the chip computes.
#include "fs1r/native/native.h"

namespace fs1r::native {

namespace {

// Every step counts itself and lands the writes three steps old, which is what begin_step does. Then
// the two latches: x takes this step's r[sr] and src this step's m[sm], each keeping the step before in
// px and psrc for the steps that store them (an rcopy step writes the previous x).
inline void latch_r(yss236_core& c, int sr)
{
	c.px = c.x; c.haspx = c.hasx; c.hasx = sr != 0; c.x = sr ? c.r[sr] : 0.0;
}
inline void latch_m(yss236_core& c, int sm) { c.psrc = c.src; c.src = c.m[sm]; }

// The t file: a result goes in as a 1.15 word (t is a multiplier input, so the word is the value / 128),
// and a tw = 1 step writes its own constant instead.
inline void tput(yss236_core& c, int tn, double y)
{
	double const w = y * 256.0;
	c.queue_t(tn, (w < -32768.0 ? -32768.0 : 32767.0 < w ? 32767.0 : w) / 32768.0);
}
inline void tput_k(yss236_core& c, int st, int tn) { c.queue_t(tn, yss236_core::s16(c.coef[st])); }

}  // namespace

// The whole program: the check hands this every step, and a part of one is the interpreter's.
void flt(yss236_core& c, int from, int to)
{
	if (from != 0 || to != yss236_core::STEPS) {
		for (int st = from; st < to; st++) c.step(st);
		return;
	}

	double* const r = c.r;
	double* const m = c.m;
	double* const t = c.t;
	double& acc = c.acc;
	double v, y;                                   // the step's value, as its arithmetic leaves it
	// 0x000
	c.begin_step();
	c.queue_d(24, c.in[0][0]);
	latch_r(c, 0);
	// 0x001
	c.begin_step();
	c.queue_d(25, c.in[1][0]);
	latch_r(c, 0);
	// 0x002
	c.begin_step();
	c.queue_d(23, c.in[3][0]);
	latch_r(c, 0);
	// 0x003
	c.begin_step();
	c.out[0] = m[25];
	latch_r(c, 0);
	// 0x004
	c.begin_step();
	c.m_skip = 0x00b;                 // the gate: the steps to its target wait for the host trigger
	// 0x005
	c.begin_step();
	// the gated block: the host's trigger writes these registers itself (yss236_core::trigger)
	// 0x006
	c.begin_step();
	// the gated block: the host's trigger writes these registers itself (yss236_core::trigger)
	// 0x007
	c.begin_step();
	// the gated block: the host's trigger writes these registers itself (yss236_core::trigger)
	// 0x008
	c.begin_step();
	// the gated block: the host's trigger writes these registers itself (yss236_core::trigger)
	// 0x009
	c.begin_step();
	// the gated block: the host's trigger writes these registers itself (yss236_core::trigger)
	// 0x00a
	c.begin_step();
	// the gated block: the host's trigger writes these registers itself (yss236_core::trigger)
	// 0x00b
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x00b]);
	c.acc = yss236_core::accmode(v, 0);
	c.queue_d(63, yss236_core::clip(c.acc, yss236_core::DSAT));
	// 0x00c
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x00c]);
	c.queue_r(127, yss236_core::mode(v, 0));
	c.acc = yss236_core::accmode(v, 0);
	// 0x00d
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x00d]);
	c.acc = yss236_core::accmode(v, 0);
	c.queue_d(62, yss236_core::clip(c.acc, yss236_core::DSAT));
	// 0x00e
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x00e]);
	c.acc = yss236_core::accmode(v, 0);
	c.queue_d(61, yss236_core::clip(c.acc, yss236_core::DSAT));
	// 0x00f
	c.begin_step();
	c.queue_d(3, c.lfo[0]);
	latch_r(c, 0);

	// ---- page 16: channels 0 to 3 ----
	// 0x010
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 17);
	y = yss236_core::s16(c.coef[0x010]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x011
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 1);
	y = (yss236_core::s16(c.coef[0x011]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x012
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x012]) + acc;
	c.queue_r(97, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x013
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 20);
	y = yss236_core::s16(c.coef[0x013]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x014
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 2);
	y = (yss236_core::s16(c.coef[0x014]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x015
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x015]) + acc;
	c.queue_r(98, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x016
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 23);
	y = yss236_core::s16(c.coef[0x016]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x017
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 3);
	y = (yss236_core::s16(c.coef[0x017]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x018
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x018]) + acc;
	c.queue_r(99, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x019
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 26);
	y = yss236_core::s16(c.coef[0x019]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x01a
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 4);
	y = (yss236_core::s16(c.coef[0x01a]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x01b
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x01b]) + acc;
	c.queue_r(97, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x01c
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x01c]) * r[97] + m[62]) * 2.0;
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x01d
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x01d]) * r[98] + m[62]) * 2.0;
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x01e
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x01e]) * r[99] + m[62]) * 2.0;
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x01f
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x01f]) * r[97] + m[62]) * 2.0;
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x020
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 61);
	y = t[1] * r[98] + m[61];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x021
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 61);
	y = t[2] * r[97] + m[61];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x022
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 61);
	y = t[1] * r[98] + m[61];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x023
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 61);
	y = t[2] * r[97] + m[61];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x024
	c.begin_step();
	c.queue_d(17, c.in[0][1]);
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x025
	c.begin_step();
	c.queue_d(20, c.in[1][1]);
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x026
	c.begin_step();
	c.queue_d(26, c.in[3][1]);
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x027
	c.begin_step();
	c.queue_d(4, c.lfo[1]);
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(99, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x028
	c.begin_step();
	c.out[1] = m[27];
	latch_r(c, 98);
	latch_m(c, 27);
	y = t[1] * r[98];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x029
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(100, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x02a
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(103, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 3, y);
	// 0x02b
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	y = t[2] * r[99];
	c.queue_r(106, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 4, y);
	// 0x02c
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x02c]) * r[97] - m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x02d
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x02d]) * r[100] - m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x02e
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x02e]) * r[103] - m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x02f
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x02f]) * r[106] - m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x030
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x030]) * r[98] + m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x031
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x031]) * r[99] + m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x032
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x032]) * r[98] + m[63];
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x033
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x033]) * r[99] + m[63];
	c.queue_r(102, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x034
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[1] * r[97];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(37, yss236_core::clip(y, yss236_core::DSAT));
	// 0x035
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 0);
	y = t[2] * r[100];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(38, yss236_core::clip(y, yss236_core::DSAT));
	// 0x036
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 0);
	y = t[3] * r[103];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(39, yss236_core::clip(y, yss236_core::DSAT));
	// 0x037
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 0);
	y = t[4] * r[106];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(40, yss236_core::clip(y, yss236_core::DSAT));
	// 0x038
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x038]) - r[98]) * 2.0;
	c.queue_r(104, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x039
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x039]) - r[99]) * 2.0;
	c.queue_r(105, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x03a
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x03a]) - r[101]) * 2.0;
	c.queue_r(104, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x03b
	c.begin_step();
	latch_r(c, 102);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x03b]) - r[102]) * 2.0;
	c.queue_r(105, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x03c
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x03c]) * r[104];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 1, y);
	// 0x03d
	c.begin_step();
	latch_r(c, 105);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x03d]) * r[105];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x03e
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x03e]) * r[104];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 1, y);
	// 0x03f
	c.begin_step();
	latch_r(c, 105);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x03f]) * r[105];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x040
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 37);
	y = t[1] * c.src + r[97];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(27, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 1, y);
	// 0x041
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 38);
	y = t[2] * c.src + r[100];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(41, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 3, y);
	// 0x042
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 39);
	y = t[1] * c.src + r[103];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(18, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 5, y);
	// 0x043
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 40);
	y = t[2] * c.src + r[106];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(42, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 7, y);
	// 0x044
	c.begin_step();
	c.queue_d(23, c.in[0][2]);
	latch_r(c, 0);
	latch_m(c, 18);
	y = yss236_core::s16(c.coef[0x044]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x045
	c.begin_step();
	c.queue_d(27, c.in[1][2]);
	latch_r(c, 0);
	latch_m(c, 19);
	y = (yss236_core::s16(c.coef[0x045]) * c.src + acc) * 4.0;
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x046
	c.begin_step();
	c.queue_d(19, c.in[3][2]);
	latch_r(c, 98);
	latch_m(c, 27);
	y = r[98] - m[27];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x046, 8);
	// 0x047
	c.begin_step();
	c.queue_d(5, c.lfo[2]);
	latch_r(c, 0);
	latch_m(c, 27);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x048
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 21);
	y = yss236_core::s16(c.coef[0x048]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x049
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 22);
	y = (yss236_core::s16(c.coef[0x049]) * c.src + acc) * 4.0;
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x04a
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 41);
	y = r[99] - m[41];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x04a, 9);
	// 0x04b
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 41);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 4, y);
	// 0x04c
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 24);
	y = yss236_core::s16(c.coef[0x04c]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x04d
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 25);
	y = (yss236_core::s16(c.coef[0x04d]) * c.src + acc) * 4.0;
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x04e
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 18);
	y = r[101] - m[18];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x04e, 10);
	// 0x04f
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 18);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 6, y);
	// 0x050
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 17);
	y = yss236_core::s16(c.coef[0x050]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x051
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 20);
	y = (yss236_core::s16(c.coef[0x051]) * c.src + acc) * 4.0;
	c.queue_r(104, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x052
	c.begin_step();
	latch_r(c, 102);
	latch_m(c, 42);
	y = r[102] - m[42];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x052, 11);
	// 0x053
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 42);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 8, y);
	// 0x054
	c.begin_step();
	c.out[2] = m[28];
	latch_r(c, 98);
	latch_m(c, 28);
	y = t[8] * r[98] - r[98];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x055
	c.begin_step();
	latch_r(c, 1);
	latch_m(c, 0);
	y = t[8] * r[1] - acc;
	c.queue_r(1, c.haspx ? c.px : r[1]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x056
	c.begin_step();
	latch_r(c, 2);
	latch_m(c, 0);
	y = t[2] * r[2] + acc;
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x057
	c.begin_step();
	latch_r(c, 3);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x057]) * r[3] + acc;
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(17, yss236_core::clip(y, yss236_core::DSAT));
	// 0x058
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	y = t[9] * r[99] - r[99];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x059
	c.begin_step();
	latch_r(c, 7);
	latch_m(c, 0);
	y = t[9] * r[7] - acc;
	c.queue_r(7, c.haspx ? c.px : r[7]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x05a
	c.begin_step();
	latch_r(c, 8);
	latch_m(c, 0);
	y = t[4] * r[8] + acc;
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x05b
	c.begin_step();
	latch_r(c, 9);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x05b]) * r[9] + acc;
	c.queue_r(102, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(18, yss236_core::clip(y, yss236_core::DSAT));
	// 0x05c
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 0);
	y = t[10] * r[101] - r[101];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x05d
	c.begin_step();
	latch_r(c, 13);
	latch_m(c, 0);
	y = t[10] * r[13] - acc;
	c.queue_r(13, c.haspx ? c.px : r[13]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x05e
	c.begin_step();
	latch_r(c, 14);
	latch_m(c, 0);
	y = t[6] * r[14] + acc;
	c.queue_r(104, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x05f
	c.begin_step();
	latch_r(c, 15);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x05f]) * r[15] + acc;
	c.queue_r(105, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(20, yss236_core::clip(y, yss236_core::DSAT));
	// 0x060
	c.begin_step();
	c.queue_d(22, c.in[0][3]);
	latch_r(c, 104);
	latch_m(c, 0);
	y = t[11] * r[104] - r[104];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x061
	c.begin_step();
	c.queue_d(24, c.in[1][3]);
	latch_r(c, 19);
	latch_m(c, 0);
	y = t[11] * r[19] - acc;
	c.queue_r(19, c.haspx ? c.px : r[19]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x062
	c.begin_step();
	c.queue_d(21, c.in[3][3]);
	latch_r(c, 20);
	latch_m(c, 0);
	y = t[8] * r[20] + acc;
	c.queue_r(107, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x063
	c.begin_step();
	latch_r(c, 21);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x063]) * r[21] + acc;
	c.queue_r(108, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(17, yss236_core::clip(y, yss236_core::DSAT));
	// 0x064
	c.begin_step();
	c.out[3] = m[29];
	latch_r(c, 2);
	latch_m(c, 29);
	y = yss236_core::s16(c.coef[0x064]) * r[2];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x065
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 17);
	y = (t[1] * c.src + acc) * 4.0;
	c.queue_r(2, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(18, yss236_core::clip(y, yss236_core::DSAT));
	// 0x066
	c.begin_step();
	c.queue_d(6, c.lfo[3]);
	latch_r(c, 8);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x066]) * r[8];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x067
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 18);
	y = (t[3] * c.src + acc) * 4.0;
	c.queue_r(8, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(20, yss236_core::clip(y, yss236_core::DSAT));
	// 0x068
	c.begin_step();
	latch_r(c, 14);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x068]) * r[14];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x069
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 20);
	y = (t[5] * c.src + acc) * 4.0;
	c.queue_r(14, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(17, yss236_core::clip(y, yss236_core::DSAT));
	// 0x06a
	c.begin_step();
	latch_r(c, 20);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x06a]) * r[20];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x06b
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 17);
	y = (t[7] * c.src + acc) * 4.0;
	c.queue_r(20, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(18, yss236_core::clip(y, yss236_core::DSAT));
	// 0x06c
	c.begin_step();
	latch_r(c, 3);
	latch_m(c, 18);
	y = t[1] * c.src + r[3];
	c.queue_r(3, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x06d
	c.begin_step();
	latch_r(c, 9);
	latch_m(c, 20);
	y = t[3] * c.src + r[9];
	c.queue_r(9, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x06e
	c.begin_step();
	latch_r(c, 15);
	latch_m(c, 17);
	y = t[5] * c.src + r[15];
	c.queue_r(15, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x06f
	c.begin_step();
	latch_r(c, 21);
	latch_m(c, 18);
	y = t[7] * c.src + r[21];
	c.queue_r(21, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x070
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 37);
	y = (yss236_core::s16(c.coef[0x070]) * c.src + r[97]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x071
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 38);
	y = (yss236_core::s16(c.coef[0x071]) * c.src + r[100]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x072
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 39);
	y = (yss236_core::s16(c.coef[0x072]) * c.src + r[103]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 3, y);
	// 0x073
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 40);
	y = (yss236_core::s16(c.coef[0x073]) * c.src + r[106]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 4, y);
	// 0x074
	c.begin_step();
	latch_r(c, 4);
	latch_m(c, 0);
	y = t[1] * r[4] - r[4];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x075
	c.begin_step();
	latch_r(c, 3);
	latch_m(c, 0);
	y = t[1] * r[3] - acc;
	c.queue_r(4, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x076
	c.begin_step();
	latch_r(c, 10);
	latch_m(c, 0);
	y = t[2] * r[10] - r[10];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x077
	c.begin_step();
	latch_r(c, 9);
	latch_m(c, 0);
	y = t[2] * r[9] - acc;
	c.queue_r(10, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x078
	c.begin_step();
	latch_r(c, 16);
	latch_m(c, 0);
	y = t[3] * r[16] - r[16];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x079
	c.begin_step();
	latch_r(c, 15);
	latch_m(c, 0);
	y = t[3] * r[15] - acc;
	c.queue_r(16, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x07a
	c.begin_step();
	latch_r(c, 22);
	latch_m(c, 0);
	y = t[4] * r[22] - r[22];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x07b
	c.begin_step();
	latch_r(c, 21);
	latch_m(c, 0);
	y = t[4] * r[21] - acc;
	c.queue_r(22, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x07c
	c.begin_step();
	latch_r(c, 5);
	latch_m(c, 0);
	y = t[1] * r[5] - r[5];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x07d
	c.begin_step();
	latch_r(c, 4);
	latch_m(c, 0);
	y = t[1] * r[4] - acc;
	c.queue_r(5, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x07e
	c.begin_step();
	latch_r(c, 11);
	latch_m(c, 0);
	y = t[2] * r[11] - r[11];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x07f
	c.begin_step();
	latch_r(c, 10);
	latch_m(c, 0);
	y = t[2] * r[10] - acc;
	c.queue_r(11, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x080
	c.begin_step();
	c.queue_d(28, c.in[0][4]);
	latch_r(c, 17);
	latch_m(c, 0);
	y = t[3] * r[17] - r[17];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x081
	c.begin_step();
	c.queue_d(29, c.in[1][4]);
	latch_r(c, 16);
	latch_m(c, 0);
	y = t[3] * r[16] - acc;
	c.queue_r(17, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x082
	c.begin_step();
	c.queue_d(25, c.in[3][4]);
	latch_r(c, 23);
	latch_m(c, 0);
	y = t[4] * r[23] - r[23];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x083
	c.begin_step();
	c.out[4] = m[31];
	latch_r(c, 22);
	latch_m(c, 31);
	y = t[4] * r[22] - acc;
	c.queue_r(23, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x084
	c.begin_step();
	latch_r(c, 6);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x084]) + r[6];
	c.queue_r(6, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 1, c.acc);
	// 0x085
	c.begin_step();
	latch_r(c, 12);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x085]) + r[12];
	c.queue_r(12, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 2, c.acc);
	// 0x086
	c.begin_step();
	latch_r(c, 18);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x086]) + r[18];
	c.queue_r(18, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 1, c.acc);
	// 0x087
	c.begin_step();
	c.queue_d(7, c.lfo[4]);
	latch_r(c, 24);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x087]) + r[24];
	c.queue_r(24, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 2, c.acc);
	// 0x088
	c.begin_step();
	latch_r(c, c.ops[0x088].sr);
	latch_m(c, 0);
	y = (t[1] * r[c.ops[0x088].sr]) * yss236_core::RSCALE[c.ops[0x088].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(17, yss236_core::clip(y, yss236_core::DSAT));
	// 0x089
	c.begin_step();
	latch_r(c, c.ops[0x089].sr);
	latch_m(c, 0);
	y = (t[2] * r[c.ops[0x089].sr]) * yss236_core::RSCALE[c.ops[0x089].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(18, yss236_core::clip(y, yss236_core::DSAT));
	// 0x08a
	c.begin_step();
	latch_r(c, c.ops[0x08a].sr);
	latch_m(c, 0);
	y = (t[1] * r[c.ops[0x08a].sr]) * yss236_core::RSCALE[c.ops[0x08a].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(19, yss236_core::clip(y, yss236_core::DSAT));
	// 0x08b
	c.begin_step();
	latch_r(c, c.ops[0x08b].sr);
	latch_m(c, 0);
	y = (t[2] * r[c.ops[0x08b].sr]) * yss236_core::RSCALE[c.ops[0x08b].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(20, yss236_core::clip(y, yss236_core::DSAT));

	// ---- page 140: channels 4 to 7 ----
	// 0x08c
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 19);
	y = yss236_core::s16(c.coef[0x08c]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x08d
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 5);
	y = (yss236_core::s16(c.coef[0x08d]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x08e
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x08e]) + acc;
	c.queue_r(97, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x08f
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 21);
	y = yss236_core::s16(c.coef[0x08f]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x090
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 6);
	y = (yss236_core::s16(c.coef[0x090]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x091
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x091]) + acc;
	c.queue_r(98, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x092
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 25);
	y = yss236_core::s16(c.coef[0x092]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x093
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 7);
	y = (yss236_core::s16(c.coef[0x093]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x094
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x094]) + acc;
	c.queue_r(99, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x095
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 30);
	y = yss236_core::s16(c.coef[0x095]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x096
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 8);
	y = (yss236_core::s16(c.coef[0x096]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x097
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x097]) + acc;
	c.queue_r(97, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x098
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x098]) * r[97] + m[62]) * 2.0;
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x099
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x099]) * r[98] + m[62]) * 2.0;
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x09a
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x09a]) * r[99] + m[62]) * 2.0;
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x09b
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x09b]) * r[97] + m[62]) * 2.0;
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x09c
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 61);
	y = t[1] * r[98] + m[61];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x09d
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 61);
	y = t[2] * r[97] + m[61];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x09e
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 61);
	y = t[1] * r[98] + m[61];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x09f
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 61);
	y = t[2] * r[97] + m[61];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x0a0
	c.begin_step();
	c.queue_d(21, c.in[0][5]);
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x0a1
	c.begin_step();
	c.queue_d(25, c.in[1][5]);
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x0a2
	c.begin_step();
	c.queue_d(30, c.in[3][5]);
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x0a3
	c.begin_step();
	c.out[5] = m[32];
	latch_r(c, 97);
	latch_m(c, 32);
	y = t[2] * r[97];
	c.queue_r(99, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x0a4
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x0a5
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(100, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x0a6
	c.begin_step();
	c.queue_d(8, c.lfo[5]);
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(103, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 3, y);
	// 0x0a7
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	y = t[2] * r[99];
	c.queue_r(106, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 4, y);
	// 0x0a8
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x0a8]) * r[97] - m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0a9
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x0a9]) * r[100] - m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0aa
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x0aa]) * r[103] - m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0ab
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x0ab]) * r[106] - m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0ac
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x0ac]) * r[98] + m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0ad
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x0ad]) * r[99] + m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0ae
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x0ae]) * r[98] + m[63];
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0af
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x0af]) * r[99] + m[63];
	c.queue_r(102, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0b0
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[1] * r[97];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(32, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0b1
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 0);
	y = t[2] * r[100];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(37, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0b2
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 0);
	y = t[3] * r[103];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(38, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0b3
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 0);
	y = t[4] * r[106];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(39, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0b4
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x0b4]) - r[98]) * 2.0;
	c.queue_r(104, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x0b5
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x0b5]) - r[99]) * 2.0;
	c.queue_r(105, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x0b6
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x0b6]) - r[101]) * 2.0;
	c.queue_r(104, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x0b7
	c.begin_step();
	latch_r(c, 102);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x0b7]) - r[102]) * 2.0;
	c.queue_r(105, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x0b8
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0b8]) * r[104];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 1, y);
	// 0x0b9
	c.begin_step();
	latch_r(c, 105);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0b9]) * r[105];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x0ba
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0ba]) * r[104];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 1, y);
	// 0x0bb
	c.begin_step();
	latch_r(c, 105);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0bb]) * r[105];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x0bc
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 32);
	y = t[1] * c.src + r[97];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(31, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 1, y);
	// 0x0bd
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 37);
	y = t[2] * c.src + r[100];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(40, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 3, y);
	// 0x0be
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 38);
	y = t[1] * c.src + r[103];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(23, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 5, y);
	// 0x0bf
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 39);
	y = t[2] * c.src + r[106];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(41, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 7, y);
	// 0x0c0
	c.begin_step();
	c.queue_d(27, c.in[0][6]);
	latch_r(c, 0);
	latch_m(c, 23);
	y = yss236_core::s16(c.coef[0x0c0]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0c1
	c.begin_step();
	c.queue_d(31, c.in[1][6]);
	latch_r(c, 0);
	latch_m(c, 27);
	y = (yss236_core::s16(c.coef[0x0c1]) * c.src + acc) * 4.0;
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0c2
	c.begin_step();
	c.queue_d(22, c.in[3][6]);
	latch_r(c, 98);
	latch_m(c, 31);
	y = r[98] - m[31];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x0c2, 8);
	// 0x0c3
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 31);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x0c4
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 22);
	y = yss236_core::s16(c.coef[0x0c4]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0c5
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 24);
	y = (yss236_core::s16(c.coef[0x0c5]) * c.src + acc) * 4.0;
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0c6
	c.begin_step();
	c.queue_d(9, c.lfo[6]);
	latch_r(c, 99);
	latch_m(c, 40);
	y = r[99] - m[40];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x0c6, 9);
	// 0x0c7
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 40);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 4, y);
	// 0x0c8
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 28);
	y = yss236_core::s16(c.coef[0x0c8]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0c9
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 29);
	y = (yss236_core::s16(c.coef[0x0c9]) * c.src + acc) * 4.0;
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0ca
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 23);
	y = r[101] - m[23];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x0ca, 10);
	// 0x0cb
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 23);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 6, y);
	// 0x0cc
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 21);
	y = yss236_core::s16(c.coef[0x0cc]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0cd
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 25);
	y = (yss236_core::s16(c.coef[0x0cd]) * c.src + acc) * 4.0;
	c.queue_r(104, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0ce
	c.begin_step();
	latch_r(c, 102);
	latch_m(c, 41);
	y = r[102] - m[41];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x0ce, 11);
	// 0x0cf
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 41);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 8, y);
	// 0x0d0
	c.begin_step();
	c.out[6] = m[33];
	latch_r(c, 98);
	latch_m(c, 33);
	y = t[8] * r[98] - r[98];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0d1
	c.begin_step();
	latch_r(c, 25);
	latch_m(c, 0);
	y = t[8] * r[25] - acc;
	c.queue_r(25, c.haspx ? c.px : r[25]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0d2
	c.begin_step();
	latch_r(c, 26);
	latch_m(c, 0);
	y = t[2] * r[26] + acc;
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0d3
	c.begin_step();
	latch_r(c, 27);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0d3]) * r[27] + acc;
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(21, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0d4
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	y = t[9] * r[99] - r[99];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0d5
	c.begin_step();
	latch_r(c, 31);
	latch_m(c, 0);
	y = t[9] * r[31] - acc;
	c.queue_r(31, c.haspx ? c.px : r[31]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0d6
	c.begin_step();
	latch_r(c, 32);
	latch_m(c, 0);
	y = t[4] * r[32] + acc;
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0d7
	c.begin_step();
	latch_r(c, 33);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0d7]) * r[33] + acc;
	c.queue_r(102, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(23, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0d8
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 0);
	y = t[10] * r[101] - r[101];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0d9
	c.begin_step();
	latch_r(c, 37);
	latch_m(c, 0);
	y = t[10] * r[37] - acc;
	c.queue_r(37, c.haspx ? c.px : r[37]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0da
	c.begin_step();
	latch_r(c, 38);
	latch_m(c, 0);
	y = t[6] * r[38] + acc;
	c.queue_r(104, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0db
	c.begin_step();
	latch_r(c, 39);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0db]) * r[39] + acc;
	c.queue_r(105, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(24, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0dc
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = t[11] * r[104] - r[104];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0dd
	c.begin_step();
	latch_r(c, 43);
	latch_m(c, 0);
	y = t[11] * r[43] - acc;
	c.queue_r(43, c.haspx ? c.px : r[43]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0de
	c.begin_step();
	latch_r(c, 44);
	latch_m(c, 0);
	y = t[8] * r[44] + acc;
	c.queue_r(107, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0df
	c.begin_step();
	latch_r(c, 45);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0df]) * r[45] + acc;
	c.queue_r(108, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(21, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0e0
	c.begin_step();
	c.queue_d(25, c.in[0][7]);
	latch_r(c, 26);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0e0]) * r[26];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0e1
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 21);
	y = (t[1] * c.src + acc) * 4.0;
	c.queue_r(26, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(23, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0e2
	c.begin_step();
	c.queue_d(28, c.in[1][7]);
	latch_r(c, 32);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0e2]) * r[32];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0e3
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 23);
	y = (t[3] * c.src + acc) * 4.0;
	c.queue_r(32, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(29, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0e4
	c.begin_step();
	c.queue_d(24, c.in[3][7]);
	latch_r(c, 38);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0e4]) * r[38];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0e5
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 24);
	y = (t[5] * c.src + acc) * 4.0;
	c.queue_r(38, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(21, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0e6
	c.begin_step();
	c.queue_d(10, c.lfo[7]);
	latch_r(c, 44);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x0e6]) * r[44];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0e7
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 21);
	y = (t[7] * c.src + acc) * 4.0;
	c.queue_r(44, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(23, yss236_core::clip(y, yss236_core::DSAT));
	// 0x0e8
	c.begin_step();
	latch_r(c, 27);
	latch_m(c, 23);
	y = t[1] * c.src + r[27];
	c.queue_r(27, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0e9
	c.begin_step();
	latch_r(c, 33);
	latch_m(c, 29);
	y = t[3] * c.src + r[33];
	c.queue_r(33, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0ea
	c.begin_step();
	latch_r(c, 39);
	latch_m(c, 21);
	y = t[5] * c.src + r[39];
	c.queue_r(39, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0eb
	c.begin_step();
	latch_r(c, 45);
	latch_m(c, 23);
	y = t[7] * c.src + r[45];
	c.queue_r(45, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0ec
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 32);
	y = (yss236_core::s16(c.coef[0x0ec]) * c.src + r[97]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x0ed
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 37);
	y = (yss236_core::s16(c.coef[0x0ed]) * c.src + r[100]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x0ee
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 38);
	y = (yss236_core::s16(c.coef[0x0ee]) * c.src + r[103]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 3, y);
	// 0x0ef
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 39);
	y = (yss236_core::s16(c.coef[0x0ef]) * c.src + r[106]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 4, y);
	// 0x0f0
	c.begin_step();
	c.out[7] = m[35];
	latch_r(c, 28);
	latch_m(c, 35);
	y = t[1] * r[28] - r[28];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0f1
	c.begin_step();
	latch_r(c, 27);
	latch_m(c, 0);
	y = t[1] * r[27] - acc;
	c.queue_r(28, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0f2
	c.begin_step();
	latch_r(c, 34);
	latch_m(c, 0);
	y = t[2] * r[34] - r[34];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0f3
	c.begin_step();
	latch_r(c, 33);
	latch_m(c, 0);
	y = t[2] * r[33] - acc;
	c.queue_r(34, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0f4
	c.begin_step();
	latch_r(c, 40);
	latch_m(c, 0);
	y = t[3] * r[40] - r[40];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0f5
	c.begin_step();
	latch_r(c, 39);
	latch_m(c, 0);
	y = t[3] * r[39] - acc;
	c.queue_r(40, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0f6
	c.begin_step();
	latch_r(c, 46);
	latch_m(c, 0);
	y = t[4] * r[46] - r[46];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0f7
	c.begin_step();
	latch_r(c, 45);
	latch_m(c, 0);
	y = t[4] * r[45] - acc;
	c.queue_r(46, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0f8
	c.begin_step();
	latch_r(c, 29);
	latch_m(c, 0);
	y = t[1] * r[29] - r[29];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0f9
	c.begin_step();
	latch_r(c, 28);
	latch_m(c, 0);
	y = t[1] * r[28] - acc;
	c.queue_r(29, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0fa
	c.begin_step();
	latch_r(c, 35);
	latch_m(c, 0);
	y = t[2] * r[35] - r[35];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0fb
	c.begin_step();
	latch_r(c, 34);
	latch_m(c, 0);
	y = t[2] * r[34] - acc;
	c.queue_r(35, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0fc
	c.begin_step();
	latch_r(c, 41);
	latch_m(c, 0);
	y = t[3] * r[41] - r[41];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0fd
	c.begin_step();
	latch_r(c, 40);
	latch_m(c, 0);
	y = t[3] * r[40] - acc;
	c.queue_r(41, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x0fe
	c.begin_step();
	latch_r(c, 47);
	latch_m(c, 0);
	y = t[4] * r[47] - r[47];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x0ff
	c.begin_step();
	latch_r(c, 46);
	latch_m(c, 0);
	y = t[4] * r[46] - acc;
	c.queue_r(47, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x100
	c.begin_step();
	c.queue_d(32, c.in[0][8]);
	latch_r(c, 30);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x100]) + r[30];
	c.queue_r(30, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 1, c.acc);
	// 0x101
	c.begin_step();
	c.queue_d(33, c.in[1][8]);
	latch_r(c, 36);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x101]) + r[36];
	c.queue_r(36, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 2, c.acc);
	// 0x102
	c.begin_step();
	c.queue_d(29, c.in[3][8]);
	latch_r(c, 42);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x102]) + r[42];
	c.queue_r(42, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 1, c.acc);
	// 0x103
	c.begin_step();
	c.out[8] = m[17];
	latch_r(c, 48);
	latch_m(c, 17);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x103]) + r[48];
	c.queue_r(48, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 2, c.acc);
	// 0x104
	c.begin_step();
	latch_r(c, c.ops[0x104].sr);
	latch_m(c, 0);
	y = (t[1] * r[c.ops[0x104].sr]) * yss236_core::RSCALE[c.ops[0x104].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(17, yss236_core::clip(y, yss236_core::DSAT));
	// 0x105
	c.begin_step();
	latch_r(c, c.ops[0x105].sr);
	latch_m(c, 0);
	y = (t[2] * r[c.ops[0x105].sr]) * yss236_core::RSCALE[c.ops[0x105].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(21, yss236_core::clip(y, yss236_core::DSAT));
	// 0x106
	c.begin_step();
	latch_r(c, c.ops[0x106].sr);
	latch_m(c, 0);
	y = (t[1] * r[c.ops[0x106].sr]) * yss236_core::RSCALE[c.ops[0x106].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(22, yss236_core::clip(y, yss236_core::DSAT));
	// 0x107
	c.begin_step();
	latch_r(c, c.ops[0x107].sr);
	latch_m(c, 0);
	y = (t[2] * r[c.ops[0x107].sr]) * yss236_core::RSCALE[c.ops[0x107].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(23, yss236_core::clip(y, yss236_core::DSAT));

	// ---- page 264: channels 8 to 11 ----
	// 0x108
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 22);
	y = yss236_core::s16(c.coef[0x108]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x109
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 9);
	y = (yss236_core::s16(c.coef[0x109]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x10a
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x10a]) + acc;
	c.queue_r(97, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x10b
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 24);
	y = yss236_core::s16(c.coef[0x10b]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x10c
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 10);
	y = (yss236_core::s16(c.coef[0x10c]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x10d
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x10d]) + acc;
	c.queue_r(98, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x10e
	c.begin_step();
	c.queue_d(11, c.lfo[8]);
	latch_r(c, 127);
	latch_m(c, 29);
	y = yss236_core::s16(c.coef[0x10e]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x10f
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 11);
	y = (yss236_core::s16(c.coef[0x10f]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x110
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x110]) + acc;
	c.queue_r(99, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x111
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 34);
	y = yss236_core::s16(c.coef[0x111]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x112
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 12);
	y = (yss236_core::s16(c.coef[0x112]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x113
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x113]) + acc;
	c.queue_r(97, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x114
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x114]) * r[97] + m[62]) * 2.0;
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x115
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x115]) * r[98] + m[62]) * 2.0;
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x116
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x116]) * r[99] + m[62]) * 2.0;
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x117
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x117]) * r[97] + m[62]) * 2.0;
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x118
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 61);
	y = t[1] * r[98] + m[61];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x119
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 61);
	y = t[2] * r[97] + m[61];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x11a
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 61);
	y = t[1] * r[98] + m[61];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x11b
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 61);
	y = t[2] * r[97] + m[61];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x11c
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x11d
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x11e
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x11f
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(99, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x120
	c.begin_step();
	c.queue_d(24, c.in[0][9]);
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x121
	c.begin_step();
	c.queue_d(18, c.in[1][9]);
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(100, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x122
	c.begin_step();
	c.queue_d(34, c.in[3][9]);
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(103, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 3, y);
	// 0x123
	c.begin_step();
	c.out[9] = m[18];
	latch_r(c, 99);
	latch_m(c, 18);
	y = t[2] * r[99];
	c.queue_r(106, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 4, y);
	// 0x124
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x124]) * r[97] - m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x125
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x125]) * r[100] - m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x126
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x126]) * r[103] - m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x127
	c.begin_step();
	c.queue_d(12, c.lfo[9]);
	latch_r(c, 106);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x127]) * r[106] - m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x128
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x128]) * r[98] + m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x129
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x129]) * r[99] + m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x12a
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x12a]) * r[98] + m[63];
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x12b
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x12b]) * r[99] + m[63];
	c.queue_r(102, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x12c
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[1] * r[97];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(29, yss236_core::clip(y, yss236_core::DSAT));
	// 0x12d
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 0);
	y = t[2] * r[100];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(37, yss236_core::clip(y, yss236_core::DSAT));
	// 0x12e
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 0);
	y = t[3] * r[103];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(38, yss236_core::clip(y, yss236_core::DSAT));
	// 0x12f
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 0);
	y = t[4] * r[106];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(39, yss236_core::clip(y, yss236_core::DSAT));
	// 0x130
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x130]) - r[98]) * 2.0;
	c.queue_r(104, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x131
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x131]) - r[99]) * 2.0;
	c.queue_r(105, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x132
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x132]) - r[101]) * 2.0;
	c.queue_r(104, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x133
	c.begin_step();
	latch_r(c, 102);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x133]) - r[102]) * 2.0;
	c.queue_r(105, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x134
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x134]) * r[104];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 1, y);
	// 0x135
	c.begin_step();
	latch_r(c, 105);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x135]) * r[105];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x136
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x136]) * r[104];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 1, y);
	// 0x137
	c.begin_step();
	latch_r(c, 105);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x137]) * r[105];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x138
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 29);
	y = t[1] * c.src + r[97];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(35, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 1, y);
	// 0x139
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 37);
	y = t[2] * c.src + r[100];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(40, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 3, y);
	// 0x13a
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 38);
	y = t[1] * c.src + r[103];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(27, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 5, y);
	// 0x13b
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 39);
	y = t[2] * c.src + r[106];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(41, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 7, y);
	// 0x13c
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 27);
	y = yss236_core::s16(c.coef[0x13c]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x13d
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 31);
	y = (yss236_core::s16(c.coef[0x13d]) * c.src + acc) * 4.0;
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x13e
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 35);
	y = r[98] - m[35];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x13e, 8);
	// 0x13f
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 35);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x140
	c.begin_step();
	c.queue_d(31, c.in[0][10]);
	latch_r(c, 0);
	latch_m(c, 25);
	y = yss236_core::s16(c.coef[0x140]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x141
	c.begin_step();
	c.queue_d(35, c.in[1][10]);
	latch_r(c, 0);
	latch_m(c, 28);
	y = (yss236_core::s16(c.coef[0x141]) * c.src + acc) * 4.0;
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x142
	c.begin_step();
	c.queue_d(28, c.in[3][10]);
	latch_r(c, 99);
	latch_m(c, 40);
	y = r[99] - m[40];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x142, 9);
	// 0x143
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 40);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 4, y);
	// 0x144
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 32);
	y = yss236_core::s16(c.coef[0x144]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x145
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 33);
	y = (yss236_core::s16(c.coef[0x145]) * c.src + acc) * 4.0;
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x146
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 27);
	y = r[101] - m[27];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x146, 10);
	// 0x147
	c.begin_step();
	c.queue_d(13, c.lfo[10]);
	latch_r(c, 0);
	latch_m(c, 27);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 6, y);
	// 0x148
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 24);
	y = yss236_core::s16(c.coef[0x148]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x149
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 18);
	y = (yss236_core::s16(c.coef[0x149]) * c.src + acc) * 4.0;
	c.queue_r(104, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x14a
	c.begin_step();
	latch_r(c, 102);
	latch_m(c, 41);
	y = r[102] - m[41];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x14a, 11);
	// 0x14b
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 41);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 8, y);
	// 0x14c
	c.begin_step();
	c.out[10] = m[19];
	latch_r(c, 98);
	latch_m(c, 19);
	y = t[8] * r[98] - r[98];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x14d
	c.begin_step();
	latch_r(c, 49);
	latch_m(c, 0);
	y = t[8] * r[49] - acc;
	c.queue_r(49, c.haspx ? c.px : r[49]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x14e
	c.begin_step();
	latch_r(c, 50);
	latch_m(c, 0);
	y = t[2] * r[50] + acc;
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x14f
	c.begin_step();
	latch_r(c, 51);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x14f]) * r[51] + acc;
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(18, yss236_core::clip(y, yss236_core::DSAT));
	// 0x150
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	y = t[9] * r[99] - r[99];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x151
	c.begin_step();
	latch_r(c, 55);
	latch_m(c, 0);
	y = t[9] * r[55] - acc;
	c.queue_r(55, c.haspx ? c.px : r[55]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x152
	c.begin_step();
	latch_r(c, 56);
	latch_m(c, 0);
	y = t[4] * r[56] + acc;
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x153
	c.begin_step();
	latch_r(c, 57);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x153]) * r[57] + acc;
	c.queue_r(102, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(19, yss236_core::clip(y, yss236_core::DSAT));
	// 0x154
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 0);
	y = t[10] * r[101] - r[101];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x155
	c.begin_step();
	latch_r(c, 61);
	latch_m(c, 0);
	y = t[10] * r[61] - acc;
	c.queue_r(61, c.haspx ? c.px : r[61]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x156
	c.begin_step();
	latch_r(c, 62);
	latch_m(c, 0);
	y = t[6] * r[62] + acc;
	c.queue_r(104, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x157
	c.begin_step();
	latch_r(c, 63);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x157]) * r[63] + acc;
	c.queue_r(105, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(24, yss236_core::clip(y, yss236_core::DSAT));
	// 0x158
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = t[11] * r[104] - r[104];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x159
	c.begin_step();
	latch_r(c, 67);
	latch_m(c, 0);
	y = t[11] * r[67] - acc;
	c.queue_r(67, c.haspx ? c.px : r[67]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x15a
	c.begin_step();
	latch_r(c, 68);
	latch_m(c, 0);
	y = t[8] * r[68] + acc;
	c.queue_r(107, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x15b
	c.begin_step();
	latch_r(c, 69);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x15b]) * r[69] + acc;
	c.queue_r(108, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(18, yss236_core::clip(y, yss236_core::DSAT));
	// 0x15c
	c.begin_step();
	latch_r(c, 50);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x15c]) * r[50];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x15d
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 18);
	y = (t[1] * c.src + acc) * 4.0;
	c.queue_r(50, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(25, yss236_core::clip(y, yss236_core::DSAT));
	// 0x15e
	c.begin_step();
	latch_r(c, 56);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x15e]) * r[56];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x15f
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 19);
	y = (t[3] * c.src + acc) * 4.0;
	c.queue_r(56, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(27, yss236_core::clip(y, yss236_core::DSAT));
	// 0x160
	c.begin_step();
	c.queue_d(19, c.in[0][11]);
	latch_r(c, 62);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x160]) * r[62];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x161
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 24);
	y = (t[5] * c.src + acc) * 4.0;
	c.queue_r(62, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(18, yss236_core::clip(y, yss236_core::DSAT));
	// 0x162
	c.begin_step();
	c.queue_d(24, c.in[1][11]);
	latch_r(c, 68);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x162]) * r[68];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x163
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 18);
	y = (t[7] * c.src + acc) * 4.0;
	c.queue_r(68, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(25, yss236_core::clip(y, yss236_core::DSAT));
	// 0x164
	c.begin_step();
	c.queue_d(18, c.in[3][11]);
	latch_r(c, 51);
	latch_m(c, 25);
	y = t[1] * c.src + r[51];
	c.queue_r(51, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x165
	c.begin_step();
	latch_r(c, 57);
	latch_m(c, 27);
	y = t[3] * c.src + r[57];
	c.queue_r(57, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x166
	c.begin_step();
	c.queue_d(14, c.lfo[11]);
	latch_r(c, 63);
	latch_m(c, 18);
	y = t[5] * c.src + r[63];
	c.queue_r(63, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x167
	c.begin_step();
	latch_r(c, 69);
	latch_m(c, 25);
	y = t[7] * c.src + r[69];
	c.queue_r(69, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x168
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 29);
	y = (yss236_core::s16(c.coef[0x168]) * c.src + r[97]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x169
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 37);
	y = (yss236_core::s16(c.coef[0x169]) * c.src + r[100]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x16a
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 38);
	y = (yss236_core::s16(c.coef[0x16a]) * c.src + r[103]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 3, y);
	// 0x16b
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 39);
	y = (yss236_core::s16(c.coef[0x16b]) * c.src + r[106]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 4, y);
	// 0x16c
	c.begin_step();
	c.out[11] = m[20];
	latch_r(c, 52);
	latch_m(c, 20);
	y = t[1] * r[52] - r[52];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x16d
	c.begin_step();
	latch_r(c, 51);
	latch_m(c, 0);
	y = t[1] * r[51] - acc;
	c.queue_r(52, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x16e
	c.begin_step();
	latch_r(c, 58);
	latch_m(c, 0);
	y = t[2] * r[58] - r[58];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x16f
	c.begin_step();
	latch_r(c, 57);
	latch_m(c, 0);
	y = t[2] * r[57] - acc;
	c.queue_r(58, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x170
	c.begin_step();
	latch_r(c, 64);
	latch_m(c, 0);
	y = t[3] * r[64] - r[64];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x171
	c.begin_step();
	latch_r(c, 63);
	latch_m(c, 0);
	y = t[3] * r[63] - acc;
	c.queue_r(64, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x172
	c.begin_step();
	latch_r(c, 70);
	latch_m(c, 0);
	y = t[4] * r[70] - r[70];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x173
	c.begin_step();
	latch_r(c, 69);
	latch_m(c, 0);
	y = t[4] * r[69] - acc;
	c.queue_r(70, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x174
	c.begin_step();
	latch_r(c, 53);
	latch_m(c, 0);
	y = t[1] * r[53] - r[53];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x175
	c.begin_step();
	latch_r(c, 52);
	latch_m(c, 0);
	y = t[1] * r[52] - acc;
	c.queue_r(53, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x176
	c.begin_step();
	latch_r(c, 59);
	latch_m(c, 0);
	y = t[2] * r[59] - r[59];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x177
	c.begin_step();
	latch_r(c, 58);
	latch_m(c, 0);
	y = t[2] * r[58] - acc;
	c.queue_r(59, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x178
	c.begin_step();
	latch_r(c, 65);
	latch_m(c, 0);
	y = t[3] * r[65] - r[65];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x179
	c.begin_step();
	latch_r(c, 64);
	latch_m(c, 0);
	y = t[3] * r[64] - acc;
	c.queue_r(65, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x17a
	c.begin_step();
	latch_r(c, 71);
	latch_m(c, 0);
	y = t[4] * r[71] - r[71];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x17b
	c.begin_step();
	latch_r(c, 70);
	latch_m(c, 0);
	y = t[4] * r[70] - acc;
	c.queue_r(71, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x17c
	c.begin_step();
	latch_r(c, 54);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x17c]) + r[54];
	c.queue_r(54, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 1, c.acc);
	// 0x17d
	c.begin_step();
	latch_r(c, 60);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x17d]) + r[60];
	c.queue_r(60, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 2, c.acc);
	// 0x17e
	c.begin_step();
	latch_r(c, 66);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x17e]) + r[66];
	c.queue_r(66, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 1, c.acc);
	// 0x17f
	c.begin_step();
	latch_r(c, 72);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x17f]) + r[72];
	c.queue_r(72, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 2, c.acc);
	// 0x180
	c.begin_step();
	c.out[12] = m[17];
	latch_r(c, c.ops[0x180].sr);
	latch_m(c, 17);
	y = (t[1] * r[c.ops[0x180].sr]) * yss236_core::RSCALE[c.ops[0x180].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(25, yss236_core::clip(y, yss236_core::DSAT));
	// 0x181
	c.begin_step();
	latch_r(c, c.ops[0x181].sr);
	latch_m(c, 0);
	y = (t[2] * r[c.ops[0x181].sr]) * yss236_core::RSCALE[c.ops[0x181].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(27, yss236_core::clip(y, yss236_core::DSAT));
	// 0x182
	c.begin_step();
	latch_r(c, c.ops[0x182].sr);
	latch_m(c, 0);
	y = (t[1] * r[c.ops[0x182].sr]) * yss236_core::RSCALE[c.ops[0x182].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(28, yss236_core::clip(y, yss236_core::DSAT));
	// 0x183
	c.begin_step();
	latch_r(c, c.ops[0x183].sr);
	latch_m(c, 0);
	y = (t[2] * r[c.ops[0x183].sr]) * yss236_core::RSCALE[c.ops[0x183].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(29, yss236_core::clip(y, yss236_core::DSAT));

	// ---- page 388: channels 12 to 15 ----
	// 0x184
	c.begin_step();
	c.queue_d(20, c.in[0][12]);
	latch_r(c, 127);
	latch_m(c, 28);
	y = yss236_core::s16(c.coef[0x184]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x185
	c.begin_step();
	c.queue_d(18, c.in[1][12]);
	latch_r(c, 0);
	latch_m(c, 13);
	y = (yss236_core::s16(c.coef[0x185]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x186
	c.begin_step();
	c.queue_d(17, c.in[3][12]);
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x186]) + acc;
	c.queue_r(97, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x187
	c.begin_step();
	c.queue_d(15, c.lfo[12]);
	latch_r(c, 127);
	latch_m(c, 18);
	y = yss236_core::s16(c.coef[0x187]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x188
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 14);
	y = (yss236_core::s16(c.coef[0x188]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x189
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x189]) + acc;
	c.queue_r(98, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x18a
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 17);
	y = yss236_core::s16(c.coef[0x18a]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x18b
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 15);
	y = (yss236_core::s16(c.coef[0x18b]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x18c
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x18c]) + acc;
	c.queue_r(99, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x18d
	c.begin_step();
	latch_r(c, 127);
	latch_m(c, 36);
	y = yss236_core::s16(c.coef[0x18d]) * c.src + r[127];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x18e
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 16);
	y = (yss236_core::s16(c.coef[0x18e]) * c.src + acc) * 2.0;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x18f
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x18f]) + acc;
	c.queue_r(97, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	// 0x190
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x190]) * r[97] + m[62]) * 2.0;
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x191
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x191]) * r[98] + m[62]) * 2.0;
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x192
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x192]) * r[99] + m[62]) * 2.0;
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x193
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 62);
	y = (yss236_core::s16(c.coef[0x193]) * r[97] + m[62]) * 2.0;
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x194
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 61);
	y = t[1] * r[98] + m[61];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x195
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 61);
	y = t[2] * r[97] + m[61];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x196
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 61);
	y = t[1] * r[98] + m[61];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x197
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 61);
	y = t[2] * r[97] + m[61];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x198
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x199
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x19a
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(98, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x19b
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(99, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x19c
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(97, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x19d
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 0);
	y = t[2] * r[97];
	c.queue_r(100, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x19e
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	y = t[1] * r[98];
	c.queue_r(103, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 3, y);
	// 0x19f
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	y = t[2] * r[99];
	c.queue_r(106, yss236_core::mode(y, 2));
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 4, y);
	// 0x1a0
	c.begin_step();
	c.queue_d(17, c.in[0][13]);
	latch_r(c, 97);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x1a0]) * r[97] - m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1a1
	c.begin_step();
	c.queue_d(32, c.in[1][13]);
	latch_r(c, 100);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x1a1]) * r[100] - m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1a2
	c.begin_step();
	c.queue_d(36, c.in[3][13]);
	latch_r(c, 103);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x1a2]) * r[103] - m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1a3
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x1a3]) * r[106] - m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1a4
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x1a4]) * r[98] + m[63];
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1a5
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x1a5]) * r[99] + m[63];
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1a6
	c.begin_step();
	c.queue_d(16, c.lfo[13]);
	latch_r(c, 98);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x1a6]) * r[98] + m[63];
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1a7
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 63);
	y = yss236_core::s16(c.coef[0x1a7]) * r[99] + m[63];
	c.queue_r(102, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1a8
	c.begin_step();
	c.out[13] = m[21];
	latch_r(c, 97);
	latch_m(c, 21);
	y = t[1] * r[97];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(33, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1a9
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 0);
	y = t[2] * r[100];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(37, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1aa
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 0);
	y = t[3] * r[103];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(38, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1ab
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 0);
	y = t[4] * r[106];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(39, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1ac
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x1ac]) - r[98]) * 2.0;
	c.queue_r(104, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x1ad
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x1ad]) - r[99]) * 2.0;
	c.queue_r(105, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x1ae
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x1ae]) - r[101]) * 2.0;
	c.queue_r(104, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x1af
	c.begin_step();
	latch_r(c, 102);
	latch_m(c, 0);
	v = (yss236_core::LOADK * yss236_core::s16(c.coef[0x1af]) - r[102]) * 2.0;
	c.queue_r(105, yss236_core::mode(v, 1));
	c.acc = yss236_core::accmode(v, 1);
	// 0x1b0
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1b0]) * r[104];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 1, y);
	// 0x1b1
	c.begin_step();
	latch_r(c, 105);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1b1]) * r[105];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x1b2
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1b2]) * r[104];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 1, y);
	// 0x1b3
	c.begin_step();
	latch_r(c, 105);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1b3]) * r[105];
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x1b4
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 33);
	y = t[1] * c.src + r[97];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(21, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 1, y);
	// 0x1b5
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 37);
	y = t[2] * c.src + r[100];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(40, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 3, y);
	// 0x1b6
	c.begin_step();
	latch_r(c, 103);
	latch_m(c, 38);
	y = t[1] * c.src + r[103];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(31, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 5, y);
	// 0x1b7
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 39);
	y = t[2] * c.src + r[106];
	c.acc = y = yss236_core::accmode(y, 2);
	c.queue_d(35, yss236_core::clip(y, yss236_core::DSAT));
	tput(c, 7, y);
	// 0x1b8
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 31);
	y = yss236_core::s16(c.coef[0x1b8]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1b9
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 35);
	y = (yss236_core::s16(c.coef[0x1b9]) * c.src + acc) * 4.0;
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1ba
	c.begin_step();
	latch_r(c, 98);
	latch_m(c, 21);
	y = r[98] - m[21];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x1ba, 8);
	// 0x1bb
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 21);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 2, y);
	// 0x1bc
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 19);
	y = yss236_core::s16(c.coef[0x1bc]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1bd
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 24);
	y = (yss236_core::s16(c.coef[0x1bd]) * c.src + acc) * 4.0;
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1be
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 40);
	y = r[99] - m[40];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x1be, 9);
	// 0x1bf
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 40);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 4, y);
	// 0x1c0
	c.begin_step();
	c.queue_d(18, c.in[0][14]);
	latch_r(c, 0);
	latch_m(c, 20);
	y = yss236_core::s16(c.coef[0x1c0]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1c1
	c.begin_step();
	c.queue_d(19, c.in[1][14]);
	latch_r(c, 0);
	latch_m(c, 18);
	y = (yss236_core::s16(c.coef[0x1c1]) * c.src + acc) * 4.0;
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1c2
	c.begin_step();
	c.queue_d(17, c.in[3][14]);
	latch_r(c, 101);
	latch_m(c, 31);
	y = r[101] - m[31];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x1c2, 10);
	// 0x1c3
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 31);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 6, y);
	// 0x1c4
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 17);
	y = yss236_core::s16(c.coef[0x1c4]) * c.src;
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1c5
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 32);
	y = (yss236_core::s16(c.coef[0x1c5]) * c.src + acc) * 4.0;
	c.queue_r(104, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1c6
	c.begin_step();
	c.queue_d(1, c.lfo[14]);
	latch_r(c, 102);
	latch_m(c, 35);
	y = r[102] - m[35];
	c.acc = y = yss236_core::accmode(y, 1);
	tput_k(c, 0x1c6, 11);
	// 0x1c7
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 35);
	y = c.src + acc;
	c.acc = y = yss236_core::accmode(y, 1);
	tput(c, 8, y);
	// 0x1c8
	c.begin_step();
	c.out[14] = m[22];
	latch_r(c, 98);
	latch_m(c, 22);
	y = t[8] * r[98] - r[98];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1c9
	c.begin_step();
	latch_r(c, 73);
	latch_m(c, 0);
	y = t[8] * r[73] - acc;
	c.queue_r(73, c.haspx ? c.px : r[73]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1ca
	c.begin_step();
	latch_r(c, 74);
	latch_m(c, 0);
	y = t[2] * r[74] + acc;
	c.queue_r(98, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1cb
	c.begin_step();
	latch_r(c, 75);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1cb]) * r[75] + acc;
	c.queue_r(99, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(20, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1cc
	c.begin_step();
	latch_r(c, 99);
	latch_m(c, 0);
	y = t[9] * r[99] - r[99];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1cd
	c.begin_step();
	latch_r(c, 79);
	latch_m(c, 0);
	y = t[9] * r[79] - acc;
	c.queue_r(79, c.haspx ? c.px : r[79]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1ce
	c.begin_step();
	latch_r(c, 80);
	latch_m(c, 0);
	y = t[4] * r[80] + acc;
	c.queue_r(101, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1cf
	c.begin_step();
	latch_r(c, 81);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1cf]) * r[81] + acc;
	c.queue_r(102, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(21, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1d0
	c.begin_step();
	latch_r(c, 101);
	latch_m(c, 0);
	y = t[10] * r[101] - r[101];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1d1
	c.begin_step();
	latch_r(c, 85);
	latch_m(c, 0);
	y = t[10] * r[85] - acc;
	c.queue_r(85, c.haspx ? c.px : r[85]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1d2
	c.begin_step();
	latch_r(c, 86);
	latch_m(c, 0);
	y = t[6] * r[86] + acc;
	c.queue_r(104, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1d3
	c.begin_step();
	latch_r(c, 87);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1d3]) * r[87] + acc;
	c.queue_r(105, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(22, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1d4
	c.begin_step();
	latch_r(c, 104);
	latch_m(c, 0);
	y = t[11] * r[104] - r[104];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1d5
	c.begin_step();
	latch_r(c, 91);
	latch_m(c, 0);
	y = t[11] * r[91] - acc;
	c.queue_r(91, c.haspx ? c.px : r[91]);
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1d6
	c.begin_step();
	latch_r(c, 92);
	latch_m(c, 0);
	y = t[8] * r[92] + acc;
	c.queue_r(107, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1d7
	c.begin_step();
	latch_r(c, 93);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1d7]) * r[93] + acc;
	c.queue_r(108, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(20, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1d8
	c.begin_step();
	latch_r(c, 74);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1d8]) * r[74];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1d9
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 20);
	y = (t[1] * c.src + acc) * 4.0;
	c.queue_r(74, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(21, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1da
	c.begin_step();
	latch_r(c, 80);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1da]) * r[80];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1db
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 21);
	y = (t[3] * c.src + acc) * 4.0;
	c.queue_r(80, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(22, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1dc
	c.begin_step();
	latch_r(c, 86);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1dc]) * r[86];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1dd
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 22);
	y = (t[5] * c.src + acc) * 4.0;
	c.queue_r(86, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(20, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1de
	c.begin_step();
	latch_r(c, 92);
	latch_m(c, 0);
	y = yss236_core::s16(c.coef[0x1de]) * r[92];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1df
	c.begin_step();
	latch_r(c, 0);
	latch_m(c, 20);
	y = (t[7] * c.src + acc) * 4.0;
	c.queue_r(92, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(22, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1e0
	c.begin_step();
	c.queue_d(21, c.in[0][15]);
	latch_r(c, 75);
	latch_m(c, 21);
	y = t[1] * c.src + r[75];
	c.queue_r(75, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1e1
	c.begin_step();
	c.queue_d(22, c.in[1][15]);
	latch_r(c, 81);
	latch_m(c, 22);
	y = t[3] * c.src + r[81];
	c.queue_r(81, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1e2
	c.begin_step();
	c.queue_d(20, c.in[3][15]);
	latch_r(c, 87);
	latch_m(c, 20);
	y = t[5] * c.src + r[87];
	c.queue_r(87, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1e3
	c.begin_step();
	latch_r(c, 93);
	latch_m(c, 22);
	y = t[7] * c.src + r[93];
	c.queue_r(93, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1e4
	c.begin_step();
	latch_r(c, 97);
	latch_m(c, 33);
	y = (yss236_core::s16(c.coef[0x1e4]) * c.src + r[97]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 1, y);
	// 0x1e5
	c.begin_step();
	latch_r(c, 100);
	latch_m(c, 37);
	y = (yss236_core::s16(c.coef[0x1e5]) * c.src + r[100]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 2, y);
	// 0x1e6
	c.begin_step();
	c.queue_d(2, c.lfo[15]);
	latch_r(c, 103);
	latch_m(c, 38);
	y = (yss236_core::s16(c.coef[0x1e6]) * c.src + r[103]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 3, y);
	// 0x1e7
	c.begin_step();
	latch_r(c, 106);
	latch_m(c, 39);
	y = (yss236_core::s16(c.coef[0x1e7]) * c.src + r[106]) * 4.0;
	c.acc = y = yss236_core::accmode(y, 2);
	tput(c, 4, y);
	// 0x1e8
	c.begin_step();
	c.out[15] = m[23];
	latch_r(c, 76);
	latch_m(c, 23);
	y = t[1] * r[76] - r[76];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1e9
	c.begin_step();
	latch_r(c, 75);
	latch_m(c, 0);
	y = t[1] * r[75] - acc;
	c.queue_r(76, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1ea
	c.begin_step();
	latch_r(c, 82);
	latch_m(c, 0);
	y = t[2] * r[82] - r[82];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1eb
	c.begin_step();
	latch_r(c, 81);
	latch_m(c, 0);
	y = t[2] * r[81] - acc;
	c.queue_r(82, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1ec
	c.begin_step();
	latch_r(c, 88);
	latch_m(c, 0);
	y = t[3] * r[88] - r[88];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1ed
	c.begin_step();
	latch_r(c, 87);
	latch_m(c, 0);
	y = t[3] * r[87] - acc;
	c.queue_r(88, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1ee
	c.begin_step();
	latch_r(c, 94);
	latch_m(c, 0);
	y = t[4] * r[94] - r[94];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1ef
	c.begin_step();
	latch_r(c, 93);
	latch_m(c, 0);
	y = t[4] * r[93] - acc;
	c.queue_r(94, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1f0
	c.begin_step();
	latch_r(c, 77);
	latch_m(c, 0);
	y = t[1] * r[77] - r[77];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1f1
	c.begin_step();
	latch_r(c, 76);
	latch_m(c, 0);
	y = t[1] * r[76] - acc;
	c.queue_r(77, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1f2
	c.begin_step();
	latch_r(c, 83);
	latch_m(c, 0);
	y = t[2] * r[83] - r[83];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1f3
	c.begin_step();
	latch_r(c, 82);
	latch_m(c, 0);
	y = t[2] * r[82] - acc;
	c.queue_r(83, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1f4
	c.begin_step();
	latch_r(c, 89);
	latch_m(c, 0);
	y = t[3] * r[89] - r[89];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1f5
	c.begin_step();
	latch_r(c, 88);
	latch_m(c, 0);
	y = t[3] * r[88] - acc;
	c.queue_r(89, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1f6
	c.begin_step();
	latch_r(c, 95);
	latch_m(c, 0);
	y = t[4] * r[95] - r[95];
	c.acc = y = yss236_core::accmode(y, 0);
	// 0x1f7
	c.begin_step();
	latch_r(c, 94);
	latch_m(c, 0);
	y = t[4] * r[94] - acc;
	c.queue_r(95, yss236_core::mode(y, 1));
	c.acc = y = yss236_core::accmode(y, 1);
	// 0x1f8
	c.begin_step();
	latch_r(c, 78);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x1f8]) + r[78];
	c.queue_r(78, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 1, c.acc);
	// 0x1f9
	c.begin_step();
	latch_r(c, 84);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x1f9]) + r[84];
	c.queue_r(84, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 2, c.acc);
	// 0x1fa
	c.begin_step();
	latch_r(c, 90);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x1fa]) + r[90];
	c.queue_r(90, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 1, c.acc);
	// 0x1fb
	c.begin_step();
	latch_r(c, 96);
	latch_m(c, 0);
	v = yss236_core::LOADK * yss236_core::s16(c.coef[0x1fb]) + r[96];
	c.queue_r(96, yss236_core::mode(v, 2));
	c.acc = yss236_core::accmode(v, 2);
	tput(c, 2, c.acc);
	// 0x1fc
	c.begin_step();
	latch_r(c, c.ops[0x1fc].sr);
	latch_m(c, 0);
	y = (t[1] * r[c.ops[0x1fc].sr]) * yss236_core::RSCALE[c.ops[0x1fc].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(31, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1fd
	c.begin_step();
	latch_r(c, c.ops[0x1fd].sr);
	latch_m(c, 0);
	y = (t[2] * r[c.ops[0x1fd].sr]) * yss236_core::RSCALE[c.ops[0x1fd].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(32, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1fe
	c.begin_step();
	latch_r(c, c.ops[0x1fe].sr);
	latch_m(c, 0);
	y = (t[1] * r[c.ops[0x1fe].sr]) * yss236_core::RSCALE[c.ops[0x1fe].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(33, yss236_core::clip(y, yss236_core::DSAT));
	// 0x1ff
	c.begin_step();
	latch_r(c, c.ops[0x1ff].sr);
	latch_m(c, 0);
	y = (t[2] * r[c.ops[0x1ff].sr]) * yss236_core::RSCALE[c.ops[0x1ff].shift];
	c.acc = y = yss236_core::accmode(y, 1);
	c.queue_d(35, yss236_core::clip(y, yss236_core::DSAT));
}

}  // namespace fs1r::native
