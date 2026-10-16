// fx_lift - writes the effect section's programs as plain C++ DSP (src/effects/<program>.cpp).
//
// The effect chip runs one 512-step program a sample, and the firmware loads it per window: the base
// program (the mixer, sends, returns, master EQ and the reverb, steps 0x000-0x0e7) and four windows of
// it, aux 0x0e8, reverb delay 0x110, variation 0x158 and insertion 0x1a0, each holding one of the
// EPROM's programs. Nothing but registers, cells, t registers and the delay RAM crosses from one of
// those five stages to the next (fx_lift --seal proves it for every combination the EPROM holds), so
// each program is a function of that state alone.
//
// This tool runs each program through a symbolic copy of yss236_core::step(): every value is an
// expression over what the stage starts with (registers, cells, the parameter words k[], the inputs,
// the delay RAM reads), with the pipeline's three-step write latency, the latches, the captures and the
// conditional skips resolved at lift time. What is left is the arithmetic, in the chip's order, and
// that is what it prints. Nothing is reassociated or simplified except x * 1 and x - 0, which are exact;
// a + 0.0 stays, because it turns -0 into +0 as the chip does.
//
// Run it after a change to yss236_core::step() or the EPROM tables, then tools/vop3_native_check, which
// compares every program with the chip interpreter bit for bit:
//   fx_lift src/effects          rewrite the programs' bodies (each file's header comment is kept)
//   fx_lift --seal               the boundary proof alone
#include <bit>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "fs1r/chips/yss236_core.h"
#include "fs1r/firmware/vop3host/effect_host.h"

namespace H = fs1r::vop3host;
using Op = yss236_core::op;

// ---------------------------------------------------------------------------------------------
// Expressions: a hash-consed DAG, so equal subexpressions are one node and print once
// ---------------------------------------------------------------------------------------------

struct Node { std::string op; int a = -1, b = -1, c = -1; double k = 0; };
static std::vector<Node> G;
static std::map<std::tuple<std::string, int, int, int, uint64_t>, int> interned;

static int mk(const std::string& op, int a = -1, int b = -1, int c = -1, double k = 0)
{
	auto key = std::make_tuple(op, a, b, c, std::bit_cast<uint64_t>(k));   // +0.0 and -0.0 are two constants
	auto it = interned.find(key);
	if (it != interned.end()) return it->second;
	G.push_back({op, a, b, c, k});
	return interned[key] = int(G.size()) - 1;
}
static int K(double v) { return mk("#", -1, -1, -1, v); }
static int leaf(const std::string& name) { return mk(name); }
static bool isk(int n, double v) { return G[n].op == "#" && G[n].k == v && std::signbit(G[n].k) == std::signbit(v); }
static bool isk(int n) { return G[n].op == "#"; }

// Arithmetic, folding only what is exact
static int add(int a, int b) { if (isk(a) && isk(b)) return K(G[a].k + G[b].k); return mk("+", a, b); }
static int sub(int a, int b) { if (isk(a) && isk(b)) return K(G[a].k - G[b].k); if (isk(b, 0.0)) return a; return mk("-", a, b); }
static int mul(int a, int b) { if (isk(a) && isk(b)) return K(G[a].k * G[b].k); if (isk(b, 1.0)) return a; if (isk(a, 1.0)) return b; return mk("*", a, b); }
static int dvd(int a, int b) { if (isk(a) && isk(b)) return K(G[a].k / G[b].k); return mk("/", a, b); }
static int neg(int a) { if (isk(a)) return K(-G[a].k); return mk("neg", a); }
static int fn(const char* f, int a) { return mk(f, a); }

// Conditions, for the one program with conditional skips (Lo-Fi): true, false, and/or/not, sign tests
static int T() { return mk("true"); }
static int F() { return mk("false"); }
static int band(int a, int b) { if (G[a].op == "false" || G[b].op == "false") return F(); if (G[a].op == "true") return b; if (G[b].op == "true") return a; return mk("&&", a, b); }
static int bor(int a, int b) { if (G[a].op == "true" || G[b].op == "true") return T(); if (G[a].op == "false") return b; if (G[b].op == "false") return a; return mk("||", a, b); }
static int bnot(int a) { if (G[a].op == "true") return F(); if (G[a].op == "false") return T(); return mk("!", a); }
static int sel(int c, int a, int b) { if (a == b || G[c].op == "true") return a; if (G[c].op == "false") return b; return mk("?:", c, a, b); }

// The chip's number format, by name (fs1r::effects::word)
static int mode(int y, int mo) { static const char* f[4] = {"wrap", "sat", "pos", "mag"}; return fn(f[mo], y); }
static int accmode(int y, int mo) { return mo == 0 ? fn("wrap_sum", y) : mode(y, mo); }

// ---------------------------------------------------------------------------------------------
// A symbolic core: yss236_core::step() over expressions
// ---------------------------------------------------------------------------------------------

struct Pend { int reg = -1, v = -1, guard = -1; };
struct Read { int st, slot, older, tbl, idx, guard, leaf; };     // idx -1: no index
struct Write { int st, slot, tbl, idx, v, guard; };

struct Sym {
	Op ops[512];
	int r[128], m[64], t[16], out[16];
	int acc, x, px, src, psrc, wlatch, idx, flag;
	int hasx, haspx, haswl;                // conditions: whether x, px came from a register, whether the write latch holds data
	Pend rq[4], dq[4], tq[4];
	struct IdxLoad { int64_t at; int v, guard; };
	std::vector<IdxLoad> iq;
	std::vector<Read> reads;              // this pass's delay RAM reads, in step order
	std::vector<Write> writes;
	std::vector<std::pair<int, int>> skip;   // (predicate, target): "if predicate, steps below target are skipped"
	int64_t n = 0;
	int guard;                            // the step being run runs only when this holds
	std::string error;

	void init()
	{
		for (int i = 0; i < 128; i++) r[i] = leaf("r[" + std::to_string(i) + "]");
		for (int i = 0; i < 64; i++) m[i] = leaf("m[" + std::to_string(i) + "]");
		for (int i = 0; i < 16; i++) t[i] = leaf("t[" + std::to_string(i) + "]");
		for (int i = 0; i < 16; i++) out[i] = leaf("out[" + std::to_string(i) + "]");
		// what a stage must not read from before it: the latches; fx_lift fails if one reaches an output
		acc = leaf("?acc"); x = leaf("?x"); px = leaf("?px"); src = leaf("?src"); psrc = leaf("?psrc");
		wlatch = leaf("?wlatch"); idx = leaf("?idx"); flag = leaf("?flag");
		hasx = haspx = leaf("?hasx"); haswl = F();   // every stage before leaves the write latch empty (fx_lift checks it)
		skip = {{T(), 0}};
		guard = T();
	}

	int k(int st) { return leaf("k[0x" + hex(st) + "]"); }
	static std::string hex(int v) { char b[8]; std::snprintf(b, 8, "%03x", v); return b; }

	void queue(Pend* q, int reg, int v) { q[(n + yss236_core::LAT) & 3] = {reg, v, guard}; }
	void land(Pend& p, int* file)
	{
		if (p.reg >= 0) file[p.reg] = sel(p.guard, p.v, file[p.reg]);
		p.reg = -1;
	}
	void begin_step()
	{
		n++;
		if (rq[n & 3].reg > 0) land(rq[n & 3], r); else rq[n & 3].reg = -1;
		land(dq[n & 3], m);
		land(tq[n & 3], t);
	}
	int cur_idx()
	{
		while (!iq.empty() && iq.front().at <= n) { idx = sel(iq.front().guard, iq.front().v, idx); iq.erase(iq.begin()); }
		return idx;
	}
	int from_step = 0;                    // the stage's first step
	int captured(int old, int st)
	{
		// the latest read of this pass made CAPMIN..11 steps back; a read that may not have run is a choice
		std::function<int(int)> from = [&](int i) -> int {
			for (; i >= 0; i--) {
				if (reads[i].st <= st - 12) break;
				if (reads[i].st <= st - yss236_core::CAPMIN)
					return G[reads[i].guard].op == "true" ? reads[i].leaf : sel(reads[i].guard, reads[i].leaf, from(i - 1));
			}
			// none of this stage's: an earlier stage's read in the window would be the one taken
			for (int s = st - 11; s <= st - yss236_core::CAPMIN && s < from_step; s++)
				if (s >= yss236_core::XFER_FROM && !ops[s].ctlop && (ops[s].memop == 2 || ops[s].memop == 3 || ops[s].memop == 4 || ops[s].memop == 6))
					error = "a capture reaches into the stage before";
			return old;
		};
		return from(int(reads.size()) - 1);
	}
	int adder(Op const& o)
	{
		switch (o.asel) {
			case yss236_core::A_P: return acc;
			case yss236_core::A_R: return o.sr ? x : dvd(acc, K(32768.0));
			case yss236_core::A_M: return o.sm ? src : dvd(acc, K(32768.0));
			default: return K(0.0);
		}
	}
	void t_write(Op const& o, int st, int y)
	{
		int const v = o.tfrac ? fn("frac", y) : fn("q15", y);
		if (o.tw & 2) queue(tq, o.tn, v);
		else if (o.tw & 1) queue(tq, o.tn, k(st));
	}

	// Whether step st runs: the predicates of every skip that ended at or before it
	int runs(int st)
	{
		int p = F();
		for (auto& [pred, target] : skip) if (target <= st) p = bor(p, pred);
		return p;
	}

	void step(int st)
	{
		begin_step();
		Op const& o = ops[st];
		int const g = runs(st);
		if (G[g].op == "false") return;
		if (o.ctlop) {
			// a conditional skip: under g, unless its condition holds, the steps up to the target are skipped
			if (o.gate) { error = "a gate in an effect program"; return; }
			int const cond = o.cskip == 6 ? fn("positive", flag) : o.cskip == 4 ? fn("negative", flag) : F();
			int const target = (st & 0x100) | o.target;
			std::vector<std::pair<int, int>> next;
			for (auto& [pred, tg] : skip) {
				if (tg <= st) {
					next.push_back({band(pred, cond), tg});
					next.push_back({band(pred, bnot(cond)), target});
				} else next.push_back({pred, tg});
			}
			skip.clear();
			for (auto& e : next) if (G[e.first].op != "false") skip.push_back(e);
			return;
		}
		if (G[g].op == "true") { body(st, o); return; }
		// A step that may not run: run it on a copy and keep each change only under g
		Sym was = *this;
		guard = g;
		body(st, o);
		guard = T();
		merge(was, g);
	}

	void merge(Sym const& was, int g)
	{
		for (int i = 0; i < 128; i++) r[i] = sel(g, r[i], was.r[i]);
		for (int i = 0; i < 64; i++) m[i] = sel(g, m[i], was.m[i]);
		for (int i = 0; i < 16; i++) { t[i] = sel(g, t[i], was.t[i]); out[i] = sel(g, out[i], was.out[i]); }
		acc = sel(g, acc, was.acc); x = sel(g, x, was.x); px = sel(g, px, was.px);
		src = sel(g, src, was.src); psrc = sel(g, psrc, was.psrc); wlatch = sel(g, wlatch, was.wlatch);
		flag = sel(g, flag, was.flag); idx = sel(g, idx, was.idx);
		hasx = sel(g, hasx, was.hasx); haspx = sel(g, haspx, was.haspx); haswl = sel(g, haswl, was.haswl);
		if (iq.size() != was.iq.size()) error = "a delay index load under a skip";
	}

	void body(int st, Op const& o)
	{
		int const cls = o.mmode, route = o.shift, mo = o.clamp, page = (st >> 5) & 15, s = st >> 2;
		bool const xfer = st >= yss236_core::XFER_FROM;
		int const rscale = K(yss236_core::RSCALE[route]);

		if (xfer && (o.memop == 2 || o.memop == 3 || o.memop == 4 || o.memop == 6)) {
			int const ix = o.idxa ? cur_idx() : -1;
			int const id = int(reads.size());
			reads.push_back({st, s, (o.memop & 4) ? 1 : 0, o.tbl, ix, guard, leaf("d" + std::to_string(id))});
		}
		if (o.dm) {
			switch (o.msrc) {
				case yss236_core::M_SERIAL: case yss236_core::M_SERIAL2: case yss236_core::M_EG:
					queue(dq, o.dm, leaf("in[" + std::to_string(o.msrc) + "][" + std::to_string(page) + "]")); break;
				case yss236_core::M_LFO: queue(dq, o.dm, leaf("lfo[" + std::to_string(st >> 5) + "]")); break;
				case yss236_core::M_CAPTURE: case yss236_core::M_CAPTURE2: queue(dq, o.dm, captured(m[o.dm], st)); break;
				default: break;
			}
		}
		if (o.sout) out[page] = m[o.sm];
		if (cls != 2 && cls != 3) { px = x; haspx = hasx; hasx = o.sr ? T() : F(); x = o.sr ? r[o.sr] : K(0.0); }

		if (cls == 1) {
			psrc = src; src = m[o.sm];
			int const a = adder(o);
			int v = add(mul(K(yss236_core::LOADK), k(st)), o.sub ? neg(a) : a);
			v = mul(v, rscale);
			if (o.dr) queue(rq, o.dr, mode(v, mo));
			acc = accmode(v, mo);
			if (o.cmp) flag = acc;
			if (o.msrc == yss236_core::M_RESULT && o.dm && (o.dwr || !o.dr)) queue(dq, o.dm, fn("cell", acc));
			t_write(o, st, acc);
			return;
		}
		if (cls == 0) {
			if (xfer && o.memop == 1 && G[haswl].op != "false") {
				writes.push_back({st, s, o.tbl, o.idxa ? cur_idx() : -1, wlatch, band(guard, haswl)});
				haswl = F();
			}
			if (o.alu) {
				psrc = src; src = m[o.sm];
				int y = o.sub ? neg(adder(o)) : adder(o);
				y = mul(y, rscale);
				if (o.dr && !o.rcopy) queue(rq, o.dr, mode(y, o.clamp));
				acc = accmode(y, o.clamp);
				if (o.cmp) flag = acc;
			}
			return;
		}
		int const kk = k(st);
		int const gm = cls == 3 ? K(1.0) : o.m1 >= 2 ? t[((o.m1 & 1) << 3) | o.t] : kk;
		int const xv = o.sr ? r[o.sr] : K(0.0);
		px = x; haspx = hasx; x = xv; hasx = o.sr ? T() : F();
		psrc = src; src = m[o.sm];
		int const m2 = o.m2m ? src : xv;
		int const prod = cls == 3 ? m2 : mul(gm, m2);
		int const a = adder(o);
		int y = mul(o.sub ? sub(prod, a) : add(prod, a), rscale);
		if (o.dr && o.rcopy) queue(rq, o.dr, sel(haspx, px, xv));
		else if (o.dr) queue(rq, o.dr, mode(y, mo));
		y = acc = accmode(y, mo);
		if (o.cmp) flag = y;
		int outv = fn("clip32", y);
		if (o.dm) {
			if (o.msrc == yss236_core::M_PREVIOUS) queue(dq, o.dm, psrc);
			else if (o.msrc == yss236_core::M_RESULT && (!(o.m2m && o.asel == yss236_core::A_NONE) || o.dm != o.sm)) queue(dq, o.dm, fn("cell", y));
		}
		t_write(o, st, y);
		if (o.idxw) { cur_idx(); iq.push_back({n + yss236_core::LAT, fn("index", y), guard}); }
		if (o.wdata) {
			if (xfer && o.memop == 1) haswl = F();
			else { wlatch = outv; haswl = T(); }
		}
		if (xfer && o.memop == 1) {
			outv = sel(haswl, wlatch, outv); haswl = F();
			writes.push_back({st, s, o.tbl, o.idxa ? cur_idx() : -1, outv, guard});
		}
	}
};

// ---------------------------------------------------------------------------------------------
// Printing a stage as C++
// ---------------------------------------------------------------------------------------------

struct Printer {
	std::vector<std::string> lines;
	std::unordered_map<int, std::string> named;
	std::unordered_map<int, int> uses;
	int temps = 0;

	void count(int i)
	{
		if (uses[i]++ || G[i].a < 0) return;
		count(G[i].a);
		if (G[i].b >= 0) count(G[i].b);
		if (G[i].c >= 0) count(G[i].c);
	}
	static std::string num(double v)
	{
		char b[40];
		std::snprintf(b, sizeof b, "%.17g", v);
		std::string s = b;
		if (s.find_first_of(".en") == std::string::npos) s += ".0";
		return s;
	}
	// The expression for node i; a node used more than once becomes a named local the first time
	std::string ex(int i, bool top = false)
	{
		auto it = named.find(i);
		if (it != named.end()) return it->second;
		Node const& n = G[i];
		std::string s;
		if (n.op == "#") s = num(n.k);
		else if (n.op == "true" || n.op == "false") s = n.op;
		else if (n.a < 0) {
			if (n.op[0] == '?') throw std::runtime_error("the stage reads " + n.op.substr(1) + " from before it");
			s = n.op;
		}
		else if (n.op == "+" || n.op == "-" || n.op == "*" || n.op == "/") s = "(" + ex(n.a) + " " + n.op + " " + ex(n.b) + ")";
		else if (n.op == "&&" || n.op == "||") s = "(" + ex(n.a) + " " + n.op + " " + ex(n.b) + ")";
		else if (n.op == "neg") s = "-" + ex(n.a);
		else if (n.op == "!") s = "!" + ex(n.a);
		else if (n.op == "?:") s = "(" + ex(n.a) + " ? " + ex(n.b) + " : " + ex(n.c) + ")";
		else if (n.op == "positive") s = "(" + ex(n.a) + " > 0)";
		else if (n.op == "negative") s = "(" + ex(n.a) + " < 0)";
		else if (n.op == "wrap_sum") s = running_sum(i);
		else s = n.op + "(" + strip(ex(n.a)) + ")";
		if (!top && uses[i] > 1 && n.a >= 0) {
			std::string const v = "v" + std::to_string(temps++);
			bool const cond = n.op == "&&" || n.op == "||" || n.op == "!" || n.op == "positive" || n.op == "negative";
			lines.push_back(std::string("\t") + (cond ? "bool" : "double") + " const " + v + " = " + strip(s) + ";");
			named[i] = v;
			return v;
		}
		return s;
	}
	// A run of products into the running sum, as word::sum(a, b, c) for wrap_sum(c + wrap_sum(b + wrap_sum(a +
	// 0.0))), or word::sum_on(s, a, b) carrying on from a sum s that has a name of its own. Same adds, same order.
	std::string running_sum(int i)
	{
		std::vector<int> terms;
		int at = i;
		for (;;) {
			Node const& n = G[at];
			if (n.op != "wrap_sum" || G[n.a].op != "+" || (at != i && (uses[at] > 1 || named.count(at)))) break;
			Node const& add = G[n.a];
			terms.push_back(add.a);
			at = add.b;
		}
		if (terms.empty()) return "wrap_sum(" + strip(ex(G[i].a)) + ")";
		std::string args;
		for (auto it = terms.rbegin(); it != terms.rend(); ++it) args += (args.empty() ? "" : ", ") + strip(ex(*it));
		if (isk(at, 0.0)) return "sum(" + args + ")";
		return "sum_on(" + strip(ex(at)) + ", " + args + ")";
	}
	static std::string strip(std::string s)
	{
		// one pair of outer parentheses is noise around a whole statement or argument
		if (s.size() > 1 && s.front() == '(' && s.back() == ')') {
			int depth = 0;
			for (size_t i = 0; i < s.size(); i++) {
				depth += s[i] == '(' ? 1 : s[i] == ')' ? -1 : 0;
				if (depth == 0 && i + 1 < s.size()) return s;
			}
			return s.substr(1, s.size() - 2);
		}
		return s;
	}
};

// Where `name` (such as t[1]) is used in e as a name of its own, not as the end of another (out[1])
static size_t refers(std::string const& e, std::string const& name)
{
	for (size_t p = e.find(name); p != std::string::npos; p = e.find(name, p + 1))
		if (p == 0 || !(std::isalnum((unsigned char)e[p - 1]) || e[p - 1] == '_' || e[p - 1] == '.')) return p;
	return std::string::npos;
}

static std::string lift(Sym& S, int from, int to, std::string& fail)
{
	S.from_step = from;
	for (int st = from; st < to; st++) S.step(st);
	for (int i = 0; i < 4; i++) S.begin_step();   // what is in flight lands; the next stage reads none of it early
	if (!S.iq.empty()) S.cur_idx();
	if (!S.error.empty()) { fail = S.error; return ""; }
	if (G[S.haswl].op != "false") { fail = "the stage leaves the write latch holding data"; return ""; }

	// Every statement in program order: the delay RAM accesses, then the stage's results.
	struct Store { std::string dest; int v; };
	std::vector<Store> stores;
	Sym fresh; fresh.init();
	for (int i = 0; i < 128; i++) if (S.r[i] != fresh.r[i]) stores.push_back({"r[" + std::to_string(i) + "]", S.r[i]});
	for (int i = 0; i < 64; i++) if (S.m[i] != fresh.m[i]) stores.push_back({"m[" + std::to_string(i) + "]", S.m[i]});
	for (int i = 0; i < 16; i++) if (S.t[i] != fresh.t[i]) stores.push_back({"t[" + std::to_string(i) + "]", S.t[i]});
	for (int i = 0; i < 16; i++) if (S.out[i] != fresh.out[i]) stores.push_back({"out[" + std::to_string(i) + "]", S.out[i]});

	Printer P;
	// which reads anything uses: a read nobody captures moves nothing
	for (auto& w : S.writes) { P.count(w.v); if (w.idx >= 0) P.count(w.idx); P.count(w.guard); }
	for (auto& s : stores) P.count(s.v);
	std::set<int> used;
	for (auto& rd : S.reads) if (P.uses.count(rd.leaf)) { used.insert(rd.leaf); if (rd.idx >= 0) P.count(rd.idx); P.count(rd.guard); }

	// A delay RAM word: the slot's own offset, one word older, and the index, as the chip adds them
	auto address = [&](int slot, int older, int tbl, int idx) {
		char a[8];
		std::snprintf(a, sizeof a, "0x%02x", slot);
		std::string o = older ? "1" : "";
		if (idx >= 0) o += std::string(older ? " + " : "") + "int(" + Printer::strip(P.ex(idx)) + ")";
		return std::string(tbl ? "table(" : "at(") + a + (o.empty() ? "" : ", " + o) + ")";
	};
	try {
		// reads and writes interleave in step order: a read after a write to the same word must see it
		size_t wi = 0;
		for (auto& rd : S.reads) {
			for (; wi < S.writes.size() && S.writes[wi].st < rd.st; wi++) {
				auto& w = S.writes[wi];
				std::string const v = Printer::strip(P.ex(w.v));
				std::string const g = G[w.guard].op == "true" ? "" : "if (" + Printer::strip(P.ex(w.guard)) + ") ";
				P.lines.push_back("\t" + g + "s.ram[s." + address(w.slot, 0, w.tbl, w.idx) + "] = " + v + ";");
			}
			if (!used.count(rd.leaf)) continue;
			P.lines.push_back("\tdouble const " + G[rd.leaf].op + " = s.ram[s." + address(rd.slot, rd.older, rd.tbl, rd.idx) + "];");
			P.named[rd.leaf] = G[rd.leaf].op;
		}
		for (; wi < S.writes.size(); wi++) {
			auto& w = S.writes[wi];
			std::string const v = Printer::strip(P.ex(w.v));
			std::string const g = G[w.guard].op == "true" ? "" : "if (" + Printer::strip(P.ex(w.guard)) + ") ";
			P.lines.push_back("\t" + g + "s.ram[s." + address(w.slot, 0, w.tbl, w.idx) + "] = " + v + ";");
		}
		// the results: every expression they need first, since a store may overwrite what another reads
		std::vector<std::pair<std::string, std::string>> rhs;
		for (auto& s : stores) rhs.push_back({s.dest, Printer::strip(P.ex(s.v, true))});
		// then the stores, each one after every store whose expression still reads what it overwrites
		std::vector<bool> done(rhs.size());
		for (size_t left = rhs.size(); left;) {
			bool progress = false;
			for (size_t i = 0; i < rhs.size(); i++) {
				if (done[i]) continue;
				bool blocked = false;
				for (size_t j = 0; j < rhs.size() && !blocked; j++)
					if (j != i && !done[j] && refers(rhs[j].second, rhs[i].first) != std::string::npos) blocked = true;
				if (blocked) continue;
				P.lines.push_back("\t" + rhs[i].first + " = " + rhs[i].second + ";");
				done[i] = true; left--; progress = true;
			}
			if (!progress) {   // a cycle, such as two registers swapping: keep the first one's value aside
				for (size_t i = 0; i < rhs.size(); i++) if (!done[i]) {
					std::string const v = "v" + std::to_string(P.temps++);
					P.lines.push_back("\tdouble const " + v + " = " + rhs[i].first + ";");
					for (size_t j = 0; j < rhs.size(); j++) if (!done[j] && j != i) {
						std::string& e = rhs[j].second;
						for (size_t p; (p = refers(e, rhs[i].first)) != std::string::npos;) e.replace(p, rhs[i].first.size(), v);
					}
					break;
				}
			}
		}
	} catch (std::exception& e) { fail = e.what(); return ""; }
	std::string body;
	for (auto& l : P.lines) body += l + "\n";
	return body;
}

// ---------------------------------------------------------------------------------------------
// The programs
// ---------------------------------------------------------------------------------------------

struct Program { std::string key; int window, msb, from, to; };

// The programs the host uploads: the base program and the aux window's, each window's program for the type
// msbs fx_dispatch.cpp routes to it, and each window's clear entry. The EPROM holds others no type loads.
static std::vector<Program> programs()
{
	std::vector<Program> ps = {{"base", -1, 0, 0x000, 0x0e8}, {"aux", H::AUX, 0, 0x0e8, 0x110}};
	const struct { int w; const char* prefix; std::vector<int> msbs; } windows[] = {
		{H::REVERB_DELAY, "dly", {0, 8, 11}},
		{H::VARIATION, "var", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
		{H::INSERTION, "ins", {0, 1, 2, 3, 4, 5, 7, 10, 12, 13, 14}},
	};
	for (auto& wn : windows)
		for (int msb : wn.msbs) {
			char key[16];
			std::snprintf(key, sizeof key, "%s_m%02d", wn.prefix, msb);
			H::Window const& W = H::WINDOWS[wn.w];
			ps.push_back({key, wn.w, msb, W.steps[0], W.steps[0] + W.nsteps});
		}
	return ps;
}

static void load(Sym& S, Program const& p)
{
	for (int st = 0; st < 512; st++) yss236_core::decode(H::rom::BASE_PROGRAM[st], S.ops[st]);
	if (p.window >= 0) {
		H::Window const& W = H::WINDOWS[p.window];
		for (int i = 0; i < W.nsteps; i++) yss236_core::decode(W.programs[p.msb * W.nsteps + i], S.ops[W.steps[i]]);
	}
}

static const char* NOTE = "// Lifted by tools/fx_lift; tools/vop3_native_check compares it with the chip, every sample.";

// The header comment of an existing file, which says what the program is; the lift replaces the rest
static std::string header(const std::string& path, const std::string& key)
{
	std::ifstream f(path);
	std::string line, h;
	while (std::getline(f, line) && line.rfind("//", 0) == 0 && line.rfind(NOTE, 0) != 0) h += line + "\n";
	while (h.size() >= 3 && h.compare(h.size() - 3, 3, "//\n") == 0) h.resize(h.size() - 3);   // the note's separator
	if (h.empty()) h = "// effects/" + key + ".cpp\n";
	return h;
}

int main(int argc, char** argv)
{
	if (argc < 2) { std::fprintf(stderr, "usage: fx_lift <src/effects> | --seal\n"); return 2; }
	int fails = 0;
	for (Program const& p : programs()) {
		G.clear(); interned.clear();
		Sym S; S.init(); load(S, p);
		std::string why;
		std::string const body = lift(S, p.from, p.to, why);
		if (body.empty() && !why.empty()) { std::printf("%-8s does not lift: %s\n", p.key.c_str(), why.c_str()); fails++; continue; }
		if (std::string(argv[1]) == "--seal") { std::printf("%-8s sealed\n", p.key.c_str()); continue; }
		std::string const path = std::string(argv[1]) + "/" + p.key + ".cpp";
		{   // a program written by hand (no lift note) is the source now: leave it
			std::ifstream f(path);
			std::stringstream text; text << f.rdbuf();
			if (f && text.str().find(NOTE) == std::string::npos) { std::printf("%-8s written by hand, left\n", p.key.c_str()); continue; }
		}
		std::string out = header(path, p.key);
		out += std::string("//\n") + NOTE + "\n\n"
		       "#include \"effects/programs.h\"\n\nnamespace fs1r::effects {\n\nusing namespace word;\n\n";
		out += "void " + p.key + "(State& s)\n{\n";
		for (const char* a : {"r", "m", "t", "out", "k", "in", "lfo"})
			if (refers(body, std::string(a) + "[") != std::string::npos)
				out += std::string("\t") + (std::string(a) == "k" || std::string(a) == "in" || std::string(a) == "lfo" ? "auto const& " : "auto& ") + a + " = s." + a + ";\n";
		out += body;
		out += "}\n\n}  // namespace fs1r::effects\n";
		std::ofstream(path) << out;
		std::printf("%-8s %zu lines\n", p.key.c_str(), std::count(body.begin(), body.end(), '\n'));
	}
	return fails ? 1 : 0;
}
