#!/usr/bin/env python3
"""The VOP3 external RAM on the PLG150-AN and AN1x: what the H8 uploads, what the programs read, and the AN1x
mask ROM rebuilt from it. Findings in docs/vop3_an_ram.md.

    python tools/vop3_an_ram.py            print the map, check it, write docs/vop3_an1x/maskrom.bin
    python tools/vop3_an_ram.py --probe    also run both programs through yss236_core (tools/vop3_an_ram.cpp)
    python tools/vop3_an_ram.py --check    the s40 takes against tools/vop3_interp.py's RAM rules (slow, ~10 min)
    python tools/vop3_an_ram.py --rig a|b  write docs/vop3_ship/40a|40b.probes.json (session 40, the FS1R.unlock s25 runner)
                                           and print what each hypothesis predicts for it
    python tools/vop3_an_ram.py --score <session dir>   the unit's answer: R's onset after L's, per probe

Sources, all in FS1R_DISASM/roms:
  PLG150-AN_cpuview.bin   FUN_00081358 streams 0xC242 bytes at 0xC245C into register 0x11 (address 0xF/0x10 = 0)
                          as little-endian words (the decompilation shows byte offsets, 2 x reg). Program: base 0xBB9F0, mode overlays 0xBDA12/0xBE894/0xBF716 on
                          steps 004..0E9 (FUN_0006e62c), patch lists 0xC0598/0xC0C56/0xC1314 (steps 105..162, per
                          mode) and 0xC19D2 (163..1F6) applied by FUN_0007f9e6.
  an1x_v104_cpuview.bin   voice program 0xD0638 (FUN_00056028); the H8 never writes the RAM registers (no write to
                          registers 0xF..0x12 anywhere in the AN1x decompilation): its RAM is the 1 Mbit mask ROM. A big-endian
                          copy of part of the same tables sits in the CPU ROM at 0xE496C, referenced from 0x6CD4C.
"""
import struct
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROMS = ROOT.parent / "FS1R_DISASM" / "roms"
OUT = ROOT / "docs" / "vop3_an1x"
MASKROM_WORDS = 1 << 16     # 1 Mbit = 64 Ki x 16

# Sub-tables of the PLG150-AN image, word offsets [start, end). Named by their shape (docs/vop3_an_ram.md).
TABLES = [
    (0x0000, 0x1000, "exp: -16384 * 2^(i/4096), one octave"),
    (0x1000, 0x1020, "powers of two, -1 .. -32768, each twice"),
    (0x1020, 0x1820, "decay: 32767 * 2^(-i/256) over 8 octaves"),
    (0x1820, 0x2820, "sine, one cycle, 4096 words"),
    (0x2820, 0x3820, "sine, second copy"),
    (0x3820, 0x4820, "odd periodic shape, 4 x 1024 (peak 1552)"),
    (0x4820, 0x5821, "soft clipper, +-18022, 4097 words"),
    (0x5821, 0x592A, "zero run, then 1 .. 16384 powers of two, then 32767 run"),
    (0x592A, 0x6121, "reciprocal: ~ 0x8000 * 1/(1 + i/k), 29127 .. 128"),
]


# PLG150-AN mode 0 offset -> AN1x voice offset read by the same step (tools output, docs/vop3_an_ram.md)
ROLES = {0x0000: 0x0000, 0x1010: 0x1010, 0x1020: 0x1020, 0x1C20: 0x3420, 0x2020: 0x3820, 0x5020: 0x4820}

# The AN1x mask ROM, word ranges [a, b): source image and its word offset. Everything past 0x6022 is unknown (0).
MASKROM = [
    (0x0000, 0x1000, "PLG", 0x0000, "exp; = CPU copy +0000, read at 0000"),
    (0x1000, 0x1020, "PLG", 0x1000, "powers of two, doubled; CPU copy +1000 has them single; read at 1010"),
    (0x1020, 0x3020, "CPU", 0x1010, "decay 2^(-i/1024), 4x the PLG's resolution; read at 1020 (index k 0x2000 vs 0x0800)"),
    (0x3020, 0x4020, "CPU", 0x3010, "sine; = PLG 1820; read at 3420 / 3820 (PLG 1C20 / 2020)"),
    (0x4020, 0x5021, "CPU", 0x4010, "soft clipper +-18022; = PLG 4820; read at 4820 (PLG 5020)"),
    (0x5021, 0x6022, "CPU", 0x5011, "soft clipper +-4506; next in the CPU copy, read by no step: placement inferred"),
]


def words(b, endian="<"):
    return list(struct.unpack(endian + "%dH" % (len(b) // 2), b[: len(b) // 2 * 2]))


def plg_ram():
    r = (ROMS / "PLG150-AN_cpuview.bin").read_bytes()
    return words(r[0xC245C:0xC245C + 0xC242])


def plg_program(mode):
    """Steps as {step: (tag, slot, k, r10, r9, r8, r7, r6)} after the boot sequence for one voice mode."""
    r = (ROMS / "PLG150-AN_cpuview.bin").read_bytes()

    def image(addr):
        first, n = struct.unpack_from(">HH", r, addr)
        return {first + i: struct.unpack_from(">8H", r, addr + 4 + 16 * i) for i in range(n)}

    prog = image(0xBB9F0)
    prog.update(image((0xBDA12, 0xBE894, 0xBF716)[mode]))
    base = dict(prog)
    for hdr in ((0xC0598, 0xC0C56, 0xC1314)[mode], 0xC19D2):       # FUN_0007f9e6
        first, n = struct.unpack_from(">HH", r, hdr)
        for i in range(n):
            e = struct.unpack_from(">9H", r, hdr + 4 + 18 * i)
            st, kind, w = first + i, e[0], e[1:]
            if kind == 1:
                w = tuple(a | b for a, b in zip(w, base[st]))
            if kind in (1, 2):
                prog[st] = w
    return prog


def an1x_program():
    raw = (ROMS / "an1x_v104_cpuview.bin").read_bytes()
    n = struct.unpack_from(">H", raw, 0xD0638)[0]
    rows = [struct.unpack_from(">6H", raw, 0xD063A + 12 * i) for i in range(n)]
    slots = [struct.unpack_from(">2H", raw, 0xD063A + 12 * n + 4 * i) for i in range((n + 3) // 4)]
    prog = {}
    for i, (k, r10b, r9, r8, r7, r6) in enumerate(rows):
        hi, lo = slots[i >> 2]
        prog[i] = ((r10b >> 4) & 0xF, (hi & 0xFF) << 9 | (lo & 0x1FF), k, r10b & 7, r9, r8, r7, r6)
    return prog


def offset(w):
    """RAM offset of a slot word w1: FUN_0007f87c writes w1 >> 9 to register 0xD and w1 & 0x1FF to 0xE."""
    return ((w >> 9) & 0xFF) << 9 | (w & 0x1FF)


def accesses(prog):
    """Every step with a RAM mark: (step, mem, offset, index added (w2[3]), w2[5])."""
    return [(st, w[3] & 7, offset(w[1]), (w[5] >> 3) & 1, (w[5] >> 5) & 1)
            for st, w in sorted(prog.items()) if w[3] & 7]


def table_of(off, an1x=False):
    if an1x:
        for a, b, _, _, why in MASKROM:
            if a <= off < b:
                return "%04X %s" % (a, why.split(";")[0])
        return "?"
    for a, b, name in TABLES:
        if a <= off < b:
            return "%04X %s" % (a, name.split(":")[0])
    return "past the image" if off >= 0x6121 else "?"


def main(argv):
    ram = plg_ram()
    progs = {"PLG150-AN mode %d" % m: plg_program(m) for m in range(3)}
    progs["AN1x voice"] = an1x_program()

    # 1. Every access, grouped by offset.
    offs = {}
    for name, prog in progs.items():
        acc = accesses(prog)
        writes = [a for a in acc if a[1] == 1]
        assert not writes, (name, writes)                         # no AN program writes its RAM
        assert all(a[4] for a in acc), name                       # every AN access carries w2[5]
        for st, mem, off, idx, _ in acc:
            offs.setdefault(off, {}).setdefault(name, []).append(st)
        print("%-18s %3d reads, 0 writes, offsets %s" % (name, len(acc), " ".join("%05X" % o for o in sorted({a[2] for a in acc}))))
    print("\noffset  table (AN1x-only rows: the rebuilt mask ROM's layout)  readers")
    for off in sorted(offs):
        lab = table_of(off, an1x=list(offs[off]) == ["AN1x voice"])
        print("%05X   %-22s %s" % (off, lab, "; ".join("%s x%d" % (n, len(s)) for n, s in offs[off].items())))

    # 2. The AN1x mask ROM. PLG150-AN mode 0 and the AN1x voice read six offsets each, pairwise from the same step
    # with the same neighbours (ROLES). Where the offset differs the AN1x table sits elsewhere, and the CPU ROM's
    # big-endian copy at 0xE496C holds it: the same tables in the AN1x order, minus the doubling of pow2.
    shared = {(o, ROLES[o]) for o in offs if "PLG150-AN mode 0" in offs[o] and o in ROLES}
    for plg, an in sorted(shared):
        ps, as_ = offs[plg]["PLG150-AN mode 0"], offs[an]["AN1x voice"]
        print("  PLG %05X <-> AN1x %05X  %2d / %2d readers, same steps %d" % (plg, an, len(ps), len(as_), len(set(ps) & set(as_))))
    assert sorted(ROLES.values()) == sorted(o for o in offs if "AN1x voice" in offs[o])
    an1x = (ROMS / "an1x_v104_cpuview.bin").read_bytes()
    cpu = words(an1x[0xE496C:], ">")
    rom = [0] * MASKROM_WORDS
    print("\nAN1x mask ROM (docs/vop3_an1x/maskrom.bin):")
    for a, b, src, at, why in MASKROM:
        data = ram[at:at + b - a] if src == "PLG" else cpu[at:at + b - a]
        rom[a:b] = data
        print("  %04X..%04X  %-3s %04X  %s" % (a, b - 1, src, at, why))
    for name, plg, cpu_at, n in (("exp", 0x0000, 0x0000, 0x1000), ("sine", 0x1820, 0x3010, 0x1000), ("clipper", 0x4820, 0x4010, 0x1001)):
        assert ram[plg:plg + n] == cpu[cpu_at:cpu_at + n], name           # the copy is the PLG150-AN's table, word for word
    assert cpu[0x1000:0x1010] == ram[0x1000:0x1020:2]                   # pow2: the CPU copy is the PLG's, undoubled
    assert ram[0x1020:0x1820] == cpu[0x1010:0x3010:4]                   # decay: the PLG's is every 4th AN1x word
    OUT.mkdir(exist_ok=True)
    (OUT / "maskrom.bin").write_bytes(struct.pack("<%dH" % MASKROM_WORDS, *rom))

    if "--probe" in argv:
        probe()


def probe():
    """Run each program through yss236_core with its table RAM loaded and print where the reads land, by the s40
    rules (r8[5] absolute, every phase from step 3, capture 5..11 back): tools/vop3_an_ram.cpp reads `step` lines
    on stdin and prints one line per read."""
    out = Path("/tmp/vop3_an_ram")
    src = ROOT / "tools" / "vop3_an_ram.cpp"
    hdr = ROOT / "src/fs1r/chips/yss236_core.h"
    if not out.exists() or out.stat().st_mtime < max(src.stat().st_mtime, hdr.stat().st_mtime):
        subprocess.run(["g++", "-std=c++17", "-O2", "-I", str(ROOT / "src"), str(src), "-o", str(out)], check=True)
    an1x_rom = words((OUT / "maskrom.bin").read_bytes())[:0x6022]
    for name, prog, ram in (("PLG150-AN mode 0", plg_program(0), plg_ram()), ("AN1x voice", an1x_program(), an1x_rom)):
        lines = ["ram " + " ".join("%x" % w for w in ram)]
        for st, w in sorted(prog.items()):
            tag, slot, k, r10, r9, r8, r7, r6 = w
            lines.append("step %x %x %x %x %x %x %x %x" % (st, r10, r9, r8, r7, r6, k, offset(slot) if r10 & 7 else 0))
        res = subprocess.run([str(out)], input="\n".join(lines) + "\nrun 4800\n", capture_output=True, text=True, check=True)
        reads = [tuple(int(x, 16) for x in ln.split()[1:]) for ln in res.stdout.splitlines() if ln.startswith("read")]
        inside = sum(a < len(ram) for _, a, _ in reads)
        print("%-17s %6d reads, %5.1f%% inside the table image" % (name, len(reads), 100 * inside / max(1, len(reads))))
        assert inside == len(reads), name
        spans = {}
        for st, a, _ in reads:
            off = offset(prog[st][1])
            d = (a - off + (1 << 17)) % (1 << 18) - (1 << 17)
            lo, hi = spans.get(off, (d, d))
            spans[off] = (min(lo, d), max(hi, d))
        silent = sorted({a[2] for a in accesses(prog)} - set(spans))
        print("   offsets never transferred: %s" % (" ".join("%05X" % o for o in silent) or "none"))
        for off, (lo, hi) in sorted(spans.items()):
            a1 = name.startswith("AN1x")
            print("   %05X  index %+6d .. %+6d  -> %s" % (off, lo, hi, table_of(off + lo, a1)))


# Session 40: settle r8[5] and the phase rule on the FS1R's VOP3-2, on Hall1 (rev type 1) with the s36 click.
# L = r[0d] (the input, as s36k), R = d[2c] (a free cell) which step 008, retargeted, captures from the latest RAM
# transfer 3..11 steps back. Shipped, that is step 003's read of slot 0 (offset 29998), which hears step 037's
# write to slot 0x0d (offset 28652) 1346 passes later.
S40 = Path(__file__).resolve().parents[1] / "docs" / "vop3_ship" / "40a.probes.json"
S40_CELL = 0x2C


def s40_probes(rnd="a"):
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import vop3_e2e as E
    prog = E.program([0, 0, 0, 0])

    def w(st, mem=None, b5=False, low=None):
        x = list(prog[st])
        if mem is not None:
            x[0] = (x[0] & ~7) | mem
        if b5:
            x[2] |= 0x20
        if low is not None:
            x[2] = (x[2] & ~0x7F) | low
        return x

    def swap(p, low=None):    # the slot-0 read word moved to step p, step p's word to 003
        r = list(prog[3]) if low is None else w(3, low=low)
        return {p: r, 3: list(prog[p])}
    cap = list(prog[8])
    cap[2] = (cap[2] & 0x7F) | (0x100 | S40_CELL) << 7
    base = {0x0CF: [0, 0, 0xC800, 0x70C0, 0x8680], 0x0D0: [0, 0, 0xC880, 0x71C0, 0x8000 | S40_CELL], 0x008: cap}
    k = {0x0D0: 0x7FFF}
    same = {0: 28652}
    if rnd == "a":
        rows = [
            ("ref", {}, {}, "shipped read 003 slot 0 / write 037 slot 0x0d: R = the comb line, 1346 after L"),
            ("rd5", {3: w(3, b5=True)}, {}, "read 003 with r8[5]: ring -> as ref; absolute -> a fixed word, no tail"),
            ("wr5", {0x37: w(0x37, b5=True)}, {}, "write 037 with r8[5]: ring -> as ref; absolute -> no tail"),
            ("same", {}, same, "slot 0 offset = slot 0x0d's: ring, no r8[5] -> silence (lag 2^18)"),
            ("same5", {3: w(3, b5=True), 0x37: w(0x37, b5=True)}, same, "as same, both with r8[5]: absolute -> R follows L 1 pass late"),
            ("ph0", {3: w(3, mem=0), 0: w(0, mem=2)}, {}, "the slot-0 read moved to step 000 (phase 0)"),
            ("ph1", {3: w(3, mem=0), 1: w(1, mem=2)}, {}, "the slot-0 read moved to step 001 (phase 1), control"),
            ("ph2", {3: w(3, mem=0), 2: w(2, mem=2)}, {}, "the slot-0 read moved to step 002 (phase 2)"),
            ("ph0_5", {3: w(3, mem=0), 0: w(0, mem=2, b5=True), 0x37: w(0x37, b5=True)}, same,
             "the AN shape: phase-0 read with r8[5], write with r8[5], same offset"),
        ]
    elif rnd == "c":   # 40c: the capture moves instead of the read. Shipped 008 stays (it captures d[38]).
        def capat(c, mem=None):
            x = list(prog[c]) if mem is None else w(c, mem=mem)
            x[2] = (x[2] & 0x7F) | (0x100 | S40_CELL) << 7
            return x
        rows = [("ref", {0x008: cap}, {}, "shipped, as 40a ref (capture at 008, read at 003: 5 steps)")]
        for c in (0x004, 0x005, 0x006, 0x009, 0x00A, 0x00C, 0x00D, 0x00E):
            rows.append(("cap%03x" % c, {0x008: list(prog[8]), c: capat(c)}, {},
                         "read 003, capture at %03x: %d steps after the read" % (c, c - 3)))
        for r_, c in ((0, 5), (1, 6), (2, 7)):
            rows.append(("rd%03x_cap%03x" % (r_, c), {0x008: list(prog[8]), 3: w(3, mem=0), r_: w(r_, mem=2), c: capat(c, mem=2 if c == 7 else None)}, {},
                         "read moved to %03x (phase %d), capture at %03x: 5 steps after it" % (r_, r_, c)))
        base = {a: v for a, v in base.items() if a != 0x008}
    elif rnd == "d":   # 40d: class-0 read words (the shape of Hall1's phase-1 reads 0b9..0c9), and capture distance 4
        C0 = [2, 0, 0x40, 0, 0]
        c60 = [2, 0, 0x60, 0, 0]
        def capat(c):
            x = w(c, mem=0)
            x[2] = (x[2] & 0x7F) | (0x100 | S40_CELL) << 7
            return x
        rows = [("ref", {}, {}, "shipped, as 40a ref"),
                ("c0ph3", {3: C0}, {}, "read 003 as a bare class-0 read word (2, 0, 0x40, 0, 0)")]
        for r_ in (0, 1, 2):
            rows.append(("c0ph%d" % r_, {3: w(3, mem=0), r_: C0}, {}, "bare class-0 read at %03x (phase %d), 003 no mark" % (r_, r_)))
        rows += [("c0ph1_7", {3: w(3, mem=0), 1: C0, 0x008: list(prog[8]), 0x007: capat(7)}, {}, "bare read at 001, capture at 007 (6 after)"),
                 ("cap007", {0x008: list(prog[8]), 0x007: capat(7)}, {}, "read 003, capture at 007: 4 steps after the read"),
                 ("c60ph1", {3: w(3, mem=0), 1: c60, 0x37: w(0x37, low=0x60)}, same, "bare absolute read at 001, write 037 absolute"),
                 ("c60ph3", {3: c60, 0x37: w(0x37, low=0x60)}, same, "bare absolute read at 003, write 037 absolute (control)")]
    elif rnd == "e":   # 40e: register 5 = 2, as both AN boards boot it (FS1R: 0x1FF4, FUN_0003a064)
        R5 = {"5": [2, 0x1FF4]}
        def capat(c):
            x = w(c, mem=0)
            x[2] = (x[2] & 0x7F) | (0x100 | S40_CELL) << 7
            return x
        rows = [("ref", {}, {}, "shipped, reg 5 as booted", None),
                ("r5_ref", {}, {}, "reg 5 = 2: shipped program", R5)]
        for r_ in (0, 1, 2):
            rows.append(("r5_ph%d" % r_, {3: w(3, mem=0), r_: list(prog[3])}, {}, "reg 5 = 2, the slot-0 read word at %03x" % r_, R5))
        rows += [("r5_abs", {3: w(3, low=0x60), 0x37: w(0x37, low=0x60)}, same, "reg 5 = 2, absolute pair (40b abs60)", R5),
                 ("r5_abs_ph0", {3: w(3, mem=0), 0: w(3, low=0x60), 0x37: w(0x37, low=0x60)}, same, "reg 5 = 2, absolute read at 000", R5),
                 ("r5_cap007", {0x008: list(prog[8]), 0x007: capat(7)}, {}, "reg 5 = 2, capture 4 after the read", R5),
                 ("ref_end", {}, {}, "reg 5 back: shipped", None)]
        probes = []
        for n, st, offs, d, v2 in rows:
            pr = dict(config="rev1c", name="s40e_" + n, steps={"%03x" % a: v for a, v in {**base, **st}.items()},
                      coefs={"%03x" % a: v for a, v in k.items()}, offs={str(a): v for a, v in offs.items()}, retrig=1, detail=d)
            if v2:
                pr["vop2regs"] = v2
            probes.append(pr)
        return prog, dict(configs={"rev1c": {"slot": "rev", "type": 1, "voice": "click", "note": 72}}, probes=probes)
    elif rnd == "f":   # 40f: phases at Hall1's own phase-1 read site (0c9 -> capture 0ce), slots 0x31/0x32 on the click
        RD = [2, 0, 0x40, 0, 0]
        cap = list(prog[0x0CE])
        cap[2] = (cap[2] & 0x7F) | (0x100 | S40_CELL) << 7
        base = {a: v for a, v in base.items() if a != 0x008}
        base.update({0x0CE: cap, 0x0C5: [0] * 5, 0x0C9: [0] * 5})
        o = {0x31: 29998, 0x32: 29998}
        rows = [("ph1", {0x0C9: RD}, o, "shipped read word at 0c9 (phase 1), 5 steps before the capture"),
                ("ph0", {0x0C8: RD}, o, "read word at 0c8 (phase 0), 6 before"),
                ("ph3", {0x0C7: RD}, o, "read word at 0c7 (phase 3), 7 before"),
                ("ph2", {0x0C6: RD}, o, "read word at 0c6 (phase 2), 8 before"),
                ("ph1b", {0x0C5: RD}, o, "read word at 0c5 (phase 1), 9 before"),
                ("none", {}, o, "no read: the capture holds")]
    elif rnd == "g":   # 40g: insertion program 13's shipped phase-0 reads (1a4 1a8 1ac 1b0), on performance 70's type
        rom = (ROMS / "fs1r_v120_eprom_cpuview.bin").read_bytes()
        itype = rom[0xA000 + 70 * 400 + 80 + 0x5F]
        ins = E.program([0, 13, 0, 0])
        def unmark(st):
            x = list(ins[st])
            x[0] &= ~7
            return x
        cfg = {"ins13": {"slot": "ins", "type": itype, "voice": "click", "note": 72}}
        rows = [("ref", {}, "insertion as shipped (P70's type %d, program 13)" % itype),
                ("nomark", {st: unmark(st) for st in (0x1A4, 0x1A8, 0x1AC, 0x1B0)}, "1a4/1a8/1ac/1b0 RAM marks cleared"),
                ("nomark_1a4", {0x1A4: unmark(0x1A4)}, "1a4's mark cleared"),
                ("nomark_1b0", {0x1B0: unmark(0x1B0)}, "1b0's mark cleared"),
                ("ref2", {}, "as shipped again")]
        probes = [dict(config="ins13", name="s40g_" + n, steps={"%03x" % a: v for a, v in st.items()}, coefs={},
                       retrig=1, detail=d) for n, st, d in rows]
        return prog, dict(configs=cfg, probes=probes)
    else:   # 40b: round a's moved reads had r8 low 0x00; here the whole read word moves (r8 low 0x40 with it)
        rows = [
            ("ref", {}, {}, "shipped, as 40a ref"),
            ("b6clr", {3: w(3, low=0x00)}, {}, "read 003 with r8 low 0x00 (bit 6 clear): bit 6 an enable -> silent"),
            ("ph0", swap(0), {}, "read word swapped 003 <-> 000 (phase 0), ring"),
            ("ph1", swap(1), {}, "read word swapped 003 <-> 001 (phase 1), ring"),
            ("ph2", swap(2), {}, "read word swapped 003 <-> 002 (phase 2), ring"),
            ("abs60", {3: w(3, low=0x60), 0x37: w(0x37, low=0x60)}, same, "40a same5 again: absolute -> R with L"),
            ("abs60_off1", {3: w(3, low=0x60), 0x37: w(0x37, low=0x60)}, {0: 28653}, "read one word above the write: absolute -> silent"),
            ("abs20", {3: w(3, low=0x20), 0x37: w(0x37, low=0x20)}, same, "both r8 low 0x20 (bit 6 clear)"),
            ("abs28", {3: w(3, low=0x28), 0x37: w(0x37, low=0x28)}, same, "both r8 low 0x28, the AN boards' table marks"),
            ("abs68", {3: w(3, low=0x68), 0x37: w(0x37, low=0x68)}, same, "both r8 low 0x68, the AN boards' other table marks"),
            ("abs60_ph0", {**swap(0, 0x60), 0x37: w(0x37, low=0x60)}, same, "absolute read on phase 0"),
            ("abs60_ph2", {**swap(2, 0x60), 0x37: w(0x37, low=0x60)}, same, "absolute read on phase 2"),
            ("abs68_ph0", {**swap(0, 0x68), 0x37: w(0x37, low=0x68)}, same, "the AN mode-0 clipper shape: 0x68 read on phase 0"),
        ]
    probes = [dict(config="rev1c", name="s40%s_%s" % (rnd if rnd != "a" else "", n), steps={"%03x" % a: v for a, v in {**base, **st}.items()},
                   coefs={"%03x" % a: v for a, v in k.items()}, offs={str(a): v for a, v in offs.items()}, retrig=1, detail=d)
              for n, st, offs, d in rows]
    return prog, dict(configs={"rev1c": {"slot": "rev", "type": 1, "voice": "click", "note": 72}}, probes=probes)


def s40_predict(spec, load):
    """Onset of R after an impulse on the input, per probe, under the four rule combinations."""
    import vop3_interp as V
    import vop3_e2e as E
    prog = E.program(load["selectors"])
    out = {}
    for pr in spec["probes"]:
        p = list(prog)
        for a, x in pr["steps"].items():
            p[int(a, 16)] = tuple(x)
        coef = list(load["coefs"])
        for a, x in pr["coefs"].items():
            coef[int(a, 16)] = x
        res = []
        for table in (False, True):
            for phases in ((1, 3), (0, 1, 2, 3)):
                V.TABLE, V.PHASES = table, phases
                it = V.Interp(p, coef, bank_of=lambda _: 0)
                it.offs = dict(enumerate(load["offsets"]))
                it.offs.update({int(a): x for a, x in pr["offs"].items()})
                nz = []
                for i in range(2600):
                    it.sample({0x0D: 1.0 if i == 5 else 0.0})
                    if abs(it.d[S40_CELL]) > 1e-9:
                        nz.append(i - 5)
                res.append(nz[0] if nz else None)
        V.TABLE, V.PHASES = False, (1, 3)
        out[pr["name"]] = res
    return out


def s40_score(d):
    """Per probe: the click on L (the retriggered note, ~0.3 s before the segment mark) and R's bursts after it,
    as (start - click, length) in samples; R in LSB of the 18-bit DAC word. [] = R silent."""
    import json as J
    import numpy as np
    import soundfile as sf
    d = Path(d)
    x, sr = sf.read(str(next(d.glob("*.flac"))), dtype="float64")
    x = x * 8 * (1 << 17)
    st = float((d / "rec_start.txt").read_text())
    res = {}
    for r in J.loads((d / "probe_segments.json").read_text()):
        a = max(0, int((r["t"] - st - 0.6) * sr))
        seg = x[a:a + int((r["hold"] + 0.6) * sr)]
        on = np.flatnonzero(np.abs(seg[:, 0]) > 50)
        if not len(on):
            continue
        l0 = int(on[0])
        bursts = []
        for i in np.flatnonzero(np.abs(seg[l0:, 1]) > 3):
            if bursts and i - bursts[-1][1] < 200:
                bursts[-1][1] = int(i)
            else:
                bursts.append([int(i), int(i)])
        res[r["name"]] = [(s0, e - s0 + 1) for s0, e in bursts][:4]
    return res


S40_TAKES = ["2026-10-15-125512-s25", "2026-10-15-130338-s25", "2026-10-15-131046-s25", "2026-10-15-131714-s25",
             "2026-10-15-133057-s25"]   # 40a, b, c, d, f (40e: register 5 = 2 silences the chip, nothing to time)


def s40_check(rules):
    """Every s40 probe's first R burst on the unit against the interpreter under `rules` (vop3_interp globals).
    Returns (matched, total, misses). A burst within 2 samples of the model's onset, or both silent, matches."""
    import vop3_interp as V
    caps = ROOT.parent / "FS1R.unlock" / "captures"
    saved = {k: getattr(V, k) for k in rules}
    for k, v in rules.items():
        setattr(V, k, v)
    ok, n, miss = 0, 0, []
    try:
        for t in S40_TAKES:
            d = caps / t
            spec = json.loads((d / "probes.json").read_text())
            load = json.loads((d / "session.json").read_text())["results"]["probe"]["loads"]["rev1c"]
            meas = s40_score(d)
            import vop3_e2e as E
            prog = E.program(load["selectors"])
            for pr in spec["probes"]:
                if pr.get("vop2regs"):
                    continue
                p = list(prog)
                for a, x in pr["steps"].items():
                    p[int(a, 16)] = tuple(x)
                coef = list(load["coefs"])
                for a, x in pr["coefs"].items():
                    coef[int(a, 16)] = x
                it = V.Interp(p, coef, bank_of=lambda _: 0)
                it.offs = dict(enumerate(load["offsets"]))
                it.offs.update({int(a): x for a, x in pr["offs"].items()})
                on = None
                for i in range(20000 if pr["name"].endswith("none") else 2600):
                    it.sample({0x0D: 1.0 if i == 5 else 0.0})
                    if on is None and abs(it.d[S40_CELL]) > 1e-9:
                        on = i - 5
                        break
                m = meas[pr["name"]][0][0] if meas.get(pr["name"]) else None
                good = (m is None and on is None) or (m is not None and on is not None and abs(m - on) <= 2)
                ok += good
                n += 1
                if not good:
                    miss.append((pr["name"], m, on))
    finally:
        for k, v in saved.items():
            setattr(V, k, v)
    return ok, n, miss


if __name__ == "__main__":
    if "--rig" in sys.argv:
        rnd = sys.argv[sys.argv.index("--rig") + 1] if len(sys.argv) > sys.argv.index("--rig") + 1 else "a"
        prog, spec = s40_probes(rnd)
        S40 = S40.with_name("40%s.probes.json" % rnd)
        S40.write_text(json.dumps(spec))
        sess = json.loads(Path(ROOT.parent / "FS1R.unlock/captures/2026-10-02-090236-s25/session.json").read_text())
        pred = s40_predict(spec, sess["results"]["probe"]["loads"]["rev1c"])
        print("wrote", S40, "\nR onset after the input, by rule: ring/(1,3)  ring/all  table/(1,3)  table/all")
        for n, v in pred.items():
            print("  %-10s %s" % (n, "  ".join("%8s" % x for x in v)))
        sys.exit(0)
    if "--check" in sys.argv:
        ok, n, miss = s40_check({})
        print("s40: %d / %d probes' first R burst within 2 samples of the interpreter" % (ok, n))
        for m in miss:
            print("   miss %-18s unit %s model %s" % m)
        assert ok >= 46, "the s40 rules no longer reproduce the takes"
        sys.exit(0)
    if "--score" in sys.argv:
        for n, b in s40_score(sys.argv[sys.argv.index("--score") + 1]).items():
            print("  %-18s R bursts after the click (start, length): %s" % (n, b or "silent"))
        sys.exit(0)
    main(sys.argv[1:])
