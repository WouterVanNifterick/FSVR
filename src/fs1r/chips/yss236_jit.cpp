// license:BSD-3-Clause
// copyright-holders:James Hansen

// The YSS236 program compiled to x86-64: one straight-line function per program, run once per pass in place
// of step(). Each step becomes only the code its decoded fields select. The interpreter in yss236_core.h
// stays the reference: tools/vop3_jit_check.cpp runs the two side by side over every effect and filter type
// and compares their whole state after every pass.
//
// What the compiled pass keeps from step():
//   - The state lives where the interpreter keeps it, in the core, so either can run any pass.
//   - A write issued at step st lands at the start of st + 3 through the interpreter's queue slot for it,
//     m_rq[st & 3] and the like (n is a multiple of 4 when a pass starts). Which register it lands in is
//     known when compiling, unless the issuing step can be skipped or the write lands in the next pass: then
//     the slot carries the register, as in the interpreter.
//   - A gate or a conditional skip that does not run is a forward branch to a path that lands the writes
//     due in the steps it skips, then joins the step it skips to.
//   - The arithmetic is the interpreter's, operation for operation, in double precision with no FMA. Its
//     clamps compare in the order minsd and maxsd do, so NaN and signed zero agree as well. A scaling by a
//     power of two is a multiply where the interpreter divides; both round the same exact value.
//   - Captures, the delay index queue and a mode-0 result that wraps call the interpreter's own code.
// Built with the hosts' floating-point flags, like the interpreter it stands in for. AsmJit needs C++20.
#define _CRT_SECURE_NO_WARNINGS
#include "fs1r/chips/yss236_core.h"
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <mutex>

#if defined(_M_X64) || defined(__x86_64__)
#include <asmjit/x86.h>
#define YSS236_JIT 1
#endif

struct yss236_core::jit_code { void (*fn)(yss236_core *, double *); };

// The interpreter's own code, for what the compiled pass hands back to it. n is set first where it is read.
struct yss236_jit
{
	using C = yss236_core;
	static void at(C *c, int st) { c->n = c->m_n0 + st + 1; }
	static int index(C *c, int st) { at(c, st); return c->cur_idx(); }
	static void queue_index(C *c, int st) { at(c, st); c->queue_i(std::floor(c->m_tmp[0] * 256.0)); }
	static double capture(C *c, int st) { return c->captured(c->m[c->ops[st].dm], st); }
	static double wrap(C *c, int) { return C::pymod(c->m_tmp[0], 2 * C::RANGE) - C::RANGE; }
	static double wrap_acc(C *c, int) { return C::pymod(c->m_tmp[0], 2 * C::ACC) - C::ACC; }
};

#if YSS236_JIT
namespace {

using namespace asmjit;
using namespace asmjit::x86;
using C = yss236_core;

#ifdef _WIN32
const Gp ARG0 = rcx, ARG1 = edx;
#else
const Gp ARG0 = rdi, ARG1 = esi;
#endif

// The constants the code reads, through r13
struct alignas(16) consts
{
	uint64_t abs[2] = { ~0ull >> 1, ~0ull >> 1 };
	uint64_t sign[2] = { 1ull << 63, 1ull << 63 };
	double zero = 0.0, k256 = 256.0, inv256 = 1.0 / 256, inv32768 = 1.0 / 32768;
	double range = C::RANGE, range2 = 2 * C::RANGE, acc = C::ACC, acc2 = 2 * C::ACC;
	double hi = C::RANGE - 1.0 / 256, lo = -C::RANGE;
	double sat = C::SAT, nsat = -C::SAT, dsat = C::DSAT, ndsat = -C::DSAT;
	double thi = 32767.0, tlo = -32768.0;
	double rscale[4] = { C::RSCALE[0], C::RSCALE[1], C::RSCALE[2], C::RSCALE[3] };
};
const consts K;

std::mutex lock;
JitRuntime &runtime()
{
	static JitRuntime *rt = new JitRuntime;   // never destroyed: a core may outlive static destruction
	return *rt;
}

struct failure : ErrorHandler
{
	bool failed = false;
	void handle_error(Error, const char *, BaseEmitter *) override { failed = true; }
};

class gen
{
	enum { R, D, T };                  // the three write queues

	Assembler &a;
	C &c;
	Label step_at[C::STEPS];
	std::vector<std::function<void()>> stubs;   // out-of-line code, emitted after the pass's return
	int8_t writes[C::STEPS][3];        // the register, cell or t register a step writes when it runs, or -1
	bool skippable[C::STEPS] = {};     // a control step before it may skip it

	int32_t off(const void *p) const { return int32_t((const char *)p - (const char *)&c); }
	Mem q(const void *p) const { return qword_ptr(rbx, off(p)); }
	Mem k(const double &v) const { return qword_ptr(r13, int32_t((const char *)&v - (const char *)&K)); }
	C::pend *queue(int w, int i) const { return &(w == R ? c.m_rq : w == D ? c.m_dq : c.m_tq)[i & 3]; }
	double *file(int w) const { return w == R ? c.r : w == D ? c.m : c.t; }

	void call(void *fn, int st)
	{
		a.mov(ARG0, rbx);
		a.mov(ARG1, st);
		a.mov(rax, Imm(uint64_t(fn)));
		a.call(rax);
	}

	// What each step writes and which steps may be skipped
	void analyse()
	{
		for (int st = 0; st < C::STEPS; st++)
		{
			C::op const &o = c.ops[st];
			int8_t *w = writes[st];
			w[R] = w[D] = w[T] = -1;
			if (o.ctlop)
			{
				for (int s = st + 1; s < ((st & 0x100) | o.target); s++)
					skippable[s] = true;
				continue;
			}
			int const cls = o.mmode;
			if (o.dm && o.msrc <= C::M_CAPTURE2)
				w[D] = o.dm;
			if (cls == 0)
			{
				if (o.alu && o.dr && !o.rcopy)
					w[R] = o.dr;
				continue;
			}
			if (o.dr)
				w[R] = o.dr;
			if (o.tw & 3)
				w[T] = o.tn;
			if (o.dm && cls == 1 && o.msrc == C::M_RESULT && (o.dwr || !o.dr))
				w[D] = o.dm;
			if (o.dm && cls >= 2 && (o.msrc == C::M_PREVIOUS ||
					(o.msrc == C::M_RESULT && (!(o.m2m && o.asel == C::A_NONE) || o.dm != o.sm))))
				w[D] = o.dm;
		}
	}

	// The writes issued three steps ago land
	void land(int st)
	{
		for (int w = R; w <= T; w++)
		{
			C::pend *slot = queue(w, st + 1);
			int const from = st - 3;
			if (from >= 0 && writes[from][w] < 0)
				continue;                             // nothing was issued
			if (from >= 0 && !skippable[from])
			{
				a.movsd(xmm0, q(&slot->v));           // issued for sure, to a known register
				a.movsd(q(file(w) + writes[from][w]), xmm0);
				continue;
			}
			Label none = a.new_label();               // the slot says
			a.movsxd(rax, dword_ptr(rbx, off(&slot->reg)));
			a.test(eax, eax);
			if (w == R)
				a.jle(none);
			else
				a.js(none);
			a.movsd(xmm0, q(&slot->v));
			a.movsd(qword_ptr(rbx, rax, 3, off(file(w))), xmm0);
			a.bind(none);
			a.mov(dword_ptr(rbx, off(&slot->reg)), -1);
		}
	}

	// Issue a write from v
	void put(int w, int st, Vec v)
	{
		C::pend *slot = queue(w, st);
		a.movsd(q(&slot->v), v);
		if (skippable[st] || st + C::LAT >= C::STEPS)
			a.mov(dword_ptr(rbx, off(&slot->reg)), writes[st][w]);
	}

	// The delay RAM address of the step's slot into ecx; with the index, cur_idx() is called first
	void address(int st, int older, bool index)
	{
		if (index)
			call((void *)&yss236_jit::index, st);
		int const s = st >> 2;
		a.movzx(ecx, word_ptr(rbx, off(&c.slot[s][0])));
		a.and_(ecx, 0x1ff);
		a.shl(ecx, 9);
		a.movzx(edx, word_ptr(rbx, off(&c.slot[s][1])));
		a.and_(edx, 0x1ff);
		a.or_(ecx, edx);
		a.add(ecx, dword_ptr(rbx, off(&c.ptr)));
		if (older)
			a.add(ecx, older);
		if (index)
			a.add(ecx, eax);
		a.and_(ecx, C::RAM_WORDS - 1);
	}

	// mode(), or accmode() with acc set, of src into dst
	void mode(Vec dst, Vec src, int mo, bool acc, int st)
	{
		if (dst.id() != src.id())
			a.movapd(dst, src);
		if (mo)
		{
			a.minsd(dst, k(K.hi));
			a.maxsd(dst, k(K.lo));
			if (mo == 2)
				a.maxsd(dst, k(K.zero));
			else if (mo == 3)
				a.andpd(dst, ptr_128(r13, offsetof(consts, abs)));
			return;
		}
		Label slow = a.new_label(), back = a.new_label();
		a.addsd(dst, k(acc ? K.acc : K.range));
		a.comisd(dst, k(K.zero));
		a.jb(slow);
		a.comisd(dst, k(acc ? K.acc2 : K.range2));
		a.jae(slow);
		a.subsd(dst, k(acc ? K.acc : K.range));
		a.bind(back);
		stubs.push_back([this, dst, acc, st, slow, back] {
			a.bind(slow);
			for (int i = 0; i < 6; i++)
				if (i != int(dst.id()))
					a.movsd(q(&c.m_tmp[2 + i]), xmm(i));
			a.movsd(q(&c.m_tmp[0]), dst);
			call(acc ? (void *)&yss236_jit::wrap_acc : (void *)&yss236_jit::wrap, st);
			if (dst.id() != 0)
				a.movapd(dst, xmm0);
			for (int i = 0; i < 6; i++)
				if (i != int(dst.id()))
					a.movsd(xmm(i), q(&c.m_tmp[2 + i]));
			a.jmp(back);
		});
	}

	void clip(Vec v, double const &c, double const &nc)
	{
		a.minsd(v, k(c));
		a.maxsd(v, k(nc));
	}

	// m_flag from y's sign
	void flag(Vec y)
	{
		a.xorpd(xmm5, xmm5);
		a.comisd(y, xmm5);
		a.seta(al);
		a.comisd(xmm5, y);
		a.seta(cl);
		a.movzx(eax, al);
		a.movzx(ecx, cl);
		a.sub(eax, ecx);
		a.mov(dword_ptr(rbx, off(&c.m_flag)), eax);
	}

	// The adder input into dst. x is in xmm1 and the m operand in xmm2.
	void adder(Vec dst, C::op const &o)
	{
		switch (o.asel)
		{
			case C::A_P: a.movsd(dst, q(&c.acc)); break;
			case C::A_R:
			case C::A_M:
				if (o.asel == C::A_R ? o.sr : o.sm)
					a.movapd(dst, o.asel == C::A_R ? xmm1 : xmm2);
				else
				{
					a.movsd(dst, q(&c.acc));
					a.mulsd(dst, k(K.inv32768));
				}
				break;
			default: a.xorpd(dst, dst); break;
		}
	}

	// s16(coef[st]) times scale into dst
	void coef(Vec dst, int st, double const &scale)
	{
		a.movsx(eax, word_ptr(rbx, off(&c.coef[st])));
		a.xorpd(dst, dst);
		a.cvtsi2sd(dst, eax);
		a.mulsd(dst, k(scale));
	}

	// px = x; haspx = hasx; x = xv; hasx = sr != 0, with xv into xmm1
	void fetch_x(C::op const &o)
	{
		if (o.sr)
			a.movsd(xmm1, q(&c.r[o.sr]));
		else
			a.xorpd(xmm1, xmm1);
		a.movsd(xmm0, q(&c.x));
		a.movsd(q(&c.px), xmm0);
		a.movzx(eax, byte_ptr(rbx, off(&c.hasx)));
		a.mov(byte_ptr(rbx, off(&c.haspx)), al);
		a.movsd(q(&c.x), xmm1);
		a.mov(byte_ptr(rbx, off(&c.hasx)), o.sr != 0);
	}

	// psrc = src; src = m[sm], with the new src into xmm2
	void fetch_m(C::op const &o)
	{
		a.movsd(xmm0, q(&c.src));
		a.movsd(q(&c.psrc), xmm0);
		a.movsd(xmm2, q(&c.m[o.sm]));
		a.movsd(q(&c.src), xmm2);
	}

	void t_write(int st, Vec y)
	{
		C::op const &o = c.ops[st];
		if (o.tw & 2)
		{
			a.movapd(xmm0, y);
			a.mulsd(xmm0, k(K.k256));
			if (o.tfrac)
			{
				a.roundsd(xmm5, xmm0, 9);             // floor
				a.subsd(xmm0, xmm5);
			}
			else
			{
				a.movsd(xmm5, k(K.thi));
				a.minsd(xmm5, xmm0);
				a.movsd(xmm0, k(K.tlo));
				a.maxsd(xmm0, xmm5);
				a.mulsd(xmm0, k(K.inv32768));
			}
			put(T, st, xmm0);
		}
		else if (o.tw & 1)
		{
			coef(xmm0, st, K.inv32768);
			put(T, st, xmm0);
		}
	}

	void control(int st)
	{
		C::op const &o = c.ops[st];
		int const to = (st & 0x100) | o.target;
		Label run = a.new_label();
		if (o.cskip == 6 || o.cskip == 4)
		{
			a.cmp(dword_ptr(rbx, off(&c.m_flag)), 0);
			if (o.cskip == 6)
				a.jg(run);
			else
				a.jl(run);
		}
		a.mov(dword_ptr(rbx, off(&c.m_skip)), to);
		if (to > st + 1)
		{
			Label skip = a.new_label();
			a.jmp(skip);
			stubs.push_back([this, st, to, skip] {
				a.bind(skip);
				for (int s = st + 1; s < to && s <= st + C::LAT; s++)
					land(s);                          // later ones were issued inside the skip
				a.jmp(step_at[to]);
			});
		}
		a.bind(run);
	}

	void step(int st)
	{
		a.bind(step_at[st]);
		land(st);
		C::op const &o = c.ops[st];
		if (o.ctlop)
		{
			control(st);
			return;
		}
		int const cls = o.mmode;
		int const page = (st >> 5) & 15;
		bool const xfer = (st & 3) == 3 || (st & 3) == 1;
		bool const write = xfer && o.memop == 1;

		if (xfer && (o.memop == 2 || o.memop == 3 || o.memop == 4 || o.memop == 6))
		{
			address(st, (o.memop & 4) ? 1 : 0, o.idxa);
			a.movsd(xmm0, qword_ptr(r12, rcx, 3));
			Label full = a.new_label();               // push_hist
			a.mov(eax, dword_ptr(rbx, off(&c.m_nhist)));
			a.cmp(eax, C::STEPS);
			a.jge(full);
			a.mov(edx, eax);
			a.shl(edx, 4);
			static_assert(sizeof(C::xfer) == 16, "m_hist entries are 16 bytes");
			a.mov(dword_ptr(rbx, rdx, 0, off(&c.m_hist[0].st)), st);
			a.movsd(qword_ptr(rbx, rdx, 0, off(&c.m_hist[0].v)), xmm0);
			a.inc(eax);
			a.mov(dword_ptr(rbx, off(&c.m_nhist)), eax);
			a.bind(full);
		}

		if (o.dm)
			switch (o.msrc)
			{
				case C::M_SERIAL: case C::M_SERIAL2: case C::M_EG:
					a.movsd(xmm0, q(&c.in[o.msrc][page]));
					put(D, st, xmm0);
					break;
				case C::M_LFO:
					a.movsd(xmm0, q(&c.lfo[st >> 5]));
					put(D, st, xmm0);
					break;
				case C::M_CAPTURE: case C::M_CAPTURE2:
					call((void *)&yss236_jit::capture, st);
					put(D, st, xmm0);
					break;
				default:
					break;
			}

		if (o.sout)
		{
			a.movsd(xmm0, q(&c.m[o.sm]));
			a.movsd(q(&c.out[page]), xmm0);
		}

		if (cls < 2)
			fetch_x(o);

		if (cls == 1)
		{
			fetch_m(o);
			adder(xmm4, o);
			coef(xmm3, st, K.inv256);                 // LOADK * s16(coef)
			if (o.sub)
				a.xorpd(xmm4, ptr_128(r13, offsetof(consts, sign)));
			a.addsd(xmm3, xmm4);
			if (o.shift)
				a.mulsd(xmm3, k(K.rscale[o.shift]));
			if (o.dr)
			{
				mode(xmm0, xmm3, o.clamp, false, st);
				put(R, st, xmm0);
			}
			mode(xmm3, xmm3, o.clamp, true, st);
			a.movsd(q(&c.acc), xmm3);
			if (o.cmp)
				flag(xmm3);
			if (o.msrc == C::M_RESULT && o.dm && (o.dwr || !o.dr))
			{
				a.movapd(xmm0, xmm3);
				clip(xmm0, K.dsat, K.ndsat);
				put(D, st, xmm0);
			}
			t_write(st, xmm3);
			return;
		}

		if (cls == 0)
		{
			if (write)
			{
				Label none = a.new_label();
				a.cmp(byte_ptr(rbx, off(&c.haswl)), 0);
				a.je(none);
				address(st, 0, o.idxa);
				a.movsd(xmm0, q(&c.wlatch));
				a.movsd(qword_ptr(r12, rcx, 3), xmm0);
				a.mov(byte_ptr(rbx, off(&c.haswl)), 0);
				a.bind(none);
			}
			if (o.alu)
			{
				fetch_m(o);
				adder(xmm3, o);
				if (o.sub)
					a.xorpd(xmm3, ptr_128(r13, offsetof(consts, sign)));
				if (o.shift)
					a.mulsd(xmm3, k(K.rscale[o.shift]));
				if (o.dr && !o.rcopy)
				{
					mode(xmm0, xmm3, o.clamp, false, st);
					put(R, st, xmm0);
				}
				mode(xmm3, xmm3, o.clamp, true, st);
				a.movsd(q(&c.acc), xmm3);
				if (o.cmp)
					flag(xmm3);
			}
			return;
		}

		// classes 2 and 3
		fetch_x(o);
		fetch_m(o);
		Vec const m2 = o.m2m ? xmm2 : xmm1;
		if (cls == 3)
			a.movapd(xmm3, m2);
		else
		{
			if (o.m1 >= 2)
				a.movsd(xmm3, q(&c.t[((o.m1 & 1) << 3) | o.t]));
			else
				coef(xmm3, st, K.inv32768);
			a.mulsd(xmm3, m2);
		}
		adder(xmm4, o);
		if (o.sub)
			a.subsd(xmm3, xmm4);
		else
			a.addsd(xmm3, xmm4);
		if (o.shift)
			a.mulsd(xmm3, k(K.rscale[o.shift]));
		if (o.dr && o.rcopy)
		{
			Label none = a.new_label();               // the previous step's x, if it had one
			a.movapd(xmm0, xmm1);
			a.cmp(byte_ptr(rbx, off(&c.haspx)), 0);
			a.je(none);
			a.movsd(xmm0, q(&c.px));
			a.bind(none);
			put(R, st, xmm0);
		}
		else if (o.dr)
		{
			mode(xmm0, xmm3, o.clamp, false, st);
			put(R, st, xmm0);
		}
		mode(xmm3, xmm3, o.clamp, true, st);
		a.movsd(q(&c.acc), xmm3);
		if (o.cmp)
			flag(xmm3);
		if (o.dm && o.msrc == C::M_PREVIOUS)
		{
			a.movsd(xmm0, q(&c.psrc));
			put(D, st, xmm0);
		}
		else if (o.dm && o.msrc == C::M_RESULT && (!(o.m2m && o.asel == C::A_NONE) || o.dm != o.sm))
		{
			a.movapd(xmm0, xmm3);
			clip(xmm0, K.dsat, K.ndsat);
			put(D, st, xmm0);
		}
		t_write(st, xmm3);
		if (o.idxw)
		{
			a.movsd(q(&c.m_tmp[0]), xmm3);
			call((void *)&yss236_jit::queue_index, st);
		}
		if (o.wdata)
		{
			if (write)
				a.mov(byte_ptr(rbx, off(&c.haswl)), 0);
			else
			{
				a.movsd(xmm0, q(&c.acc));
				clip(xmm0, K.sat, K.nsat);
				a.movsd(q(&c.wlatch), xmm0);
				a.mov(byte_ptr(rbx, off(&c.haswl)), 1);
			}
		}
		if (write)
		{
			address(st, 0, o.idxa);
			a.movsd(xmm0, q(&c.acc));
			clip(xmm0, K.sat, K.nsat);
			if (!o.wdata)
			{
				Label none = a.new_label();           // a latched word goes instead
				a.cmp(byte_ptr(rbx, off(&c.haswl)), 0);
				a.je(none);
				a.movsd(xmm0, q(&c.wlatch));
				a.mov(byte_ptr(rbx, off(&c.haswl)), 0);
				a.bind(none);
			}
			a.movsd(qword_ptr(r12, rcx, 3), xmm0);
		}
	}

public:
	gen(Assembler &a, C &c) : a(a), c(c) {}

	void pass()
	{
		analyse();
		for (auto &l : step_at)
			l = a.new_label();
		for (int st = 0; st < C::STEPS; st++)
			step(st);
	}

	void out_of_line()
	{
		for (size_t i = 0; i < stubs.size(); i++)   // a skip path's landings can add stubs of their own
			stubs[i]();
	}
};

C::jit_code *compile(C &c)
{
	static bool const sse41 = CpuInfo::host().features().x86().has_sse4_1();   // roundsd
	if (!sse41)
		return nullptr;
	std::lock_guard<std::mutex> guard(lock);
	JitRuntime &rt = runtime();
	CodeHolder code;
	failure fail;
	if (code.init(rt.environment(), rt.cpu_features()) != Error::kOk)
		return nullptr;
	code.set_error_handler(&fail);
	Assembler a(&code);

	FuncDetail func;
	func.init(FuncSignature::build<void, C *, double *>(), rt.environment());
	FuncFrame frame;
	frame.init(func);
	frame.add_dirty_regs(rax, rcx, rdx, rbx, r12, r13, ARG0.r64(), ARG1.r64());
	frame.add_dirty_regs(xmm0, xmm1, xmm2, xmm3, xmm4, xmm5);
	frame.set_func_calls();
	frame.update_call_stack_size(32);         // the callee's home area on Windows
	FuncArgsAssignment args(&func);
	args.assign_all(rbx, r12);
	args.update_func_frame(frame);
	frame.finalize();

	a.emit_prolog(frame);
	a.emit_args_assignment(frame, args);
	a.mov(r13, Imm(uint64_t(&K)));
	gen g(a, c);
	g.pass();
	a.emit_epilog(frame);
	g.out_of_line();
	if (fail.failed)
		return nullptr;

	void (*fn)(C *, double *);
	if (rt.add(&fn, &code) != Error::kOk)
		return nullptr;
	return new C::jit_code{ fn };
}

}  // namespace

void yss236_core::jit_free::operator()(jit_code *code) const
{
	std::lock_guard<std::mutex> guard(lock);
	runtime().release(code->fn);
	delete code;
}
#else
static yss236_core::jit_code *compile(yss236_core &) { return nullptr; }
void yss236_core::jit_free::operator()(jit_code *code) const { delete code; }
#endif

bool yss236_core::jit_pass()
{
	static bool const off = std::getenv("FSVR_VOP3_INTERP") != nullptr;
	if (interpret || off || (n & 3))
		return false;
	if (m_stale)
	{
		m_jit.reset(compile(*this));
		m_stale = false;
	}
	if (!m_jit)
		return false;
	m_n0 = n;
	m_jit->fn(this, ram.data());
	n = m_n0 + STEPS;
	return true;
}
