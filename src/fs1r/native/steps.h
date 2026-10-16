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

#include <array>
#include <cmath>
#include <type_traits>
#include <utility>

#include "fs1r/chips/yss236_core.h"

// Every helper here is a few instructions inside a step that is itself unrolled, so they are forced
// inline: a call per helper would cost more than the arithmetic it wraps.
// A skipped block, which a step almost never meets, is kept out of line and cold instead, so the
// straight line the compiler schedules is the arithmetic alone.
#if defined(_MSC_VER)
#define FS1R_INLINE __forceinline
#define FS1R_FLATTEN [[msvc::flatten]]
#define FS1R_COLD __declspec(noinline)
#else
#define FS1R_INLINE inline __attribute__((always_inline))
#define FS1R_FLATTEN __attribute__((flatten))
#define FS1R_COLD __attribute__((noinline, cold))
#endif

namespace fs1r::native::step {

// ---------------------------------------------------------------------------------------------
// The pipeline, by step number
// ---------------------------------------------------------------------------------------------

// A write issued at a step lands LAT steps later through one of four queue slots, and step() picks the
// slot from the global step counter n. A pass is 512 steps, a multiple of four, and every step counts
// itself first, so at step st the counter is st + 1 modulo 4 on every pass: the slot is a function of
// the step alone. Picking it from st instead of n is what lets the compiler, which knows st, resolve
// every queue to a fixed field. The checker compares the queues after every pass, so a slot off by
// one shows up as a difference there.
constexpr int slot_of(int st) { return (st + 1 + yss236_core::LAT) & 3; }

FS1R_INLINE void queue_r(yss236_core& c, int st, int reg, double v) { c.m_rq[slot_of(st)] = {reg, v}; }
FS1R_INLINE void queue_d(yss236_core& c, int st, int cell, double v) { c.m_dq[slot_of(st)] = {cell, v}; }
FS1R_INLINE void queue_t(yss236_core& c, int st, int reg, double v) { c.m_tq[slot_of(st)] = {reg, v}; }

// What every step does first, as core::begin_step(), with the slot taken from st.
FS1R_INLINE void begin(yss236_core& c, int st)
{
	c.n++;
	int const k = (st + 1) & 3;
	yss236_core::pend& q = c.m_rq[k];
	if (q.reg > 0) c.r[q.reg] = q.v;
	q.reg = -1;
	yss236_core::pend& d = c.m_dq[k];
	if (d.reg >= 0) c.m[d.reg] = d.v;
	d.reg = -1;
	yss236_core::pend& t = c.m_tq[k];
	if (t.reg >= 0) c.t[t.reg] = t.v;
	t.reg = -1;
}

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
FS1R_INLINE void latch_reg(yss236_core& c, int sr)
{
	c.px = c.x;
	c.haspx = c.hasx;
	c.hasx = sr != 0;
	c.x = sr ? c.r[sr] : 0.0;
}

// src takes m[sm] and the step before's cell goes to psrc, which several steps then write back out as
// their own cell write: that chain of the previous operand is what gives the reverb's comb units a
// second input tap a step after the first.
FS1R_INLINE void latch_cell(yss236_core& c, int sm)
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
FS1R_INLINE void take_serial(yss236_core& c, int dm, int st, int bank) { queue_d(c, st, dm, c.in[bank][st >> 5]); }

// A cell takes an LFO value, the one its page carries.
FS1R_INLINE void take_lfo(yss236_core& c, int dm, int st) { queue_d(c, st, dm, c.lfo[st >> 5]); }

// A cell takes an EG level, which the hardware side keeps in the third input bank.
FS1R_INLINE void take_eg(yss236_core& c, int dm, int st) { queue_d(c, st, dm, c.in[3][st >> 5]); }

// A cell takes up a delay RAM read: the latest transfer of this pass made CAPMIN to 11 steps before
// this one, which is what read_delay() pushed and captured() walks back through. Five steps is the
// nearest the hardware allows (s40c), so a read and its capture are never adjacent.
FS1R_INLINE void take_capture(yss236_core& c, int dm, int st) { queue_d(c, st, dm, c.captured(c.m[dm], st)); }

// A cell takes up the delay RAM read and publishes it at the same time, which is how a tap reaches the
// registers a few steps later: one read, one history push and one capture.
FS1R_INLINE void tap_delay(yss236_core& c, int dm, int st, bool older = false)
{
	queue_d(c, st, dm, c.captured(c.m[dm], st));
	c.push_hist(st, c.ram[c.addr(st >> 2, older ? 1 : 0, false)]);
}

// A delay RAM read, pushed into the capture history so a capture further down can take it up. The
// slot is the step's own over four, which is where step() takes it from, and the offset is the sum of
// two of its fields: one word older (memop 4 and 6), and the delay index (w2[3]), which is what a tap
// at a moving position reads. Otherwise the offset is the c.slot[] entry the host last wrote, never a
// number here.
FS1R_INLINE void read_delay(yss236_core& c, int st, bool older = false, bool indexed = false)
{
	int const off = (older ? 1 : 0) + (indexed ? c.cur_idx() : 0);
	c.push_hist(st, c.ram[c.addr(st >> 2, off, false)]);
}

// A delay RAM write: the value takes the slot at the ring pointer, or one word indexed from the
// running value (w2[3]), which is what a tap at a moving position writes.
FS1R_INLINE void write_delay(yss236_core& c, int st, double v, bool indexed = false)
{
	c.ram[c.addr(st >> 2, indexed ? c.cur_idx() : 0, false)] = v;
}

// A class-0 step that writes the delay RAM (s34): it writes the data a wdata step latched, and does
// nothing at all without one, since a class-0 step has no result of its own to write.
FS1R_INLINE void write_delay_latched(yss236_core& c, int st)
{
	if (c.haswl)
	{
		c.ram[c.addr(st >> 2, 0, false)] = c.wlatch;
		c.haswl = false;
	}
}

// A cell goes to the serial output the step's page carries (w4[6]), so the base program picks it up.
FS1R_INLINE void serial_out(yss236_core& c, int st, int sm) { c.out[st >> 5] = c.m[sm]; }

// What a step does on the hardware side before its arithmetic, as one table entry: nothing, a delay RAM
// read at the step's own slot, or a cell taking a capture, an LFO, a serial input or an EG level, or a
// cell going out to the serial output. A program's tables say this per step rather than per call, so
// a row reads as "this band's b1 step also takes the page's LFO into m[11]".
enum decor_t { DEC_NONE, DEC_READ, DEC_CAPTURE, DEC_LFO, DEC_SERIAL_L, DEC_SERIAL_R, DEC_EG, DEC_OUT };

struct Decor { decor_t kind = DEC_NONE; int cell = 0; };

FS1R_INLINE void decor(yss236_core& c, int st, Decor const& d)
{
	switch (d.kind)
	{
		case DEC_READ:      read_delay(c, st); break;
		case DEC_CAPTURE:   take_capture(c, d.cell, st); break;
		case DEC_LFO:       take_lfo(c, d.cell, st); break;
		case DEC_SERIAL_L:  take_serial(c, d.cell, st, 0); break;
		case DEC_SERIAL_R:  take_serial(c, d.cell, st, 1); break;
		case DEC_EG:        take_eg(c, d.cell, st); break;
		case DEC_OUT:       serial_out(c, st, d.cell); break;
		case DEC_NONE:      break;
	}
}

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
	bool dwr = false;         // load() only: write dm even though dr is set (w1[12:8], s29)
};

// The adder's input (w3[7:6]), read after the step's two latches moved: the running value, or the
// register or the cell the step just latched, or with neither of those the running value / 32768.
FS1R_INLINE double adder(yss236_core const& c, alu const& s)
{
	switch (s.add)
	{
		case ADD_ACC:  return c.acc;
		case ADD_REG:  return s.sr ? c.x : c.acc / 32768.0;
		case ADD_CELL: return s.sm ? c.src : c.acc / 32768.0;
		default:       return 0.0;
	}
}

// One class-2 or class-3 step. The order is step()'s: the multiplier's word and its two inputs, then
// the adder, then the route, then the clamp into the register and the running value, then the cell
// the result goes to and the flag. Doubles do not associate, so a program cannot fold two of these
// steps into one expression.
FS1R_INLINE double run(yss236_core& c, alu const& s)
{
	double const xv = s.sr ? c.r[s.sr] : 0.0;   // read before the latch moves it to px
	latch_reg(c, s.sr);
	latch_cell(c, s.sm);

	double const m2 = s.m2 == M2_CELL ? c.src : xv;
	double const g = s.unity ? 1.0 : s.treg ? c.t[s.treg] : yss236_core::s16(c.coef[s.st]);
	double const prod = s.unity ? m2 : g * m2;

	double const a = adder(c, s);

	double y = (s.sub ? prod - a : prod + a) * yss236_core::RSCALE[s.route];
	if (s.dr && s.z1)
		queue_r(c, s.st, s.dr, c.haspx ? c.px : xv);
	else if (s.dr)
		queue_r(c, s.st, s.dr, yss236_core::mode(y, s.clamp));
	y = c.acc = yss236_core::accmode(y, s.clamp);
	if (s.cmp)
		c.m_flag = (y > 0) - (y < 0);
	if (s.dm)
	{
		if (s.prev)
			queue_d(c, s.st, s.dm, c.psrc);
		else if (!(s.m2 == M2_CELL && s.add == ADD_OFF && s.dm == s.sm))   // s30: a step whose
			queue_d(c, s.st, s.dm, yss236_core::clip(y, yss236_core::DSAT));        // source is its own
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
FS1R_INLINE void idle(yss236_core& c)
{
	c.px = c.x;
	c.haspx = c.hasx;
	c.hasx = false;
	c.x = 0.0;
}

// p = k * r[sr], the adder off and nothing written: the first term of a sum, the value the rest add
// to. This is what a coefficient word is normally applied on.
FS1R_INLINE void term(yss236_core& c, int st, int sr) { run(c, {.st = st, .sr = sr}); }

// p = k * m[sm], the same term on a cell rather than a register.
FS1R_INLINE void term_cell(yss236_core& c, int st, int sm) { run(c, {.st = st, .sm = sm, .m2 = M2_CELL}); }

// p = k * r[sr] + p: the next term of the same sum. Every term after the first is one of these, which
// is what makes the running value the accumulator it is.
FS1R_INLINE void sum(yss236_core& c, int st, int sr) { run(c, {.st = st, .sr = sr, .add = ADD_ACC}); }

// p = k * m[sm] + p, a term on a cell added to the running value.
FS1R_INLINE void sum_cell(yss236_core& c, int st, int sm) { run(c, {.st = st, .sm = sm, .m2 = M2_CELL, .add = ADD_ACC}); }

// r[dr] = (k * r[sr] + p) * route, saturated: a sum closed and scaled into a register. The route is
// what restores the scale designFilter()'s own (16 >> scale) took out of the coefficients.
FS1R_INLINE void close(yss236_core& c, int st, int sr, int dr, route_t route = ROUTE_1)
{
	run(c, {.st = st, .sr = sr, .add = ADD_ACC, .route = route, .clamp = CLAMP_SAT, .dr = dr});
}

// r[dr] = (k * r[sr]) * route with the adder off: one register on its own, a gain and no sum.
FS1R_INLINE void gain(yss236_core& c, int st, int sr, int dr, route_t route)
{
	run(c, {.st = st, .sr = sr, .route = route, .clamp = CLAMP_SAT, .dr = dr});
}

// The z^-1 copy (w1[7]): r[dr] takes the value the step before latched, which is how a section holds
// x[-1] or y[-2] in a register of its own. The step still adds its own term to the running value, so
// `add` says whether this term continues a sum.
FS1R_INLINE void copy_z(yss236_core& c, int st, int sr, int dr, add_t add = ADD_ACC, clamp_t clamp = CLAMP_WRAP)
{
	run(c, {.st = st, .sr = sr, .add = add, .clamp = clamp, .dr = dr, .z1 = true});
}

// A term whose result is also the compare flag: the conditional skips of a program test it. The step
// is class 3, so its word is ignored and the term is the register it read; the adder reads the cell,
// negated when `sub`. The clamp is per step like every other: a compare whose result saturates says so.
FS1R_INLINE double compare(yss236_core& c, int st, int sr, int sm, bool sub = false, clamp_t clamp = CLAMP_WRAP)
{
	return run(c, {.st = st, .sr = sr, .sm = sm, .unity = true, .add = ADD_CELL, .sub = sub, .clamp = clamp, .cmp = true});
}

// A class-1 step: the step's word loaded as a value rather than applied to one, in LOADK units, so
// r[dr] = 128 * k +- the adder's input, routed. This is what a rate counter, a threshold or a ROM
// word of its own is loaded by. `dm` publishes the loaded value into a cell when the step has no
// register to write, or when `dwr` says the program word lets it anyway (s29). The multiplier fields
// (m2, unity, treg) and the z^-1 copy mean nothing here.
FS1R_INLINE double load(yss236_core& c, alu const& s)
{
	latch_reg(c, s.sr);
	latch_cell(c, s.sm);
	double const a = adder(c, s);
	double v = yss236_core::LOADK * yss236_core::s16(c.coef[s.st]) + (s.sub ? -a : a);
	v *= yss236_core::RSCALE[s.route];
	if (s.dr)
		queue_r(c, s.st, s.dr, yss236_core::mode(v, s.clamp));
	c.acc = yss236_core::accmode(v, s.clamp);
	if (s.cmp)
		c.m_flag = (c.acc > 0) - (c.acc < 0);
	if (s.dm && (s.dwr || !s.dr))
		queue_d(c, s.st, s.dm, yss236_core::clip(c.acc, yss236_core::DSAT));
	return v;
}

// ---------------------------------------------------------------------------------------------
// The t file, the index queue and the control steps
// ---------------------------------------------------------------------------------------------

// A result goes into a t register as a 1.15 word (tw = 2): a t register is the multiplier's other
// input, and the word is the value over 128.
FS1R_INLINE void t_put(yss236_core& c, int st, int tn, double y)
{
	double const w = y * 256.0;
	queue_t(c, st, tn, (w < -32768.0 ? -32768.0 : 32767.0 < w ? 32767.0 : w) / 32768.0);
}

// A tw = 1 step writes its own word instead of its result, which is how a pole gain is loaded without
// the value that computed it.
FS1R_INLINE void t_put_word(yss236_core& c, int st, int tn) { queue_t(c, st, tn, yss236_core::s16(c.coef[st])); }

// A tfrac step takes the fraction below one delay sample, so y * 256 is the delay and the word its
// floor. That is how a fractional delay position is set.
FS1R_INLINE void t_put_fraction(yss236_core& c, int st, int tn, double y)
{
	double const w = y * 256.0;
	queue_t(c, st, tn, w - std::floor(w));
}

// A delay address index from the running value (w2[2]), which takes effect three steps later, when a
// memory cycle reads it: y * 256 is the delay in samples and the word its floor.
FS1R_INLINE void load_index(yss236_core& c, double y) { c.queue_i(std::floor(y * 256.0)); }

// The gate (w1[15] with w3 = 8): the steps up to `target` wait for the host's trigger, which is what
// core::trigger() writes their registers from. The step itself computes nothing.
FS1R_INLINE void gate(yss236_core& c, int st, int target) { c.m_skip = (st & 0x100) | target; }

// A conditional skip: the steps up to `target` run only while the flag is positive (cskip 6), or only
// while it is negative (cskip 4). This is how a program branches on its own arithmetic.
FS1R_INLINE void skip_positive(yss236_core& c, int st, int target)
{
	if (c.m_flag <= 0)
		c.m_skip = (st & 0x100) | target;
}

FS1R_INLINE void skip_negative(yss236_core& c, int st, int target)
{
	if (c.m_flag >= 0)
		c.m_skip = (st & 0x100) | target;
}

// The jump (cskip 1): the steps up to `target` never run, so this is the branch's other arm.
FS1R_INLINE void jump(yss236_core& c, int st, int target) { c.m_skip = (st & 0x100) | target; }

// ---------------------------------------------------------------------------------------------
// Running a block of steps
// ---------------------------------------------------------------------------------------------

// Steps From to To - 1, each one compiled for its own number. `body` is a lambda taking `auto st`, and
// st is a std::integral_constant, so every table lookup, switch and comparison on it in the body folds
// away at compile time: what is left per step is the arithmetic the step's row names, with no decode,
// no branch on the row and no loop. That is what the JIT did at run time, done by the compiler instead.
// Each step opens as step() does, by counting itself and honouring the m_skip a gate or a skip set.
//
// The block is flattened: every helper a program calls from its body is inlined into it, so a program
// keeps its own small named functions (a band's step, a tap pair) taking `int st` and they fold too.
// The rest of a block once a gate or a conditional skip has fired in or before it: the interpreter,
// which is the definition of a skipped step. Kept out of line and cold, so the straight-line path the
// compiler sees has no join after any step, and a value a step queues reaches the step it lands on
// without a trip through memory.
FS1R_COLD inline void interpret(yss236_core& c, int from, int to)
{
	for (int st = from; st < to; st++) c.step(st);
}

template <int St, int To, typename F>
FS1R_INLINE bool one_step(yss236_core& c, F& body)
{
	if (St < c.m_skip)
	{
		interpret(c, St, To);
		return false;
	}
	begin(c, St);
	body(c, std::integral_constant<int, St>{});
	return true;
}

template <int From, int To, typename F, int... I>
FS1R_FLATTEN void for_steps(yss236_core& c, F&& body, std::integer_sequence<int, I...>)
{
	(one_step<From + I, To>(c, body) && ...);
}

template <int From, int To, typename F>
FS1R_INLINE void for_steps(yss236_core& c, F&& body)
{
	for_steps<From, To>(c, body, std::make_integer_sequence<int, To - From>{});
}

// ---------------------------------------------------------------------------------------------
// Finding a step's row, without looking for it
// ---------------------------------------------------------------------------------------------

// The tables below are the program's structure and they are what a reader wants to see, but a program
// runs one step per microsecond and every table is short, so scanning one per step costs more than the
// step itself did. index_by_step builds the map the interpreter gets for free from its switch: a step
// indexes straight to its row.
//
// Both need a table whose first field is the step the row is at, named `st`.
//
// The result is a constexpr array of row indices, -1 for a step no row owns, so it costs nothing at run
// time and the compiler can see every row it needs.
//
// Where two rows claim the same step the later one wins, so a table whose rows overlap must not use
// these: ins_m03's taps are five steps apart where the reads are four, and a span over them would hand a
// capture the next read's cell. That one builds its capture index by hand instead.

// The row of `rows` at each step, -1 where no row is. One row per step.
template <typename T, int N>
constexpr std::array<int16_t, yss236_core::STEPS> index_by_step(T const (&rows)[N])
{
    std::array<int16_t, yss236_core::STEPS> ix{};
    for (int16_t& i : ix) i = -1;
    for (int k = 0; k < N; k++)
        ix[rows[k].st] = int16_t(k);
    return ix;
}

// The same, where each row covers `nsteps` consecutive steps from its own `st`. All four steps of a
// unit index to the same row, and a step's place within its unit is `st - row.st`.
template <typename T, int N>
constexpr std::array<int16_t, yss236_core::STEPS> index_by_span(T const (&rows)[N])
{
    std::array<int16_t, yss236_core::STEPS> ix{};
    for (int16_t& i : ix) i = -1;
    for (int k = 0; k < N; k++)
        for (int i = 0; i < rows[k].nsteps; i++)
            ix[rows[k].st + i] = int16_t(k);
    return ix;
}

}  // namespace fs1r::native::step