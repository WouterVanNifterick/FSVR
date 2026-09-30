#!/usr/bin/env python3
"""VOP3 (YSS236) microcode field disassembler. Work in progress: splits each step into the fields
the firmware and the block structure prove exist, and names the ones with a settled meaning.
See docs/vop3_isa.md for the evidence behind each field.

    python tools/vop3_disasm.py docs/vop3/program_0.bin --coef docs/vop3/coefficients_0.bin --byte docs/vop3/byte_table.bin
    python tools/vop3_disasm.py docs/vop3_an/mode0.bin --an --first 4
    python tools/vop3_disasm.py docs/vop3_2/base.bin --byte docs/vop3_2/base_bytes.bin

Step formats:
  5-word (FS1R uploads): r10 r9 r8 r7 r6, big-endian, 10 bytes per step
  8-word (--an, PLG150-AN images): r0C slot r0B r10 r9 r8 r7 r6, 16 bytes per step
"""
import argparse
import struct


def fields(r10, r9, r8, r7, r6):
    return dict(
        mem=r10 & 7,            # delay-memory access this step (slot = step >> 2); 0 on FS1R VOP3-1, which has no DRAM
        r10hi=r10 >> 3,
        op9=r9 >> 8,            # r9[15:8]: 0x11..0x18, 0x40/0x41, 0x60, 0x80, 0x01/02/04/08 ...
        ra=r9 & 0x7f,           # r9[6:0]: register address A (per-voice state, stride 6 per filter channel)
        r9b7=(r9 >> 7) & 1,
        daddr=r8 >> 7,          # r8[15:7]: 9-bit data-memory address (points at other steps' constants / voice state)
        mmode=r8 & 0x7f,        # r8[6:0]: memory-access mode bits (0x40 on VOP3-2 reads, 0x28/0x30/0x68 on the AN)
        f7a=r7 >> 14, f7b=(r7 >> 12) & 3, f7c=(r7 >> 7) & 0x1f, f7d=(r7 >> 6) & 1, f7e=r7 & 0x3f,
        f6a=r6 >> 14,           # r6[15:14]: 2 = normal step, 1 = constant/parameter load, 0 = idle, 3 rare
        rb=(r6 >> 7) & 0x7f,    # r6[13:7]: register address B (per-voice state, stride 6)
        f6c=r6 & 0x7f,          # r6[6:0]: 7-bit field, shares the 17..27 / 37..42 / 61..63 value ranges with r7[5:0]
    )


def disasm(step, words, coef=None, byt=None):
    r10, r9, r8, r7, r6 = words
    f = fields(r10, r9, r8, r7, r6)
    if not any(words):
        return "%03x  nop" % step
    parts = []
    if f["mem"]:
        parts.append("mem%d[slot %02x] mode %02x" % (f["mem"], step >> 2, f["mmode"]))
    if f["daddr"]:
        parts.append("d[%03x]" % f["daddr"])
    if f["ra"]:
        parts.append("rA=%02x" % f["ra"])
    if f["rb"]:
        parts.append("rB=%02x" % f["rb"])
    if f["f6c"]:
        parts.append("c6=%02x" % f["f6c"])
    if f["f7e"]:
        parts.append("c7=%02x" % f["f7e"])
    op = "op9=%02x r7=%d.%d.%02x.%d r6t=%d" % (f["op9"], f["f7a"], f["f7b"], f["f7c"], f["f7d"], f["f6a"])
    k = ""
    if coef is not None and coef[step]:
        k += " k=%04x(%+.4f)" % (coef[step], struct.unpack(">h", struct.pack(">H", coef[step]))[0] / 32768.0)
    if byt is not None and byt[step]:
        k += " b=%d" % byt[step]
    return "%03x  %s  %s%s" % (step, op, " ".join(parts), k)


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
    a = ap.parse_args()
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
        print(disasm(a.first + i, w, coef, byt))


def demo():
    # FS1R VOP3-1 step 0x08e (ch0 cutoff constant): r7=8000 r6=4000, k=7fff
    f = fields(0, 0x0061, 0, 0x8000, 0x4000)
    assert f["ra"] == 0x61 and f["f6a"] == 1 and f["rb"] == 0 and f["daddr"] == 0
    # PLG150-AN step 0x11: r10=2 marks a memory access, r8 low bits carry the mode
    f = fields(2, 0x0017, 0x0068, 0x4112, 0x800A)
    assert f["mem"] == 2 and f["mmode"] == 0x68 and f["daddr"] == 0 and f["f7e"] == 0x12
    assert disasm(0, (0, 0, 0, 0, 0)) == "000  nop"
    print("ok")


if __name__ == "__main__":
    import sys
    demo() if len(sys.argv) == 1 else main()
