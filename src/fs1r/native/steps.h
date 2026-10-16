// fs1r/native/steps.h - the shapes every step of the chip's programs has. Ours.
//
// yss236_core::step() is the definition of a step: it decodes five program words and moves the
// pipeline. Every step of every VOP3 program is one of a dozen shapes, and this header is those
// dozen, with the program words of a row already read and named the way step() names them, so a
// program in src/fs1r/native/ reads as the arithmetic it computes instead of as its own plumbing.
//
// Nothing here decides anything. Every constant is read from c.coef[st] on the pass, as step()
// reads it, every delay offset through c.addr() off the c.slot[] entry the host last wrote, and the
// shapes follow step()'s order exactly, so a program built from these leaves the core bit for bit as
// the interpreter would. docs/vop3_native/README.md is the brief.
//
// The names are the chip's, not ours: a latch, a multiplier, an adder, a route, a clamp, a capture,
// a z^-1. The DSP a program's steps add up to is named in the program, not here.

#pragma once

#include <cmath>

#include "fs1r/chips/yss236_core.h"

namespace fs1r::native::step {

// ---------------------------------------------------------------------------------------------
// The fields a step's words fix, named as w3 and w4 name them
// ---------------------------------------------------------------------------------------------

// What the multiplier scales (w3[8], and w3[3:4] for the word): the register the step read, or the
// cell it latched. s18: a second input of m[sm] is how a step reads a value another step published.
enum m2_t { M2_REG, M2_CELL };

// What the adder adds to the product (w3[7:6] with w3[10]): nothing, the running value, the register
// the step just read, or the cell it just latched. Almost every step after the first of a sum is
// ADD_ACC, which is what makes the running value a sum.
enum add_t { ADD_OFF, ADD_ACC, ADD_REG, ADD_CELL };

// The result's route (w3[13:12]): s24 measured x1, x2, x4 and x16, and the designer's own
// (16 >> scale) is what a closing step's route takes back out.
enum route_t { ROUTE_1, ROUTE_2, ROUTE_4, ROUTE_16 };

// The result's clamp (w3[15:14]): wrap two's complement at +-128, saturate, hold the negatives at
// zero, or take the absolute value.
enum clamp_t { CLAMP_WRAP, CLAMP_SAT, CLAMP_ZERO, CLAMP_ABS };

// ---------------------------------------------------------------------------------------------
// The two latches, which every step moves
// ---------------------------------------------------------------------------------------------

// x takes r[sr] and the step before's x goes to px. That pair is the z^-1 a copy step reads back,
// so a section keeps its x[-1] and y[-2] without a cell. hasx says whether x came from a register at
// all, and a step that reads no register (sr = 0) leaves it clear: s27 r12 fetches on every step.
inline void latch_reg(yss236_core& c, int sr)
{
	c.px = c.x;
	c.haspx = c.hasx;
	c.hasx = sr != 0;
	c.x = sr ? c.r[sr] : 0.0;
}

// src takes m[sm] and the step before's cell goes to psrc, which several steps then write back out as
// their own cell write: that chain of the previous operand is what gives the reverb's comb units a
// second input tap a step after the first.
inline void latch_cell(yss236_core& c, int sm)
{
	c.psrc = c.src;
	c.src = c.m[sm];
}

// ---------------------------------------------------------------------------------------------
// The cells taking a value from the hardware side, before the arithmetic
// ---------------------------------------------------------------------------------------------

// A cell takes a serial input. The slot is the step's own 32-step page: page * 2 + half, so bank 0 is
// the left channel and bank 1 the right. The cell lands DLAT steps later, so it is read by a step
// further down the program.
inline void take_serial(yss236_core& c, int dm, int st, int bank) { c.queue_d(dm, c.in[bank][st >> 5]); }

// A cell takes an LFO value, the one its page carries.
inline void take_lfo(yss236_core& c, int dm, int st) { c.queue_d(dm, c.lfo[st >> 5]); }

// A cell takes an EG level, which the hardware side keeps in the third input bank.
inline void take_eg(yss236_core& c, int dm, int st) { c.queue_d(dm, c.in[3][st >> 5]); }

// A cell takes up a delay RAM read: the latest transfer of this pass made CAPMIN to 11 steps before
// this one, which is what read_delay() pushed and captured() walks back through. Five steps is the
// nearest the hardware allows (s40c), so a read and its capture are never adjacent.
inline void take_capture(yss236_core& c, int dm, int st) { c.queue_d(dm, c.captured(c.m[dm], st)); }

// A cell takes up the delay RAM read and publishes it at the same time, which is how a tap reaches the
// registers a few steps later: one read, one history push and one capture.
inline void tap_delay(yss236_core& c, int dm, int st, bool older = false)
{
	c.queue_d(dm, c.captured(c.m[dm], st));
	c.push_hist(st, c.ram[c.addr(st >> 2, older ? 1 : 0, false)]);
}

// A delay RAM read on its own, at the ring pointer or one word older (memop 4 and 6), pushed into the
// capture history so a capture further down can take it up. The slot is the step's own over four, which
// is where step() takes it from, and its offset is the c.slot[] entry the host last wrote, never a
// number here.
inline void read_delay(yss236_core& c, int st, bool older = false)
{
	c.push_hist(st, c.ram[c.addr(st >> 2, older ? 1 : 0, false)]);
}

// A delay RAM write: the value takes the slot at the ring pointer, or one word indexed from the
// running value (w2[3]), which is what a tap at a moving position writes.
inline void write_delay(yss236_core& c, int st, double v, bool indexed = false)
{
	c.ram[c.addr(st >> 2, indexed ? c.cur_idx() : 0, false)] = v;
}

// A class-0 step that writes the delay RAM (s34): it writes the data a wdata step latched, and does
// nothing at all without one, since a class-0 step has no result of its own to write.
inline void write_delay_latched(yss236_core& c, int st)
{
	if (c.haswl)
	{
		c.ram[c.addr(st >> 2, 0, false)] = c.wlatch;
		c.haswl = false;
	}
}

// A cell goes to the serial output the step's page carries (w4[6]), so the base program picks it up.
inline void serial_out(yss236_core& c, int st, int sm) { c.out[st >> 5] = c.m[sm]; }

// ---------------------------------------------------------------------------------------------
// The multiplier-accumulator: what nearly every step that computes something does
// ---------------------------------------------------------------------------------------------

// The step's arithmetic, as one struct: the fields are the ones a row's program words fix, and each
// left out is that field's zero, so a step says only what is different about it.
//
// This is the general form. A program should reach for one of the named shapes below, which say what
// the step computes; this is for the step that does not fit one.
struct alu
{
	int st = 0;               // the step, whose word is c.coef[st]
	int sr = 0;               // the register the step reads (w4[13:7])
	int sm = 0;               // the cell the step latches (w4[5:0])
	m2_t m2 = M2_REG;         // what the multiplier scales
	bool unity = false;       // class 3: the word is 1.0 and the multiplier passes m2 through (s19)
	int treg = 0;             // the word is t[treg], a pole gain the t file holds (w3[3:4])
	add_t add = ADD_OFF;      // what the adder adds to the product
	bool sub = false;         // and the adder's input is negated (w3[10])
	route_t route = ROUTE_1;  // the result's route
	clamp_t clamp = CLAMP_WRAP;
	int dr = 0;               // the register the result is written to, 0 for none
	bool z1 = false;          // and it takes the step before's x operand instead: the z^-1 copy (w1[7])
	int dm = 0;               // the cell the result is written to, 0 for none
	bool prev = false;        // and it takes the step before's m operand (w2[15:13] = 7)
	bool cmp = false;         // the sign of the result becomes the compare flag (w2[0])
	bool wdata = false;       // the result becomes the data of the next delay RAM write (w1[13])
	bool write = false;       // and this step is that write: it takes the latched data if there is one
};

// One class-2 or class-3 step. The order is step()'s: the multiplier's word and its two inputs, then
// the adder, then the route, then the clamp into the register and the running value, then the cell
// the result goes to and the flag. Doubles do not associate, so a program cannot fold two of these
// steps into one expression.
inline double run(yss236_core& c, alu const& s)
{
	double const xv = s.sr ? c.r[s.sr] : 0.0;   // read before the latch moves it to px
	latch_reg(c, s.sr);
	latch_cell(c, s.sm);

	double const m2 = s.m2 == M2_CELL ? c.src : xv;
	double const g = s.unity ? 1.0 : s.treg ? c.t[s.treg] : yss236_core::s16(c.coef[s.st]);
	double const prod = s.unity ? m2 : g * m2;

	double a;
	switch (s.add)
	{
		case ADD_ACC:  a = c.acc; break;
		case ADD_REG:  a = s.sr ? c.x : c.acc / 32768.0; break;
		case ADD_CELL: a = s.sm ? c.src : c.acc / 32768.0; break;
		default:       a = 0.0; break;
	}

	double y = (s.sub ? prod - a : prod + a) * yss236_core::RSCALE[s.route];
	if (s.dr && s.z1)
		c.queue_r(s.dr, c.haspx ? c.px : xv);
	else if (s.dr)
		c.queue_r(s.dr, yss236_core::mode(y, s.clamp));
	y = c.acc = yss236_core::accmode(y, s.clamp);
	if (s.cmp)
		c.m_flag = (y > 0) - (y < 0);
	if (s.dm)
	{
		if (s.prev)
			c.queue_d(s.dm, c.psrc);
		else if (!(s.m2 == M2_CELL && s.add == ADD_OFF && s.dm == s.sm))   // s30: a step whose
			c.queue_d(s.dm, yss236_core::clip(y, yss236_core::DSAT));        // source is its own
	}                                                                        // cell does not write it
	if (s.wdata)
	{
		if (s.write)
			c.haswl = false;                       // s34: a writer carrying the flag writes its own data
		else
		{
			c.wlatch = yss236_core::clip(y, yss236_core::SAT);
			c.haswl = true;
		}
	}
	if (s.write)
	{
		double outv = yss236_core::clip(y, yss236_core::SAT);
		if (c.haswl)                              // s34: a writer takes the data a wdata step left
		{
			outv = c.wlatch;
			c.haswl = false;
		}
		c.ram[c.addr(s.st >> 2, 0, false)] = outv;
	}
	return y;
}

// The named shapes. Each says what the step computes, and they cover every step of every program.

// A class-0 step that computes nothing: it counts and moves the x latch, which is what the padding
// between a program's blocks is.
inline void idle(yss236_core& c)
{
	c.px = c.x;
	c.haspx = c.hasx;
	c.hasx = false;
	c.x = 0.0;
}

// p = k * r[sr], the adder off and nothing written: the first term of a sum, the value the rest add
// to. This is what a coefficient word is normally applied on.
inline void term(yss236_core& c, int st, int sr) { run(c, {st, sr}); }

// p = k * m[sm], the same term on a cell rather than a register.
inline void term_cell(yss236_core& c, int st, int sm) { run(c, {st, 0, sm, M2_CELL}); }

// p = k * r[sr] + p: the next term of the same sum. Every term after the first is one of these, which
// is what makes the running value the accumulator it is.
inline void sum(yss236_core& c, int st, int sr) { run(c, {st, sr, 0, M2_REG, false, 0, ADD_ACC}); }

// p = k * m[sm] + p, a term on a cell added to the running value.
inline void sum_cell(yss236_core& c, int st, int sm) { run(c, {st, 0, sm, M2_CELL, false, 0, ADD_ACC}); }

// r[dr] = (k * r[sr] + p) * route, saturated: a sum closed and scaled into a register. The route is
// what restores the scale designFilter()'s own (16 >> scale) took out of the coefficients.
inline void close(yss236_core& c, int st, int sr, int dr, route_t route = ROUTE_1)
{
	run(c, {st, sr, 0, M2_REG, false, 0, ADD_ACC, false, route, CLAMP_SAT, dr});
}

// r[dr] = (k * r[sr]) * route with the adder off: one register on its own, a gain and no sum.
inline void gain(yss236_core& c, int st, int sr, int dr, route_t route)
{
	run(c, {st, sr, 0, M2_REG, false, 0, ADD_OFF, false, route, CLAMP_SAT, dr});
}

// The z^-1 copy (w1[7]): r[dr] takes the value the step before latched, which is how a section holds
// x[-1] or y[-2] in a register of its own. The step still adds its own term to the running value, so
// `add` says whether this term continues a sum.
inline void copy_z(yss236_core& c, int st, int sr, int dr, add_t add = ADD_ACC, clamp_t clamp = CLAMP_WRAP)
{
	run(c, {st, sr, 0, M2_REG, false, 0, add, false, ROUTE_1, clamp, dr, true});
}

// A term whose result is also the compare flag: the conditional skips of a program test it.
inline double compare(yss236_core& c, int st, int sr, int sm, bool sub = false)
{
	return run(c, {st, sr, sm, M2_REG, true, 0, ADD_CELL, sub, ROUTE_1, CLAMP_WRAP, 0, false, 0, false, true});
}

// A class-1 step: the step's word loaded as a value rather than applied to one, in LOADK units, so
// r[dr] = 128 * k +- the adder's input, routed. This is what a rate counter, a threshold or a ROM
// word of its own is loaded by. `dm` publishes the loaded value into a cell, and `dwr` says the
// program word lets it (s29).
inline double load(yss236_core& c, int st, int sr, int sm, add_t add = ADD_OFF, bool sub = false,
                   route_t route = ROUTE_1, clamp_t clamp = CLAMP_WRAP, int dr = 0, int dm = 0,
                   bool dwr = false, bool cmp = false)
{
	latch_reg(c, sr);
	latch_cell(c, sm);
	double a;
	switch (add)
	{
		case ADD_ACC:  a = c.acc; break;
		case ADD_REG:  a = sr ? c.x : c.acc / 32768.0; break;
		case ADD_CELL: a = sm ? c.src : c.acc / 32768.0; break;
		default:       a = 0.0; break;
	}
	double v = yss236_core::LOADK * yss236_core::s16(c.coef[st]) + (sub ? -a : a);
	v *= yss236_core::RSCALE[route];
	if (dr)
		c.queue_r(dr, yss236_core::mode(v, clamp));
	c.acc = yss236_core::accmode(v, clamp);
	if (cmp)
		c.m_flag = (c.acc > 0) - (c.acc < 0);
	if (dm && (dwr || !dr))
		c.queue_d(dm, yss236_core::clip(c.acc, yss236_core::DSAT));
	return v;
}

// ---------------------------------------------------------------------------------------------
// The t file, the index queue and the control steps
// ---------------------------------------------------------------------------------------------

// A result goes into a t register as a 1.15 word (tw = 2): a t register is the multiplier's other
// input, and the word is the value over 128.
inline void t_put(yss236_core& c, int tn, double y)
{
	double const w = y * 256.0;
	c.queue_t(tn, (w < -32768.0 ? -32768.0 : 32767.0 < w ? 32767.0 : w) / 32768.0);
}

// A tw = 1 step writes its own word instead of its result, which is how a pole gain is loaded without
// the value that computed it.
inline void t_put_word(yss236_core& c, int st, int tn) { c.queue_t(tn, yss236_core::s16(c.coef[st])); }

// A tfrac step takes the fraction below one delay sample, so y * 256 is the delay and the word its
// floor. That is how a fractional delay position is set.
inline void t_put_fraction(yss236_core& c, int tn, double y)
{
	double const w = y * 256.0;
	c.queue_t(tn, w - std::floor(w));
}

// A delay address index from the running value (w2[2]), which takes effect three steps later, when a
// memory cycle reads it: y * 256 is the delay in samples and the word its floor.
inline void load_index(yss236_core& c, double y) { c.queue_i(std::floor(y * 256.0)); }

// The gate (w1[15] with w3 = 8): the steps up to `target` wait for the host's trigger, which is what
// core::trigger() writes their registers from. The step itself computes nothing.
inline void gate(yss236_core& c, int st, int target) { c.m_skip = (st & 0x100) | target; }

// A conditional skip: the steps up to `target` run only while the flag is positive (cskip 6), or only
// while it is negative (cskip 4). This is how a program branches on its own arithmetic.
inline void skip_positive(yss236_core& c, int st, int target)
{
	if (c.m_flag <= 0)
		c.m_skip = (st & 0x100) | target;
}

inline void skip_negative(yss236_core& c, int st, int target)
{
	if (c.m_flag >= 0)
		c.m_skip = (st & 0x100) | target;
}

// The jump (cskip 1): the steps up to `target` never run, so this is the branch's other arm.
inline void jump(yss236_core& c, int st, int target) { c.m_skip = (st & 0x100) | target; }

}  // namespace fs1r::native::step