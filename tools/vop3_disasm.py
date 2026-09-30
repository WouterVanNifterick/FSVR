#!/usr/bin/env python3
"""VOP3 (YSS236) microcode field disassembler. Work in progress: splits each step into the fields
the firmware and the block structure prove exist, and names the ones with a settled meaning.
See docs/vop3_isa.md for the evidence behind each field.

    python tools/vop3_disasm.py docs/vop3/program_0.bin --coef docs/vop3/coefficients_0.bin --byte docs/vop3/byte_table.bin
    python tools/vop3_disasm.py docs/vop3_an/mode0.bin --an --first 4
    python tools/vop3_disasm.py docs/vop3_2/base.bin --byte docs/vop3_2/base_bytes.bin
    python tools/vop3_disasm.py docs/vop3_an1x/voice.bin --an --labels docs/an1x_param_map.md

Step formats:
  5-word (FS1R uploads): r10 r9 r8 r7 r6, big-endian, 10 bytes per step
  8-word (--an, PLG150-AN images): r0C slot r0B r10 r9 r8 r7 r6, 16 bytes per step
"""
import argparse
import struct


def fields(r10, r9, r8, r7, r6):
    # Field roles marked CHIP are measured on the FS1R (FS1R.unlock captures/2026-09-30-9, single-bit
    # flips on live steps); the rest are from the three firmwares' structure.
    return dict(
        mem=r10 & 7,            # delay-memory access this step (slot = step >> 2); 0 on FS1R VOP3-1, which has no DRAM
        r10hi=r10 >> 3,
        op9=(r9 >> 8) & 0xf,    # CHIP r9[11:8]: exact-match, all 15 other values close the filter; per-channel 1/2/.., a path/slot id, not an opcode
        op9hi=(r9 >> 12) & 7,   # r9[14:12]: bit 12 changes the operation (0x11 -> 0x01 opens the filter), 13/14 inert
        ra=r9 & 0x7f,           # CHIP r9[6:0]: source register A on a MAC step; destination register on a constant load
        r9b7=(r9 >> 7) & 1,     # CHIP r9[7]: a mode bit of its own (same effect as r9[15])
        r9b15=r9 >> 15,
        daddr=r8 >> 7,          # r8[15:7]: 9-bit data-memory address (points at other steps' constants / voice state)
        mmode=r8 & 0x7f,        # r8[6:0]: memory-access mode bits (0x40 on VOP3-2 reads, 0x28/0x30/0x68 on the AN)
        f7a=r7 >> 14,           # CHIP r7[15:14]: no effect on any of three probed steps
        f7b=(r7 >> 12) & 3,     # CHIP r7[13:12]: result routing; 0 = corner drops an octave, 1 = right, 2/3 = runaway
        f7c=(r7 >> 7) & 0x1f,   # (r7[11:7], kept for the listing) CHIP: r7[10] a selector (alone = filter opens), r7[9] ignored on the MAC probed
        f7d=(r7 >> 6) & 1,
        op7=(r7 >> 6) & 7,      # CHIP r7[8:6]: 3-bit MAC operation; the cutoff MAC needs 2, every other code loses corner or gain
        r7sel=(r7 >> 10) & 1,   # CHIP r7[10]: 0x10 alone opens the filter with the peak intact
        rd_en=(r7 >> 4) & 1,    # CHIP r7[4]: read enable; with it clear r7[3:0] is ignored (all 16 values inert)
        rd=r7 & 0xf,            # CHIP r7[3:0]: register read when enabled (0xc..0xf hold a scaled copy of the operand); r7[5] inert
        f7e=r7 & 0x3f,
        f6a=r6 >> 14,           # CHIP r6[15:14] class: 1 = constant load (rest of r6 unused), 2 = MAC, 0 = no output, 3 = runaway
        rb=(r6 >> 7) & 0x7f,    # CHIP r6[13:7]: source register B on a MAC step (0x61 = where the cutoff constant is loaded)
        f6c=r6 & 0x3f,          # CHIP r6[5:0]: exact-match (55/63 other values run away): the result's write slot w[]; r7[3:0] reads it back
                                # FW: w[] is not r[40..7f]: the FS1R's stride-6 per-channel registers run through 0x40 (tools/vop3_verify.py)
    )


def encode(cls=2, ra=0, rb=0, wdst=0, op=0, sel=0, route=0, rd=None, path=0, mem=0, daddr=0, mmode=0,
           r9x=0, r7x=0, r6x=0, r10x=0):
    """The inverse of fields(): build (r10, r9, r8, r7, r6) for a probe step. rd=None leaves the read disabled;
    the *x arguments OR raw bits in for fields that have no name yet."""
    r10 = mem | r10x
    r9 = path << 8 | ra | r9x
    r8 = daddr << 7 | mmode
    r7 = route << 12 | sel << 10 | op << 6 | (0x10 | rd if rd is not None else 0) | r7x
    r6 = cls << 14 | rb << 7 | wdst | r6x
    return r10, r9, r8, r7, r6


def reg(n, voices=0):
    """Register name. With --voices V, r[1 + V*k + v] is input/state k of voice v (AN1x: V=5; the FS1R filter
    is V=6 per channel with r[6c] the class-1 control word, so its stride starts at 0, not 1)."""
    if voices and 0 < n <= 13 * voices:      # AN1x: k = 0..12 verified per-voice (tools/vop3_verify.py); above that, scheduler temporaries
        k, v = divmod(n - 1, voices)
        return "in[%d].%d" % (k, v)
    return "r[%02x]" % n


def disasm(step, words, coef=None, byt=None, labels=None, voices=0):
    r10, r9, r8, r7, r6 = words
    f = fields(r10, r9, r8, r7, r6)
    lab = ("   ; " + labels[step]) if labels and step in labels else ""
    if not any(words):
        return "%03x  nop%s" % (step, lab)
    parts = []
    if f["mem"]:
        parts.append("mem%d[slot %02x] mode %02x" % (f["mem"], step >> 2, f["mmode"]))
    if f["daddr"]:
        parts.append("d[%03x]" % f["daddr"])
    if f["f6a"] == 1:
        parts.append("r[%02x] = k" % f["ra"])            # constant load: r9[7:0] names the destination register
        if f["rb"]:
            parts.append("rB=" + reg(f["rb"], voices))   # the r9[15] variant (AN uploads: k=0, rA=0) carries an rB; role unknown
    else:
        if f["ra"]:
            parts.append("rA=" + reg(f["ra"], voices))
        if f["rb"]:
            parts.append("rB=" + reg(f["rb"], voices))
        if f["f6c"]:
            parts.append("-> w[%02x]" % f["f6c"])
    if f["rd_en"]:
        parts.append("rd w[%x]" % f["rd"])        # replaces k as the third operand (k is dead on every FS1R rd-en step, FW)
    op = "path=%x.%x%s%s route=%d op=%d%s r7=%d.%02x class=%d" % (f["op9hi"], f["op9"], "+" if f["r9b7"] else "", "^" if f["r9b15"] else "",
                                                                 f["f7b"], f["op7"], "s" if f["r7sel"] else "", f["f7a"], f["f7c"], f["f6a"])
    k = ""
    if coef is not None and coef[step]:
        k += " k=%04x(%+.4f)" % (coef[step], struct.unpack(">h", struct.pack(">H", coef[step]))[0] / 32768.0)
    if byt is not None and byt[step]:
        k += " b=%d" % byt[step]
    return "%03x  %s  %s%s%s" % (step, op, " ".join(parts), k, lab)


def load_labels(path):
    """docs/an1x_param_map.md lines: - `33` VCF Cutoff  ... steps=[165 16d ...] | step=004/009"""
    import re
    labels = {}
    for line in open(path, encoding="utf-8"):
        m = re.match(r"- `(\w+)` (.+?)\s+handler=.*?(?:steps=\[([0-9a-f ]+)\]|step=([0-9a-f]+)/([0-9a-f]+))", line)
        if not m:
            continue
        name = m.group(2).strip()
        steps = m.group(3).split() if m.group(3) else [m.group(4), m.group(5)]
        for v, st in enumerate(steps):
            st = int(st, 16)
            if st:
                tag = "%s v%d" % (name, v) if m.group(3) else name
                labels[st] = (labels[st] + ", " + tag) if st in labels else tag
    return labels


def load(path, an=False):
    b = open(path, "rb").read()
    if an:
        rows = [struct.unpack_from(">8H", b, i * 16) for i in range(len(b) // 16)]
        return [r[3:] for r in rows], [r[2] for r in rows], [r[0] for r in rows]
    return [struct.unpack_from(">5H", b, i * 10) for i in range(len(b) // 10)], None, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("program")
    ap.add_argument("--an", action="store_true", help="8-word PLG150-AN image")
    ap.add_argument("--coef", help="512 x u16 per-step constants (reg 0xB)")
    ap.add_argument("--byte", help="per-step byte table (reg 0xC)")
    ap.add_argument("--first", type=lambda x: int(x, 0), default=0, help="program address of the first step")
    ap.add_argument("--labels", help="parameter->step map (docs/an1x_param_map.md) to annotate steps")
    ap.add_argument("--voices", type=int, default=0, help="name r[01..3f] as in[k].v with this many voices per input (AN1x: 5)")
    a = ap.parse_args()
    labels = load_labels(a.labels) if a.labels else None
    steps, coef, byt = load(a.program, a.an)
    if a.coef:
        c = open(a.coef, "rb").read(); coef = list(struct.unpack(">%dH" % (len(c) // 2), c))
    if a.byte:
        byt = list(open(a.byte, "rb").read())
    if coef is not None and len(coef) < a.first + len(steps):
        coef = [0] * a.first + list(coef)   # AN images carry the constant per row, at that row's address
    if byt is not None and len(byt) < a.first + len(steps):
        byt = [0] * a.first + list(byt)
    for i, w in enumerate(steps):
        print(disasm(a.first + i, w, coef, byt, labels, a.voices))


def demo():
    # FS1R VOP3-1 step 0x08e (ch0 cutoff constant): r7=8000 r6=4000, k=7fff
    f = fields(0, 0x0061, 0, 0x8000, 0x4000)
    assert f["ra"] == 0x61 and f["f6a"] == 1 and f["rb"] == 0 and f["daddr"] == 0
    assert "r[61] = k" in disasm(0x8e, (0, 0x0061, 0, 0x8000, 0x4000))
    # FS1R step 0x098, the cutoff MAC: reg 0x62 * reg 0x61, op 1, mode 0x3e (measured on the chip)
    f = fields(0, 0x1162, 0, 0x9280, 0xb0be)
    assert f["op9"] == 1 and f["ra"] == 0x62 and f["rb"] == 0x61 and f["f6c"] == 0x3e and f["f6a"] == 2
    assert f["op7"] == 2 and f["r7sel"] == 0 and f["rd_en"] == 0
    assert fields(0, 0, 0, 0x8291, 0)["rd_en"] == 1 and fields(0, 0, 0, 0x8291, 0)["rd"] == 1
    assert "-> w[3e]" in disasm(0x98, (0, 0x1162, 0, 0x9280, 0xb0be)) and "rd w[1]" in disasm(0x9c, (0, 0x1162, 0, 0x8291, 0xb13d))
    assert "rA=in[2].1" in disasm(0x58, (0, 0x000c, 0, 0x0140, 0x8615), voices=5)   # AN1x 058: r[0c] = input 2, voice 1
    # PLG150-AN step 0x11: r10=2 marks a memory access, r8 low bits carry the mode
    f = fields(2, 0x0017, 0x0068, 0x4112, 0x800A)
    assert f["mem"] == 2 and f["mmode"] == 0x68 and f["daddr"] == 0 and f["f7e"] == 0x12
    assert disasm(0, (0, 0, 0, 0, 0)) == "000  nop"
    assert encode(ra=0x62, rb=0x61, wdst=0x3e, op=2, route=1, path=1, r9x=0x1000, r7x=0x8200) == (0, 0x1162, 0, 0x9280, 0xb0be)
    assert encode(ra=0x62, rb=0x62, wdst=0x3d, op=2, rd=1, path=1, r9x=0x1000, r7x=0x8200) == (0, 0x1162, 0, 0x8291, 0xb13d)
    assert encode(cls=1, ra=0x61, r7x=0x8000) == (0, 0x0061, 0, 0x8000, 0x4000)
    print("ok")


if __name__ == "__main__":
    import sys
    demo() if len(sys.argv) == 1 else main()
