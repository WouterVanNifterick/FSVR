#!/usr/bin/env python3
"""The VOP3 external RAM on the PLG150-AN and AN1x: what the H8 uploads, what the programs read, and the AN1x
mask ROM rebuilt from it. Findings in docs/vop3_an_ram.md.

    python tools/vop3_an_ram.py            print the map, check it, write docs/vop3_an1x/maskrom.bin
    python tools/vop3_an_ram.py --probe    also run both programs through yss236_core (tools/vop3_an_ram.cpp)

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
    """Run each program through yss236_core with the RAM loaded, both addressing rules, and print where the reads
    land: tools/vop3_an_ram.cpp reads `prog` lines on stdin and prints one line per read."""
    exe = Path("/tmp/vop3_an_ram")
    src = ROOT / "tools" / "vop3_an_ram.cpp"
    for rule in ("ring", "table"):
        out = exe.with_name(exe.name + "_" + rule)
        if not out.exists() or out.stat().st_mtime < src.stat().st_mtime:
            flags = ["-DYSS236_TABLE_ADDR"] if rule == "table" else []
            subprocess.run(["g++", "-std=c++17", "-O2", *flags, "-I", str(ROOT / "src"), str(src), "-o", str(out)], check=True)
        an1x_rom = words((OUT / "maskrom.bin").read_bytes())[:0x6022]
        for name, prog, ram in (("PLG150-AN mode 0", plg_program(0), plg_ram()), ("AN1x voice", an1x_program(), an1x_rom)):
            lines = ["ram " + " ".join("%x" % w for w in ram)]
            for st, w in sorted(prog.items()):
                tag, slot, k, r10, r9, r8, r7, r6 = w
                lines.append("step %x %x %x %x %x %x %x %x" % (st, r10, r9, r8, r7, r6, k, offset(slot) if r10 & 7 else 0))
            res = subprocess.run([str(out)], input="\n".join(lines) + "\nrun 4800\n", capture_output=True, text=True, check=True)
            reads = [tuple(int(x, 16) for x in ln.split()[1:]) for ln in res.stdout.splitlines() if ln.startswith("read")]
            inside = sum(a < len(ram) for _, a, _ in reads)
            nz = sum(v != 0 for *_, v in reads)
            print("%-6s %-17s %6d reads, %5.1f%% inside the table image, %5.1f%% non-zero"
                  % (rule, name, len(reads), 100 * inside / max(1, len(reads)), 100 * nz / max(1, len(reads))))
            if rule == "table":     # the index each offset was read with: address - offset
                spans = {}
                for st, a, _ in reads:
                    off = offset(prog[st][1])
                    d = (a - off + (1 << 17)) % (1 << 18) - (1 << 17)
                    lo, hi = spans.get(off, (d, d))
                    spans[off] = (min(lo, d), max(hi, d))
                silent = sorted({a[2] for a in accesses(prog)} - set(spans))
                print("         offsets marked but never transferred (reads on phase 0/2): %s" % " ".join("%05X" % o for o in silent))
                for off, (lo, hi) in sorted(spans.items()):
                    a1 = name.startswith("AN1x")
                    print("         %05X  index %+6d .. %+6d  -> %s" % (off, lo, hi, table_of(off + lo, a1) + " .. " + table_of(off + hi, a1)))


if __name__ == "__main__":
    main(sys.argv[1:])
