#!/usr/bin/env python3
"""Read VOP3-2's per-type step constants and delay offsets out of the FS1R v1.20 firmware by running it.

    python tools/extract_vop3_2_params.py            # emulate (about 15 min on 8 cores), then write the docs
    python tools/extract_vop3_2_params.py --reuse    # rewrite the docs from the last emulation in --work
    python tools/extract_vop3_2_params.py --reuse --curves /tmp/curves.json   # also dump every value sweep

Writes docs/vop3_2_params.md and docs/vop3_2/params.json.

The effect parameter handlers fill DAT_0106842c (u16 per program step, register 0xB) and DAT_0106882c
(s32 per delay slot, registers 0xD/0xE) through soft-float code in the EPROM (FUN_00037414, the
FUN_002030b4 family), so rather than transcribe ~50 handlers this runs them in Ghidra's p-code emulator
(EmulatorHelper, analyzeHeadless) against the read-only FS1R project:

  1. FUN_0003AAC8, the effect init, once.
  2. per effect type: set the type bytes (DAT_0106840D..DAT_01068415) and call FUN_0039B09C(kind, 0xFF),
     which loads the type's preset block (parameters, constants, offsets) from EPROM and runs the
     handler for every parameter. DAT_0106842C/DAT_0106882C afterwards are the type's defaults.
  3. per parameter p: fill both arrays with markers, call FUN_0039B09C(kind, p) and record what changed
     (the steps and slots p writes), with every function entered (which handler, which helpers);
     then sweep the parameter's value and record the written words for each value.
  4. the dispatcher's 0xFF path on markers, to separate handler-written steps from preset-block ones.

The self-check at the end asserts that every step and slot the handlers do not write still holds the
word of the type's EPROM preset block, read here independently, so a wrong block address or a broken
emulation fails the run.
"""
import argparse
import math
import getpass
import glob
import json
import os
import re
import shutil
import sqlite3
import struct
import subprocess
import sys
import tempfile
from collections import OrderedDict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIS = ROOT.parent / "FS1R_DISASM"
ROM = DIS / "roms/fs1r_v120_eprom_cpuview.bin"
PROJ = DIS / "FS1R_GHIDRA_PROJ"
DB = DIS / "FS1R_GHIDRA_ANALYSIS/decomp.db"
OUT_MD = ROOT / "docs/vop3_2_params.md"
OUT_JSON = ROOT / "docs/vop3_2/params.json"
DISASM = ROOT / "docs/vop3_disasm"
BASE = 0x200000          # the EPROM is at 0x200000 in the CPU map (tools/extract_vop3_2.py)

JAVA = r"""
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.util.task.TimeoutTaskMonitor;
import java.math.BigInteger;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.TimeUnit;

public class Vop3Params extends GhidraScript {
    EmulatorHelper emu;
    static final long SENT = 0x3fff0L, CONST = 0x0106842cL, OFFS = 0x0106882cL;
    static final long[] PARAMS = {0x01068cecL, 0x01068d0cL, 0x01068d2cL, 0x01068d4cL, 0x01068d6cL, 0x01068d8cL, 0x01068dbcL};
    static final int[] NP = {16, 16, 16, 16, 16, 20, 15};
    static final long[] DISP = {0x39c144L, 0x39c18cL, 0x39c1acL, 0x39c48aL, 0x39c51cL, 0x3a356cL, 0x3a2ecL};
    boolean tracing = false;
    Set<Long> entries = new HashSet<>(), hits = new TreeSet<>();
    List<String> designs = new ArrayList<>();

    Address a(long x) { return toAddr(x); }
    void w8(long ad, int v) { emu.writeMemory(a(ad), new byte[]{(byte)v}); }
    void w16(long ad, int v) { emu.writeMemory(a(ad), new byte[]{(byte)(v >> 8), (byte)v}); }
    // Ghidra's SuperH semantics store Rn-4 for "mov.x Rn,@-Rn" and keep the increment for "mov.x @Rn+,Rn"; the
    // SH-2 stores the original Rn and keeps the loaded value, and the compiler relies on it (every soft-float
    // call pushes its result pointer with mov.l r15,@-r15 in the jsr delay slot). FIX maps a breakpoint (the
    // instruction, or the branch whose delay slot it is) to {0 store / 1 load, register, size}; fixed after the step.
    Map<Long, int[]> fix = new HashMap<>();
    void findFixes() {
        long[][] ranges = {{0x0L, 0x40000L}, {0x200000L, 0x400000L}};
        for (long[] r : ranges) {
            byte[] b = new byte[(int)(r[1] - r[0])];
            try { currentProgram.getMemory().getBytes(a(r[0]), b); } catch (Exception e) { continue; }
            for (int i = 2; i + 1 < b.length; i += 2) {
                int op = ((b[i] & 0xff) << 8) | (b[i+1] & 0xff), n = (op >> 8) & 0xf, m = (op >> 4) & 0xf, lo = op & 0xf, hi = op >> 12;
                if (n != m || lo < 4 || lo > 6 || (hi != 2 && hi != 6)) continue;
                int[] f = {hi == 2 ? 0 : 1, n, 1 << (lo - 4)};
                int pv = ((b[i-2] & 0xff) << 8) | (b[i-1] & 0xff);
                boolean slot = (pv & 0xf0ff) == 0x400b || (pv & 0xf0ff) == 0x402b || (pv & 0xf000) == 0xa000 || (pv & 0xf000) == 0xb000
                    || pv == 0x000b || pv == 0x002b || (pv & 0xff00) == 0x8d00 || (pv & 0xff00) == 0x8f00 || (pv & 0xf0ff) == 0x0023 || (pv & 0xf0ff) == 0x0003;
                fix.put(r[0] + i, f);
                if (slot) fix.put(r[0] + i - 2, f);
            }
        }
        for (long k : fix.keySet()) emu.setBreakpoint(a(k));
    }
    long reg(int n) { return emu.readRegister("r" + n).longValue() & 0xffffffffL; }
    void stepAt(long pc) throws Exception {
        boolean bp = fix.containsKey(pc) || entries.contains(pc) || pc == SENT;
        if (bp) emu.clearBreakpoint(a(pc));
        if (!emu.step(monitor)) throw new Exception("step failed at " + Long.toHexString(pc) + " " + emu.getLastError());
        if (bp) emu.setBreakpoint(a(pc));
    }
    void doFix(long pc) throws Exception {
        int[] f = fix.get(pc);
        long before = reg(f[1]);
        stepAt(pc);
        if (f[0] == 0) {
            long rn = reg(f[1]), v = rn + f[2];
            byte[] w = new byte[f[2]];
            for (int i = 0; i < f[2]; i++) w[i] = (byte)(v >> (8 * (f[2] - 1 - i)));
            emu.writeMemory(a(rn), w);
        } else {
            byte[] m = emu.readMemory(a(before), f[2]);
            long v = 0;
            for (byte x : m) v = (v << 8) | (x & 0xff);
            int sh = 64 - 8 * f[2];
            v = ((v << sh) >> sh) & 0xffffffffL;
            emu.writeRegister("r" + f[1], BigInteger.valueOf(v));
        }
    }
    void call(long fn, long... args) throws Exception {
        String[] regs = {"r4", "r5", "r6", "r7"};
        for (int i = 0; i < args.length; i++) emu.writeRegister(regs[i], BigInteger.valueOf(args[i]));
        emu.writeRegister("r15", BigInteger.valueOf(0x0107f000L));
        emu.writeRegister("pr", BigInteger.valueOf(SENT));
        emu.writeRegister(emu.getPCRegister(), BigInteger.valueOf(fn));
        while (true) {
            long pc = emu.getExecutionAddress().getOffset();
            if (pc == SENT) return;
            if (fix.containsKey(pc)) { doFix(pc); continue; }
            if (tracing && entries.contains(pc)) {
                hits.add(pc);
                if (pc == 0x37414L) {   // FUN_00037414(order, scale, kind, freq; gain @sp+3, q @sp+7, step list @sp+8)
                    long sp = reg(15);
                    byte[] st = emu.readMemory(a(sp), 12);
                    long lst = ((st[8] & 0xffL) << 24) | ((st[9] & 0xffL) << 16) | ((st[10] & 0xffL) << 8) | (st[11] & 0xffL);
                    byte[] ls = emu.readMemory(a(lst), 12);
                    StringBuilder d = new StringBuilder("[" + (reg(4) & 0xff) + "," + (reg(5) & 0xff) + "," + (reg(6) & 0xff) + "," + (reg(7) & 0xff) + "," + (st[3] & 0xff) + "," + (st[7] & 0xff) + ",[");
                    for (int i = 0; i < 6; i++) { if (i > 0) d.append(','); d.append(((ls[2*i] & 0xff) << 8) | (ls[2*i+1] & 0xff)); }
                    designs.add(d.append("]]").toString());
                }
                stepAt(pc); continue;
            }
            if (entries.contains(pc) || pc == fn) { stepAt(pc); continue; }   // a breakpoint we are not tracing
            emu.run(TimeoutTaskMonitor.timeoutIn(60, TimeUnit.SECONDS, monitor));
            long pc2 = emu.getExecutionAddress().getOffset();
            if (pc2 != SENT && !fix.containsKey(pc2) && !entries.contains(pc2)) throw new Exception("stopped at " + Long.toHexString(pc2) + " " + emu.getLastError());
        }
    }
    int[] consts() { byte[] b = emu.readMemory(a(CONST), 0x400); int[] r = new int[0x200]; for (int i = 0; i < 0x200; i++) r[i] = ((b[2*i] & 0xff) << 8) | (b[2*i+1] & 0xff); return r; }
    int[] offs() { byte[] b = emu.readMemory(a(OFFS), 0x200); int[] r = new int[0x80]; for (int i = 0; i < 0x80; i++) r[i] = ((b[4*i] & 0xff) << 24) | ((b[4*i+1] & 0xff) << 16) | ((b[4*i+2] & 0xff) << 8) | (b[4*i+3] & 0xff); return r; }
    void put(int[] c, int[] o) {
        byte[] b = new byte[0x400]; for (int i = 0; i < 0x200; i++) { b[2*i] = (byte)(c[i] >> 8); b[2*i+1] = (byte)c[i]; } emu.writeMemory(a(CONST), b);
        b = new byte[0x200]; for (int i = 0; i < 0x80; i++) { b[4*i] = (byte)(o[i] >> 24); b[4*i+1] = (byte)(o[i] >> 16); b[4*i+2] = (byte)(o[i] >> 8); b[4*i+3] = (byte)o[i]; } emu.writeMemory(a(OFFS), b);
    }
    int[] params(int k) { byte[] b = emu.readMemory(a(PARAMS[k]), 2 * NP[k]); int[] r = new int[NP[k]]; for (int i = 0; i < NP[k]; i++) r[i] = ((b[2*i] & 0xff) << 8) | (b[2*i+1] & 0xff); return r; }
    static String arr(Collection<?> x) { StringBuilder s = new StringBuilder("["); for (Object o : x) { if (s.length() > 1) s.append(','); s.append(o); } return s.append(']').toString(); }
    static String arr(int[] x) { List<Integer> l = new ArrayList<>(); for (int v : x) l.add(v); return arr(l); }

    // which steps/slots does fn(args) write: two marker fills, so a write of the marker value itself still shows
    Set<Integer>[] written(long fn, long... args) throws Exception {
        Set<Integer> wc = new TreeSet<>(), wo = new TreeSet<>();
        for (int m = 0; m < 2; m++) {
            int[] mc = new int[0x200], mo = new int[0x80];
            for (int i = 0; i < 0x200; i++) mc[i] = (m == 0 ? 0xA000 : 0x5000) | i;
            for (int i = 0; i < 0x80; i++) mo[i] = (m == 0 ? 0x7A000000 : 0x75000000) | i;
            put(mc, mo);
            call(fn, args);
            int[] c1 = consts(), o1 = offs();
            for (int i = 0; i < 0x200; i++) if (c1[i] != mc[i]) wc.add(i);
            for (int i = 0; i < 0x80; i++) if (o1[i] != mo[i]) wo.add(i);
        }
        return new Set[]{wc, wo};
    }

    public void run() throws Exception {
        String[] args = getScriptArgs();
        PrintWriter out = new PrintWriter(new FileWriter(args[1]));
        emu = new EmulatorHelper(currentProgram);
        emu.setBreakpoint(a(SENT));
        findFixes();
        call(0x3aac8L);
        List<Address> fns = new ArrayList<>();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            long e = f.getEntryPoint().getOffset();
            if (!f.isThunk() && e != 0x39648L && e != 0x39b09cL) fns.add(f.getEntryPoint());
        }
        for (String job : Files.readAllLines(Paths.get(args[0]))) {
            String[] f = job.trim().split("\\s+");
            if (f.length < 4) continue;
            int kind = Integer.parseInt(f[1]), msb = Integer.parseInt(f[2]), lsb = Integer.parseInt(f[3]);
            if (kind == 0) w8(0x0106840dL, msb);
            else if (kind <= 4) { w8(0x0106840eL + 2 * (kind - 1), msb); w8(0x0106840fL + 2 * (kind - 1), lsb); }
            call(0x39b09cL, kind, 0xff);
            int[] c0 = consts(), o0 = offs(), p0 = params(kind);
            StringBuilder s = new StringBuilder();
            s.append("{\"tag\":\"").append(f[0]).append("\",\"kind\":").append(kind).append(",\"msb\":").append(msb).append(",\"lsb\":").append(lsb);
            s.append(",\"params\":").append(arr(p0)).append(",\"const\":").append(arr(c0)).append(",\"offs\":").append(arr(o0));
            Set<Integer>[] all = written(DISP[kind], 0xff);
            put(c0, o0);
            s.append(",\"all_c\":").append(arr(all[0])).append(",\"all_o\":").append(arr(all[1])).append(",\"per\":[");
            for (Address e : fns) { entries.add(e.getOffset()); emu.setBreakpoint(e); }
            List<String> traces = new ArrayList<>();
            List<String> dsg = new ArrayList<>();
            for (int p = 0; p < NP[kind]; p++) { hits.clear(); designs.clear(); tracing = true; call(0x39b09cL, kind, p); tracing = false; traces.add(arr(hits)); dsg.add(arr(designs)); }
            for (Address e : fns) emu.clearBreakpoint(e);
            entries.clear();
            for (long k : fix.keySet()) emu.setBreakpoint(a(k));
            for (int p = 0; p < NP[kind]; p++) {
                Set<Integer>[] w = written(0x39b09cL, kind, p);
                put(c0, o0);
                if (p > 0) s.append(',');
                s.append("{\"p\":").append(p).append(",\"trace\":").append(traces.get(p)).append(",\"designs\":").append(dsg.get(p)).append(",\"wc\":").append(arr(w[0])).append(",\"wo\":").append(arr(w[1])).append(",\"sweep\":{");
                if (!w[0].isEmpty() || !w[1].isEmpty()) {
                    List<Integer> vs = new ArrayList<>();
                    for (int v = 0; v <= 127; v++) vs.add(v);
                    if (kind <= 4 && (kind != 0 || p < 8)) for (int v : new int[]{200, 255, 256, 511, 1000, 1023, 2000, 4095, 6820, 8191, 13650, 16383}) vs.add(v);
                    boolean first = true;
                    for (int v : vs) {
                        w16(PARAMS[kind] + 2 * p, v);
                        try { call(0x39b09cL, kind, p); } catch (Exception e) { continue; }
                        int[] c1 = consts(), o1 = offs();
                        List<Integer> lc = new ArrayList<>(), lo = new ArrayList<>();
                        for (int i : w[0]) lc.add(c1[i]);
                        for (int i : w[1]) lo.add(o1[i]);
                        if (!first) s.append(',');
                        first = false;
                        s.append('"').append(v).append("\":[").append(arr(lc)).append(',').append(arr(lo)).append(']');
                    }
                    w16(PARAMS[kind] + 2 * p, p0[p]);
                    put(c0, o0);
                    call(0x39b09cL, kind, p);
                }
                s.append("}}");
            }
            s.append("]}");
            out.println(s); out.flush();
            println("done " + job);
        }
        out.close();
        emu.dispose();
    }
}
"""


# ------------------------------------------------------------------------------------------ ROM side
rom = ROM.read_bytes()


def rb(a, n):
    return rom[a - BASE:a - BASE + n]


def u16s(a, n):
    return list(struct.unpack(">%dH" % n, rb(a, 2 * n)))


# Type tables FUN_0003AC88 reads, two bytes per Data List type: (msb, lsb) of the firmware type.
#   reverb 0x372124 (performance byte 0xA8): msb 0 -> kind 0 type lsb, msb 3 -> kind 3 (msb 8, lsb)
#   variation 0x372146 (byte 0xAB) -> kind 4; insertion 0x372180 (byte 0xAF) -> kind 2
def pairs(a, n):
    b = rb(a, 2 * n)
    return [(b[2 * i], b[2 * i + 1]) for i in range(n)]


REV_T, VAR_T, INS_T = pairs(0x372124, 17), pairs(0x372146, 29), pairs(0x372180, 41)

# FUN_0039B09C: per kind, the preset block (step words, then slot words) and the step/slot lists
BLK = {0: (0x128, {0: (0x363600, 0x364F10)}),
       2: (0x130, {0: (0x35ECB4, 0x3619C2), 1: (0x35F634, 0x3619D4), 2: (0x3619FC, 0x3635DC), 3: (0x363AA0, 0x364F1D),
                   4: (0x361D8C, 0x3635DF), 5: (0x36211C, 0x3635E7), 6: (0x363E30, 0x364F25), 7: (0x35FD54, 0x3619DA),
                   10: (0x3642F0, 0x364F32), 12: (0x364550, 0x364F34), 13: (0x3647B0, 0x364F37)}),
       3: (200, {0: (0x360474, 0x3619E1), 8: (0x360D0C, 0x3619F5)}),
       4: (200, {0: (0x360474, 0x3619E1), 1: (0x36085C, 0x3619EF), 2: (0x36283C, 0x3635EF), 3: (0x3629CC, 0x3635F3),
                 4: (0x362F44, 0x3635F7), 5: (0x362CEC, 0x3635FB), 6: (0x360B7C, 0x3619F3), 7: (0x3656E0, 0x3657A8),
                 8: (0x360D0C, 0x3619F5), 9: (0x364B40, 0x364F3C), 10: (0x364CD0, 0x364F40)})}
LISTS = {0: (0x35E02C, 0x5E, 0x35E0E8, 0x36), 2: (0x35E17C, 0x60, 0x35E23C, 0x28),
         3: (0x35E268, 0x48, 0x35E2F8, 0xC), 4: (0x35E308, 0x48, 0x35E398, 0xC)}
PROG = {0: "base", 2: "variation", 3: "ins1", 4: "ins2"}    # docs/vop3_disasm file stem per FUN_00039C8A kind


def preset_block(kind, msb, lsb):
    stride, m = BLK[kind]
    base, idx = m.get(msb, m[0])
    i = rom[idx - BASE + (msb if kind == 0 else lsb)]
    a = base + ((i * 200) & 0xFF if (kind, msb) == (4, 7) else i * stride)   # Ghidra's & 0xff on 4/7 is the firmware's
    sa, sn, la, ln = LISTS[kind]
    w = struct.unpack(">%dh" % (sn + ln), rb(a, 2 * (sn + ln)))
    return a, dict(zip(u16s(sa, sn), [x & 0xFFFF for x in w[:sn]])), dict(zip(rb(la, ln), w[sn:]))


# ------------------------------------------------------------------------------------------ the filter designer
# FUN_00037414(order, scale, kind, freq, gain, q, steps) designs one section in soft float and writes its words
# to DAT_0106842C[steps[i]]. Read off a grid of 3520 emulated calls (--designer): bilinear transform at
# fs = 44100 with K = tan(pi f / fs) from the float table at 0x371614 (index 0..60 = 20 Hz .. 20 kHz on the
# 1/6-octave series), gain V = float table 0x371708[gain - 52] (10^((gain-64)/20), -12..+12 dB), Q = q / 10.
# Shelves and the peak use the boost prototype and swap numerator and denominator for a cut. Words are the
# coefficients times 2^(15 - scale), rounded; order 1 writes b0, b1, -a1 and order 2 b0, b1, b2, -a1, -a2.
# This matches the firmware to 1 LSB on 3430 of 3432 grid calls and 2 LSB on the rest; the last bit depends on its float operation order, so the emulated
# words in params.json are the exact ones.
TAN_TAB, GAIN_TAB = 0x371614, 0x371708
SWEEP_V, SWEEP_HI = list(range(128)), [200, 255, 256, 511, 1000, 1023, 2000, 4095, 6820, 8191, 13650, 16383]
CURVES = []
OUT_CURVES = ROOT / "docs/vop3_2/curves.json"
DESIGN_KINDS = {0: "1st-order LPF (gain)", 1: "1st-order HPF (gain)", 2: "1st-order low shelf", 3: "1st-order high shelf",
                4: "LPF", 5: "HPF", 6: "low shelf (Q fixed)", 7: "high shelf (Q fixed)", 8: "band pass", 9: "notch", 10: "peak"}


def design(order, scale, kind, freq, gain, q):
    """Words FUN_00037414 writes, to within 1 LSB; None for kind 11 (its case 0xb does no arithmetic)."""
    if kind not in DESIGN_KINDS:
        return None
    K = struct.unpack(">f", rb(TAN_TAB + 4 * freq, 4))[0]
    V = struct.unpack(">f", rb(GAIN_TAB + 4 * (gain - 52), 4))[0]    # every kind scales by it, plain filters too
    Q = q / 10 if q else 1.0     # first-order kinds are called with q = 0 and never use it
    cut = kind in (2, 3, 6, 7, 10) and V < 1
    W = 1 / V if cut else V
    if kind <= 3:      # s-polynomials (s, 1) over (s + 1)
        nb, na = {0: (0, W), 1: (W, 0), 2: (1, W), 3: (W, 1)}[kind], (1, 1)
        if cut:
            nb, na = na, nb
        bil = lambda c: (c[0] + c[1] * K, c[1] * K - c[0], 0)
    else:              # (s^2, s, 1)
        r = math.sqrt(W)
        nb, na = {4: ((0, 0, W), (1, 1 / Q, 1)), 5: ((W, 0, 0), (1, 1 / Q, 1)), 6: ((1, 2 * r, W), (1, 2, 1)),
                  7: ((W, 2 * r, 1), (1, 2, 1)), 8: ((0, 1 / Q, 0), (1, 1 / Q, 1)), 9: ((1, 0, 1), (1, 1 / Q, 1)),
                  10: ((1, W / Q, 1), (1, 1 / Q, 1))}[kind]
        if cut:
            nb, na = na, nb
        bil = lambda c: (c[2] * K * K + c[1] * K + c[0], 2 * c[2] * K * K - 2 * c[0], c[2] * K * K - c[1] * K + c[0])
    b, a = bil(nb), bil(na)
    b = [x / a[0] for x in b]
    a = [-a[1] / a[0], -a[2] / a[0]]
    xs = [b[0], b[1], a[0]] if order == 1 else [b[0], b[1], b[2], a[0], a[1]]
    return [int(math.floor(x * 2 ** (15 - scale) + 0.5)) & 0xFFFF for x in xs]


# FUN_0039C796(time, damp, 6, sel, list 0x35DB6C), reverb types 1-8 (FUN_003A3728 p0/p13): six combs, list
# entries (g, x, x, a) each, then the damping LPF's b0, b1, -a1 (designed by FUN_00037414 order 1 scale 1 kind 0
# at freq s8@0x36FE12[damp]). Per comb i: c = u8@0x36FE1D[sel*6 + i], sel = s16@0x36F826[type*3];
# j = clamp(u8@0x36F9CC[time] + c - 0x38, 0, 255); g = u16@0x36FA14[j*4]; x = trunc(u16@0x36FA12[j*4] * b0 / 32768)
# in single float; the comb's last word copies the LPF's -a1. c = 0 zeroes the comb.
def reverb_combs(rtype, time, damp):
    sel = struct.unpack(">h", rb(0x36F826 + rtype * 6, 2))[0]
    fr = struct.unpack("b", rb(0x36FE12 + damp, 1))[0]
    b0, b1, a1 = design(1, 1, 0, fr, 64, 0)
    lst = u16s(0x35DB6C, 24)
    out = {lst[21]: b0, lst[22]: b1, lst[23]: a1}
    f32 = lambda v: struct.unpack(">f", struct.pack(">f", v))[0]
    for i in range(6):
        c = rb(0x36FE1D + sel * 6 + i, 1)[0]
        g = k = 0
        if c:
            j = max(0, min(255, rb(0x36F9CC + time, 1)[0] + c - 0x38))
            g, k = u16s(0x36FA14 + 4 * j, 1)[0], u16s(0x36FA12 + 4 * j, 1)[0]
        kk, bb = (k - 65536 if k >= 32768 else k), (b0 - 65536 if b0 >= 32768 else b0)
        x = int(f32(f32(kk) * f32(bb)) / 32768) & 0xFFFF
        out.update({lst[4 * i]: g, lst[4 * i + 1]: x, lst[4 * i + 2]: x, lst[4 * i + 3]: a1})
    return out


# ------------------------------------------------------------------------------------------ Data List
def datalist():
    txt = (ROOT / "docs/FS1R_DataList_text.txt").read_text(encoding="utf-8", errors="replace").replace("\r", "").split("\n")
    heads = [i for i, l in enumerate(txt) if l.startswith("Range(Default) Param#")]
    out = []
    for i in heads:
        name = txt[i - 1].split(" Same parameters")[0].strip()
        ps = OrderedDict()
        j = i + 1
        while j < len(txt) and not txt[j + 1].startswith("Range(Default)") and not txt[j].startswith("Datalist/") \
                and txt[j].strip() not in ("VARIATION", "INSERTION"):
            m = re.search(r"\)\s+([0-9A-F]{2,3})\s", txt[j])
            if m:
                head = txt[j][:m.start() + 1]
                nm = []
                for t in head.split(" "):
                    if t.startswith("*") or "~" in t or "(" in t or (re.match(r"^[-+]?\d", t) and nm):
                        break
                    nm.append(t)
                dflt = re.search(r"\(([^()]*)\)\s*$", head)
                ps[int(m.group(1), 16)] = (" ".join(nm), dflt.group(1).strip() if dflt else "")
            j += 1
        out.append((name, ps))
    return out[0:17], out[17:46], out[46:87]


def param_index(group, addr):
    """Data List Param# -> index into the firmware's 16-word parameter block."""
    if group == 0:
        return (addr - 0x50) // 2 if addr <= 0x60 else addr - 0x61 + 9
    lin = lambda a: (a >> 8) * 128 + (a & 0xFF)
    return (lin(addr) - lin(0x68 if group == 1 else 0x108)) // 2


# ------------------------------------------------------------------------------------------ emulation
def emulate(work, workers):
    jobs = [("rev%02d" % t, 0, b, 0) if a == 0 else ("rev%02d" % t, 3, 8, b) for t, (a, b) in enumerate(REV_T)]
    jobs += [("var%02d" % t, 4, a, b) for t, (a, b) in enumerate(VAR_T)]
    jobs += [("ins%02d" % t, 2, a, b) for t, (a, b) in enumerate(INS_T)]
    jobs += [("eq", 5, 0, 0), ("sys", 6, 0, 0)]
    work.mkdir(parents=True, exist_ok=True)
    for f in work.glob("emu_*.jsonl"):
        f.unlink()
    (work / "scripts").mkdir(exist_ok=True)
    (work / "scripts/Vop3Params.java").write_text(JAVA)
    procs = []
    for k in range(workers):
        proj = work / ("proj%d" % k)
        shutil.rmtree(proj, ignore_errors=True)
        shutil.copytree(PROJ, proj)
        prp = proj / "FS1R.rep/project.prp"     # the project is owned by the host user; headless refuses otherwise
        prp.write_text(re.sub(r'(NAME="OWNER" TYPE="string" VALUE=")[^"]*', r"\g<1>" + getpass.getuser(), prp.read_text()))
        jf = work / ("jobs_%d.txt" % k)
        jf.write_text("".join("%s %d %d %d\n" % j for j in jobs[k::workers]))
        log = open(work / ("emu_%d.log" % k), "w")
        procs.append(subprocess.Popen(["analyzeHeadless", str(proj), "FS1R", "-process", "-noanalysis", "-readOnly",
                                       "-scriptPath", str(work / "scripts"), "-postScript", "Vop3Params.java",
                                       str(jf), str(work / ("emu_%d.jsonl" % k))], stdout=log, stderr=subprocess.STDOUT))
    for p in procs:
        p.wait()
    for k in range(workers):
        shutil.rmtree(work / ("proj%d" % k), ignore_errors=True)
    return len(jobs)


def load(work):
    E = {}
    for f in sorted(glob.glob(str(work / "emu_*.jsonl"))):
        for line in open(f):
            r = json.loads(line)
            E[r["tag"]] = r
    return E


# ------------------------------------------------------------------------------------------ formulas
def tab(A, v, et):
    if et == "u8":
        return rom[A - BASE + v]
    return struct.unpack(">h" if et == "s16" else ">H", rb(A + 2 * v, 2))[0]


PRE = OrderedDict([("v", lambda v: v), ("(v*0x4CCC>>12)", lambda v: (v * 0x4CCC) >> 12)])
POST = OrderedDict([("{}", lambda x: x), ("({}>>1)", lambda x: x >> 1), ("({}>>2)", lambda x: x >> 2), ("({}>>3)", lambda x: x >> 3),
                    ("({}<<1)", lambda x: x << 1), ("({}*0x4CCC>>12)", lambda x: (x * 0x4CCC) >> 12),
                    ("({}*0x7AE1>>16)", lambda x: (x * 0x7AE1) >> 16)])
IDX = OrderedDict([("v", lambda v: v), ("128-v", lambda v: 128 - v), ("127-v", lambda v: 127 - v)])


def fit(xs, ys, M):
    """y = a*x + b (mod M), a and b integers, or None."""
    pts = list(zip(xs, ys))
    xa, ya = pts[0]
    xb = next((x for x, _ in pts if x != xa), None)
    if xb is None:
        return None
    yb = next(y for x, y in pts if x == xb)
    dy = yb - ya
    if M:
        dy = (dy + M // 2) % M - M // 2
    if dy % (xb - xa):
        return None
    a = dy // (xb - xa)
    b = ya - a * xa
    if all((a * x + b - y) % M == 0 if M else a * x + b == y for x, y in pts):
        return a, (b % M if M else b)
    return None


def term(a, s):
    return s if a == 1 else "-" + s if a == -1 else "%d*%s" % (a, s)


def classify(vs, ys, tables, mod16):
    """A formula for ys over parameter values vs, from the handler's own helpers and EPROM tables."""
    M = 0x10000 if mod16 else 0
    hx = (lambda b: "0x%04X" % b) if mod16 else str
    if len(set(ys)) == 1:
        return "same for every value (set by other parameters)"
    for pn, pf in PRE.items():
        r = fit([pf(v) for v in vs], ys, M)
        if r:
            return "%s + %s" % (term(r[0], pn), hx(r[1]))
    for A in tables:
        for et in ("u16", "s16", "u8"):
            for iname, ix in IDX.items():
                try:
                    xs0 = [tab(A, ix(v), et) for v in vs]
                except (struct.error, IndexError):
                    continue
                for qn, qf in POST.items():
                    r = fit([qf(x) for x in xs0], ys, M)
                    if r:
                        s = term(r[0], qn.format("%s@0x%06X[%s]" % (et, A, iname)))
                        return s if r[1] == 0 else "%s + %s" % (s, hx(r[1]))
    return None


def eprom_tables(decomp, funcs):
    t = set()
    for f in funcs:
        for m in re.findall(r"_00(3[5-7][0-9a-f]{4})\b", decomp.get(f, "")):
            t.add(int(m, 16))
    return sorted(t)


def ranges(xs, w=3):
    xs = sorted(xs)
    out, i = [], 0
    while i < len(xs):
        j = i
        while j + 1 < len(xs) and xs[j + 1] == xs[j] + 1:
            j += 1
        out.append(("%0*x" % (w, xs[i])) if i == j else ("%0*x-%0*x" % (w, xs[i], w, xs[j])))
        i = j + 1
    return ", ".join(out)


# ------------------------------------------------------------------------------------------ report
KIND_NAME = {0: "reverb (types 1-12)", 2: "insertion", 3: "reverb (types 13-16)", 4: "variation", 5: "master EQ", 6: "system"}
DISP = {0: 0x39C144, 2: 0x39C1AC, 3: 0x39C48A, 4: 0x39C51C, 5: 0x3A356C, 6: 0x3A2EC}
LOWLEVEL = {0x39652, 0x396A6, 0x396FC, 0x39764, 0x397F4, 0x39390, 0x3943C, 0x394BA, 0x39B24, 0x39C8A, 0xAD1E, 0xAD3E}
SYS_SRC = {0: "DAT_01068DBC (perf 0xA9 or 0x40)", 1: "DAT_01068DBE (perf 0xAA or 0)", 3: "DAT_01068DC2", 4: "DAT_01068DC4 (0x5A)",
           5: "DAT_01068DC6 (perf 0xB0)", 6: "DAT_01068DC8 (perf 0xAD)", 8: "DAT_01068DCC (perf 0xAE)", 10: "DAT_01068DD0",
           11: "DAT_01068DD2 (perf 0xAC)", 14: "DAT_01068DD8"}


def class3_steps():
    out = {}
    for f in sorted(DISASM.glob("fs1r_vop3_2_*.txt")):
        stem = f.stem.replace("fs1r_vop3_2_", "")
        for line in f.read_text().splitlines():
            if "class=3" in line:
                out.setdefault(stem, []).append((int(line.split()[0], 16), " ".join(line.split()[1:])))
    return out


def build(E, curves_path=None):
    rv, va, ins = datalist()
    con = sqlite3.connect("file:%s?mode=ro" % DB, uri=True)
    decomp = {a: s or "" for a, s in con.execute("select address, raw_decomp from decompilations")}
    dset = {k: {int(x[4:], 16) for x in re.findall(r"FUN_[0-9a-f]{8}", decomp.get(a, ""))} - {a} for k, a in DISP.items()}

    groups = [("reverb", "rev", REV_T, rv, 0), ("variation", "var", VAR_T, va, 1), ("insertion", "ins", INS_T, ins, 2)]
    pj = OrderedDict()
    md_types = []
    users = {}            # (program stem, step) -> [(group, type no, type name, param label)]
    checks = 0
    checks_d = [0, 0]
    checks_c = [0, 0]
    curve_ids = {}

    def curve_id(ys):
        """Index of this value table in CURVES (values for SWEEP_V, or SWEEP_V + SWEEP_HI for 14-bit delay words)."""
        return curve_ids.setdefault(tuple(ys), len(curve_ids))
    curves = {}
    for gname, pref, TT, dl, gi in groups + [("master_eq", "eq", [(0, 0)], [("Master EQ", {})], None),
                                             ("system", "sys", [(0, 0)], [("System", {})], None)]:
        pj[gname] = OrderedDict()
        for t in range(len(TT)):
            tag = "%s%02d" % (pref, t) if gi is not None else pref
            r = E[tag]
            kind = r["kind"]
            name, dps = dl[t]
            prog = None
            if kind in PROG:
                prog = PROG[kind] if kind == 0 else "%s_%02d" % (PROG[kind], r["msb"])
            pnames = {param_index(gi, a): (a, n, d) for a, (n, d) in dps.items()} if gi is not None else {}
            # preset block check: everything the handlers leave alone is the EPROM block word
            blk = None
            if kind in BLK:
                blk, bc, bo = preset_block(kind, r["msb"], r["lsb"])
                hw_c = set(r["all_c"]).union(*[p["wc"] for p in r["per"]])
                hw_o = set(r["all_o"]).union(*[p["wo"] for p in r["per"]])
                bad = [s for s in bc if s not in hw_c and r["const"][s] != bc[s]] + \
                      [0x1000 + s for s in bo if s not in hw_o and r["offs"][s] != bo[s]]
                assert not bad, "%s: preset block 0x%06X disagrees with the emulation at %s" % (tag, blk, bad)
                checks += 1
                steps, slots = sorted(bc), sorted(bo)
            else:
                steps = sorted(set(r["all_c"]).union(*[p["wc"] for p in r["per"]]))
                slots = sorted(set(r["all_o"]).union(*[p["wo"] for p in r["per"]]))
            handler = None
            if kind in dset:
                hs = set().union(*[p["trace"] for p in r["per"]])
                handler = next((h for h in sorted(hs) if h in dset[kind]), DISP[kind] if kind in (5, 6) else None)
            entry = OrderedDict(name=name, fw_kind=kind, fw_type=[r["msb"], r["lsb"]], program=prog,
                                handler=("FUN_%08X" % handler) if handler else None,
                                preset_block=("0x%06X" % blk) if blk else None, params=r["params"],
                                const=OrderedDict(("%03x" % s, r["const"][s]) for s in steps),
                                offs=OrderedDict(("%02x" % s, r["offs"][s]) for s in slots))
            pj[gname][str(t)] = entry
            rows = []
            entry["map"] = []
            for p in r["per"]:
                if not (p["wc"] or p["wo"]):
                    continue
                sw = p["sweep"]
                vs = sorted(int(k) for k in sw)
                v7 = [v for v in vs if v < 128]
                helpers = [h for h in p["trace"] if h not in LOWLEVEL and h != handler and h not in DISP.values()
                           and not 0x200000 <= h < 0x206000]
                softf = any(0x203000 <= h < 0x206000 for h in p["trace"]) or 0x37414 in p["trace"]
                tabs = eprom_tables(decomp, [h for h in p["trace"] if h >= 0x39000])
                forms = OrderedDict()
                dstep = {}
                for d in p.get("designs", []):
                    o, sc, k, fr, gn, q, lst = d
                    pred = design(o, sc, k, fr, gn, q)
                    if pred is None:
                        continue
                    for i, st in enumerate(lst[:len(pred)]):
                        got = r["const"][st]
                        ok = min((pred[i] - got) & 0xFFFF, (got - pred[i]) & 0xFFFF) <= 2
                        checks_d[0 if ok else 1] += 1
                        # only the reverb damping LPF is rescaled after the call (FUN_0039C796); anything else is a broken design()
                        assert ok or (o, sc, k) == (1, 1, 0), "%s p%d step %03x: design%s = %04x, firmware %04x" % (tag, p["p"], st, tuple(d[:6]), pred[i], got)
                        dstep[st] = "FUN_00037414 %s, order %d, scale %d (freq %d, gain %d, q %d)%s" % (
                            DESIGN_KINDS.get(k, k), o, sc, fr, gn, q, "" if ok else ", then rescaled by the handler")
                # other-channel copies (FUN_00039390 and friends): a step whose word equals a designed step's for every swept value
                col = {s: tuple(sw[str(v)][0][j] for v in v7) for j, s in enumerate(p["wc"])}
                for st in p["wc"]:
                    if st not in dstep:
                        src = next((d for d in sorted(dstep) if col.get(d) == col[st] and not dstep[d].startswith("same as")), None)
                        if src is not None:
                            dstep[st] = "same as step %03x (other channel)" % src
                if kind == 0 and 1 <= r["msb"] <= 8 and p["p"] in (0, 13):
                    for v in v7:
                        if p["p"] == 0 and v > 69 or p["p"] == 13 and not 1 <= v <= 10:
                            continue     # time beyond the 70-entry table, damp outside 0.1..1.0
                        pred = reverb_combs(t, v, r["params"][13]) if p["p"] == 0 else reverb_combs(t, r["params"][0], v)
                        for j, st in enumerate(p["wc"]):
                            if st in pred:
                                checks_c[0] += 1
                                checks_c[1] += sw[str(v)][0][j] != pred[st]
                    for st in reverb_combs(t, r["params"][0], r["params"][13]):
                        if st in p["wc"]:
                            dstep[st] = "reverb_combs(type, time, damp): FUN_0039C796, see the comb section"
                for j, s in enumerate(p["wc"]):
                    ys = [sw[str(v)][0][j] for v in v7]
                    f = (dstep[s] if dstep.get(s, "").startswith("reverb_combs") else None) or classify(v7, ys, tabs, True) or dstep.get(s) or ("computed in soft float%s" % (" (FUN_00037414)" if 0x37414 in p["trace"] else "") if softf else "computed")
                    forms.setdefault(("c", f), []).append(s)
                    if curves_path:
                        curves["%s/p%d/step%03x" % (tag, p["p"], s)] = {str(v): sw[str(v)][0][j] for v in vs}
                for j, s in enumerate(p["wo"]):
                    ys = [sw[str(v)][1][j] for v in vs]
                    f = classify(vs, ys, tabs, False) or ("computed in soft float" if softf else "computed")
                    forms.setdefault(("o", f), []).append(s)
                    if curves_path:
                        curves["%s/p%d/slot%02x" % (tag, p["p"], s)] = {str(v): sw[str(v)][1][j] for v in vs}
                pm = OrderedDict(p=p["p"], steps=OrderedDict(), slots=OrderedDict())
                for j, st in enumerate(p["wc"]):
                    pm["steps"]["%03x" % st] = curve_id([sw[str(v)][0][j] for v in vs])
                for j, st in enumerate(p["wo"]):
                    pm["slots"]["%02x" % st] = curve_id([sw[str(v)][1][j] for v in vs])
                entry["map"].append(pm)
                if gi is not None:
                    a, pn, d = pnames.get(p["p"], (None, "(not in the Data List)", ""))
                    label = ("%X %s" % (a, pn)) if a is not None else "p%d %s" % (p["p"], pn)
                elif gname == "system":
                    a, d, label = None, "", "p%d %s" % (p["p"], SYS_SRC.get(p["p"], ""))
                else:
                    a, d, label = None, "", "p%d" % p["p"]
                rows.append((p["p"], label, d, r["params"][p["p"]], helpers, forms))
                if prog:
                    lo = {"base": 0, "variation": 0x1A0, "ins1": 0x110, "ins2": 0x158}[prog.split("_")[0]]
                    for s in p["wc"]:
                        users.setdefault((prog, s), []).append((gname, t, name, label))
            md_types.append((gname, t, name, entry, rows))
    con.close()
    if curves_path:
        Path(curves_path).write_text(json.dumps(curves))
    print("designer: %d words equal design() within 2 LSB, %d rescaled after the call" % tuple(checks_d))
    print("reverb combs: %d words, %d off by 1 LSB (single-float rounding)" % tuple(checks_c))
    assert checks_c[0] and checks_c[1] * 50 < checks_c[0], "reverb_combs() no longer matches FUN_0039C796"
    CURVES[:] = [list(c) for c in sorted(curve_ids, key=curve_ids.get)]
    return pj, md_types, users, checks


def write_md(pj, md_types, users, checks):
    L = []
    w = L.append
    w("# VOP3-2 effect parameters: steps, constants and delay offsets")
    w("")
    w("Generated by `tools/extract_vop3_2_params.py` from the FS1R v1.20 firmware. Machine-readable defaults are in `docs/vop3_2/params.json`. The microcode these constants feed is in `docs/vop3_2_microcode.md` and the listings in `docs/vop3_disasm/fs1r_vop3_2_*.txt`.")
    w("")
    w("## How this was read")
    w("")
    w("The effect parameter handlers compute their constants with the EPROM's soft-float library (`FUN_00037414` and the `FUN_002030b4` family), so instead of transcribing about fifty handlers the generator runs the firmware itself in Ghidra's p-code emulator: `FUN_0003AAC8` (the effect init) once, then for every effect type `FUN_0039B09C(kind, 0xFF)`, which loads the type's preset block and runs every parameter handler. `DAT_0106842C` (u16 per step, register 0xB) and `DAT_0106882C` (s32 per slot, registers 0xD/0xE) after that call are the type's default constants and offsets. Each parameter is then called alone on marker-filled arrays to find the steps and slots it writes, with every function it enters recorded, and its value swept over 0..127 (plus 12 points up to 16383 for the 14-bit delay words) to get the word written for each value. Ghidra's SuperH semantics get `mov.l Rn,@-Rn` and `mov.l @Rn+,Rn` wrong (they use the updated register), and the firmware pushes every soft-float result pointer with `mov.l r15,@-r15`, so the emulator patches those instructions after each step; without it every designer coefficient reads 0.")
    w("")
    w("Ghidra 12.1's SuperH p-code gets `mov.x Rn,@-Rn` and `mov.x @Rn+,Rn` wrong (it stores the decremented Rn, and keeps the incremented Rn over the loaded value), and the soft-float calls depend on exactly that form: each one pushes its result pointer with `mov.l r15,@-r15` in the `jsr` delay slot. Uncorrected, every soft-float result lands one word off and every filter coefficient comes out as zero or garbage. The generator scans both ROMs for those encodings and patches the store or the load after each one executes; the SH-2 behaviour it restores was checked instruction by instruction against the emulator, along with the shifts, rotates, carries, `div1` and the multiplies the library uses, and the library's double add, subtract, multiply, divide and float conversions return correct IEEE results under it.")
    w("")
    w("Every number below is the firmware's output, not a fit. The formula column is a closed form only where one reproduces every swept value exactly from the handler's own fixed-point helpers or an EPROM table the handler reads; otherwise it says computed, and `--curves` dumps the full value-by-value table. Self-check: for all %d types with a preset block, every step and slot no handler writes holds the word of the block as read straight out of the EPROM, which the generator asserts." % checks)
    w("")
    w("## Dispatch")
    w("")
    w("`FUN_0039B09C(kind, p)` is the entry for every effect parameter change (callers `FUN_0003AC88` on a performance load, `FUN_0003AAC8` at init, the sysex and panel paths). `p` is the index into the kind's 16-word parameter block; `p >= 0xFC` reloads the type's preset block first and then runs every handler; `p >= 0xFE` also resets the window through `FUN_003A3308`. It hands `p` to a per-kind dispatcher, which picks the handler by the type bytes.")
    w("")
    w("The kind numbers are `FUN_00039C8A`'s, and the listings in `docs/vop3_disasm` are named after them, which is not the user-facing block: `FUN_0003AC88` sends the performance's insertion type (byte 0xAF, table 0x372180) to kind 2 and its variation type (byte 0xAB, table 0x372146) to kind 4. So `fs1r_vop3_2_variation_NN` is the insertion program and `fs1r_vop3_2_ins2_NN` the variation program. The reverb type (byte 0xA8, table 0x372124) goes to kind 0 for types 1-12, which run on the 0xE8-step base program, and to kind 3 with type (8, n) for the four delay types 13-16, which run on `ins1_08`. Kind 1 (`reverb_NN`, the window at 0x0E8) is only ever uploaded with type 0 and has no handler (`FUN_0039C18C` returns).")
    w("")
    w("| kind | block | type bytes | params (u16 x n) | dispatcher | preset blocks | step list | slot list | program |")
    w("|---|---|---|---|---|---|---|---|---|")
    w("| 0 | reverb 1-12 | DAT_0106840D | DAT_01068CEC x16 | FUN_0039C144 | 0x363600 + 0x128 * u8@0x364F10[type] | 0x35E02C (0x5E) | 0x35E0E8 (0x36) | base, 0x000-0x0E7 |")
    w("| 2 | insertion | DAT_01068410/11 | DAT_01068D2C x16 | FUN_0039C1AC | per msb, stride 0x130 | 0x35E17C (0x60) | 0x35E23C (0x28) | `variation_NN`, 0x1A0 |")
    w("| 3 | reverb 13-16 | DAT_01068412/13 | DAT_01068D4C x16 | FUN_0039C48A | per msb, stride 200 | 0x35E268 (0x48) | 0x35E2F8 (0x0C) | `ins1_NN`, 0x110 |")
    w("| 4 | variation | DAT_01068414/15 | DAT_01068D6C x16 | FUN_0039C51C | per msb, stride 200 | 0x35E308 (0x48) | 0x35E398 (0x0C) | `ins2_NN`, 0x158 |")
    w("| 5 | master EQ | none | DAT_01068D8C x20 | FUN_003A356C | none | | | base |")
    w("| 6 | system (returns, pans, sends) | none | DAT_01068DBC x15 | FUN_0003A2EC | 0x364E60 for 0x58 steps of 0x35E40C | | | base |")
    w("")
    w("Register writes: `FUN_00039652(n, list)` writes register 0xB for each listed step from `DAT_0106842C[step]` (`FUN_000396A6` is the same without the per-step address write, used right after an upload). `FUN_000397F4(n, slots)` writes `(DAT_0106882C[slot] + base) >> 9 & 0x1FF` to register 0xD and `(... ) & 0x1FF` to 0xE, where base is `DAT_010683F8/FC/400/404/408` picked by the byte at 0x3720A4[slot] (0..4; `FUN_0003AAC8` sets them to 0, 0xFFFF, 0x10000, 0x20000, 0x30000), so the offsets in this document are before that base. Helpers that recur in the handlers: `FUN_0039CF62(v) = (v - 64) * 500`, `FUN_0039CF72(v) = (v - 64) * 376`, `FUN_0039D47C(v) = v * 0x4CCC >> 12` (0.1 ms word to samples at 48 kHz), `FUN_0039D490(v) = v * 0x7AE1 >> 16`, `FUN_0039D468(x, v) = max(1, x - u8@0x37006A[v])`, `FUN_0039D500/D59C/D4A2(x) = x * u8 table[type] >> 6`, `FUN_003A3070(v, n)` writes `u16@0x36EF54[v]` into `DAT_01068A2C[n]` (register 0x24, not a step constant), `FUN_0000AD1E` (= `FUN_00037414`) the soft-float filter designer (EQ, HPF, LPF, damping).")
    w("")
    w("## The filter designer (FUN_00037414)")
    w("")
    w("`FUN_00037414(order, scale, kind, freq, gain, q, steps)` (reached as `FUN_0000AD1E`) designs every EQ, HPF, LPF, damping and wah section in soft float and writes its words into `DAT_0106842C` at the listed steps. Read off 3432 emulated calls on a grid of all eleven kinds, both orders and scales 0/1/2/4, and checked in every generator run against each call the handlers make (the generator asserts the match):")
    w("")
    w("* Bilinear transform at fs = 44100 Hz: `K = tan(pi f / 44100)`, taken from the float table at 0x371614 indexed by `freq` (0..60 = 20 Hz to 20 kHz on the 1/6-octave series 20, 22, 25, 28, 32, 36, 40, 45, 50, 56, 63, 70, 80 ...). The EQ and filter frequency bytes are this index directly.")
    w("* `V` = float table 0x371708[gain - 52], which is `10^((gain - 64)/20)` (gain byte 52..76 = -12..+12 dB). `Q = q / 10`.")
    w("* Analog prototypes in s (normalized to the corner), boost form; for a cut (V < 1) the shelves and the peak use `1/V` and swap numerator and denominator:")
    w("  * kind 0 first-order LPF `V / (s + 1)`; 1 first-order HPF `V s / (s + 1)`; 2 first-order low shelf `(s + V) / (s + 1)`; 3 first-order high shelf `(V s + 1) / (s + 1)`")
    w("  * 4 LPF `V / (s^2 + s/Q + 1)`; 5 HPF `V s^2 / (s^2 + s/Q + 1)`; 8 band pass `(s/Q) / (s^2 + s/Q + 1)`; 9 notch `(s^2 + 1) / (s^2 + s/Q + 1)`; 10 peak `(s^2 + V s/Q + 1) / (s^2 + s/Q + 1)`")
    w("  * 6 low shelf `(s^2 + 2 sqrt(V) s + V) / (s + 1)^2`; 7 high shelf `(V s^2 + 2 sqrt(V) s + 1) / (s + 1)^2` (Q not used)")
    w("* Words are `round(c * 2^(15 - scale))` as 16-bit two's complement, normalized by `a0`. Order 1 writes `b0, b1, -a1`, order 2 writes `b0, b1, b2, -a1, -a2`, in the order of the step list. The scale is the section's headroom shift: scale 1 means the step runs the coefficient at half size.")
    w("* Accuracy: 3430 of 3432 grid words exact or 1 LSB off, 2 at 2 LSB; the last bit depends on the firmware's float operation order, so `params.json` keeps the emulated words.")
    w("")
    w("## Reverb combs (FUN_0039C796, reverb types 1-8)")
    w("")
    w("Reverb Time (p0) and High Damp (p13) on Hall1..Plate go through `FUN_0039C796(time, damp, 6, sel, list 0x35DB6C)` with `sel = s16@0x36F826[type*3]`. It designs the damping LPF once (`FUN_00037414` order 1, scale 1, kind 0 at freq `s8@0x36FE12[damp]`, words b0, b1, -a1 to steps d2, d3, d4) and fills six combs of four steps each (list order: g, x, x, a):")
    w("")
    w("* `c = u8@0x36FE1D[sel*6 + i]`; c = 0 zeroes the comb.")
    w("* `j = clamp(u8@0x36F9CC[time] + c - 0x38, 0, 255)`. 0x36F9CC maps the time index (0..69, seconds in the float table 0x36F8B0) to a decay row; c is the comb's length class.")
    w("* `g = u16@0x36FA14[4j]` (feedback gain), `x = trunc(float(u16@0x36FA12[4j]) * float(b0) / 32768)` (the comb's loss-filter gain), `a` = the LPF's -a1.")
    w("")
    w("`reverb_combs()` in the generator implements this and is checked against every swept value of p0 and p13 on all eight types each run; the only misses are 1-LSB single-float rounding. Types 9-12 (White Room..Canyon) use `FUN_0039CF82` with room-size tables (0x37054E, 0x37061C) and are left as value tables.")
    w("")
    w("## Exact value tables")
    w("")
    w("Every word a parameter writes, for every parameter value, is in `docs/vop3_2/curves.json`: `params.json` maps each type's parameter to `{step: table index}` and `{slot: table index}` (key `map`), and `curves.json` holds the deduplicated tables, indexed by value 0..127 (`values`), or 0..127 plus 12 points to 16383 for the 14-bit delay words (`values_14bit`). A step whose word depends on another parameter reads at that parameter's default. This covers the rows below marked computed, whose closed form is not transcribed.")
    w("")
    w("Type to handler (from the dispatchers, confirmed by the emulation trace):")
    w("")
    w("| block | type | name | fw type | handler | program |")
    w("|---|---|---|---|---|---|")
    for g, t, name, e, rows in md_types:
        w("| %s | %d | %s | %d/%d | %s | %s |" % (g, t, name, e["fw_type"][0], e["fw_type"][1], e["handler"] or "none", e["program"] or ""))
    w("")
    w("## Per type")
    w("")
    w("Steps are program addresses (hex), slots are `DAT_0106882C` indices (hex). Default is the raw word in the parameter block after the preset load, with the Data List default beside it. `u16@A[v]` means the 16-bit EPROM word at A + 2v. Constants are u16 (two's complement for negative gains), offsets are signed integers before the 0x3720A4 base.")
    for g, t, name, e, rows in md_types:
        w("")
        w("### %s %d: %s" % (g, t, name))
        w("")
        w("fw kind %d type %d/%d, handler %s, program `%s`, preset block %s. Steps written by the type: %d, slots: %d." % (
            e["fw_kind"], e["fw_type"][0], e["fw_type"][1], e["handler"] or "none", e["program"] or "", e["preset_block"] or "none", len(e["const"]), len(e["offs"])))
        if not rows:
            w("")
            w("No parameter writes a step constant or a delay offset.")
            continue
        w("")
        w("| param | default | helpers | writes | formula |")
        w("|---|---|---|---|---|")
        for p, label, d, raw, helpers, forms in rows:
            first = True
            for (k, f), ss in forms.items():
                tgt = ("step " + ranges(ss)) if k == "c" else ("slot " + ranges(ss, 2))
                hs = " ".join("FUN_%08X" % h for h in helpers) or ""
                if first:
                    w("| %s | %d%s | %s | %s | %s |" % (label, raw, (" (%s)" % d) if d else "", hs, tgt, f))
                else:
                    w("| | | | %s | %s |" % (tgt, f))
                first = False
    w("")
    w("## Class-3 steps and the parameters that write them")
    w("")
    w("Every `class=3` step in the listings, with each type that runs that program and the parameter whose handler writes the step's constant. A step no handler writes keeps its preset-block word, listed in `params.json`.")
    w("")
    w("| program | step | instruction | written by |")
    w("|---|---|---|---|")
    used = {}
    for g, t, name, e, rows in md_types:
        if e["program"]:
            used.setdefault(e["program"], []).append("%s %d" % (g, t))
    # FUN_0003AC88 sets kind 3 to type 0/1 for reverbs 1-12, so ins1_00 is uploaded with no handler behind it
    used.setdefault("ins1_00", []).append("every reverb 1-12 (kind 3 type 0/1, no handler)")
    for prog, lst in class3_steps().items():
        for s, ins in lst:
            who = users.get((prog, s))
            if who:
                txt = "; ".join("%s %d %s: %s" % x for x in who)
            elif prog in used:
                txt = "no parameter (preset word only); used by " + ", ".join(used[prog])
            else:
                txt = "program not selected by any type"
            w("| %s | %03x | `%s` | %s |" % (prog, s, ins, txt))
    w("")
    w("## Not resolved")
    w("")
    w("* No closed form for the rows marked computed: White Room..Canyon early reflections (`FUN_0039CF82`, `FUN_0039D2C4`), density and diffusion (`FUN_0039C9E8`), the gate and ambience rooms (`FUN_003A6354`), distortion drive and output (`FUN_003A4F00`), edge (`FUN_000394F8`), LFO depth (`FUN_003A328E`). Their exact words are in `curves.json`.")
    w("* The system kind's parameter names are the RAM words `FUN_0003AC88` fills from the performance common bytes shown; their user-facing meaning (return, pan, send) is not mapped one by one.")
    w("* The emulation starts from `FUN_0003AAC8`'s state; a handler that reads state set elsewhere (the tempo, `DAT_010683F5`'s memory-size flag, the insertion connection) is read at its init value. `DAT_010683F5` = 0 selects the full 0x40000-word delay memory.")
    w("* Data List parameters that never reach `DAT_0106842C`/`DAT_0106882C` (LFO rates and depths, which `FUN_00039B24` and its siblings write to registers 0x24-0x27, the levels and pans the system kind owns) do not appear in the per-type tables.")
    OUT_MD.write_text("\n".join(L) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--work", default=str(Path(tempfile.gettempdir()) / "vop3_2_params"))
    ap.add_argument("--workers", type=int, default=max(1, min(8, (os.cpu_count() or 2) - 1)))
    ap.add_argument("--reuse", action="store_true", help="skip the emulation, use the jsonl already in --work")
    ap.add_argument("--curves", help="also write every swept value of every written word to this JSON file")
    a = ap.parse_args()
    work = Path(a.work)
    if not a.reuse:
        if not shutil.which("analyzeHeadless"):
            raise SystemExit("analyzeHeadless not on PATH")
        n = emulate(work, a.workers)
        print("emulated %d types" % n)
    E = load(work)
    need = 17 + 29 + 41 + 2
    if len(E) != need:
        raise SystemExit("emulation incomplete: %d of %d types (see %s/emu_*.log)" % (len(E), need, work))
    pj, md_types, users, checks = build(E, a.curves)
    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUT_JSON.write_text(json.dumps(pj, indent=1) + "\n")
    OUT_CURVES.write_text(json.dumps({"values": SWEEP_V, "values_14bit": SWEEP_V + SWEEP_HI, "tables": CURVES}, separators=(",", ":")) + "\n")
    write_md(pj, md_types, users, checks)
    print("ok: %d preset blocks checked; wrote %s and %s" % (checks, OUT_MD.relative_to(ROOT), OUT_JSON.relative_to(ROOT)))


if __name__ == "__main__":
    main()
