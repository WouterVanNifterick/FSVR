#!/usr/bin/env python3
"""Pull Yamaha's FS1R Bonus Tool Pack presets out of the web archive's copy of yamahasynth.com.

    python tools/extract_bonus.py   -> presets/bonus/1_Best_of_DX.syx, 2_Popular.syx, 3_Cyber.syx, 4_WX5.syx

The pack was Yamaha's download at yamahasynth.com/download/fs1r/s_bonus.htm (updated 15/05/2000), now only
on the Wayback Machine (snapshot 20080703223529). Each file written here is one internal-memory set as the
unit takes it: 128 voices (0x51 nn) then 128 performances (0x11 nn), whose parts name those voices as Int.

  1_Best_of_DX  BestofDXVoice/Vdfs1r01.mid   Best of DX Voice, a Standard MIDI File of bulks
  2_Popular     Disk3/Sounds/POPULAR.MID      the same, as Yamaha also offered it alone (popular.zip)
  3_Cyber       Disk3/Sounds/CYBER.MID        and cyber.zip
  4_WX5         Disk4/FSforWXbank/WxBank1     Jan Beiling's WX5 soundbank, 1999, in the editor's .lib layout:
                                              26 byte header, 128 x 400 performance, 128 x 608 voice

The WX5 bank is only in the Mac archive (bonus.hqx: BinHex 4 of a StuffIt 1.5 archive, LZ+Huffman, method
13), so that is the one read for all four; its three MIDI files match the PC zip's byte for byte. Four of the
WX5 bank's parts carry velocity limit low 0, below the Data List's 1..127; they are written as 1, which is
what Yamaha's own editor wrote for the same thing when it exported Best of DX (Vdfs1r01.lib -> .mid).
"""
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "presets" / "bonus"
URL = "https://web.archive.org/web/20080703223529id_/http://www.yamahasynth.com/download/fs1r/bonus.hqx"
SETS = {"Vdfs1r01.mid": "1_Best_of_DX", "POPULAR.MID": "2_Popular", "CYBER.MID": "3_Cyber", "WxBank1": "4_WX5"}


def unbinhex(text):
    """BinHex 4.0: 6-bit text, 0x90 run-length, then a header; the data fork."""
    abc = "!\"#$%&'()*+,-012345689@ABCDEFGHIJKLMNPQRSTUVXYZ[`abcdefhijklmpqr"
    s = text[text.index(":", text.index("BinHex")) + 1:]
    s = "".join(c for c in s[:s.index(":")] if not c.isspace())
    raw, acc, n = bytearray(), 0, 0
    for c in s:
        acc, n = acc << 6 | abc.index(c), n + 6
        if n >= 8:
            n -= 8
            raw.append(acc >> n & 0xFF)
    out, i, last = bytearray(), 0, 0
    while i < len(raw):
        b = raw[i]
        i += 1
        if b != 0x90:
            out.append(b)
            last = b
        elif raw[i] == 0:
            out.append(0x90)
            last = 0x90
            i += 1
        else:
            out += bytes([last]) * (raw[i] - 1)
            i += 1
    p = 1 + out[0] + 1 + 10
    size = int.from_bytes(out[p:p + 4], "big")
    return bytes(out[p + 10:p + 10 + size])


# StuffIt method 13 (LZ+Huffman), after XADMaster's XADStuffIt13Handle.m (LGPL 2.1). Only its dynamic-code
# form, which is the one every file in this archive uses.
META_CODES = [0x5d8, 0x058, 0x040, 0x0c0, 0x000, 0x078, 0x02b, 0x014, 0x00c, 0x01c, 0x01b, 0x00b, 0x010, 0x020, 0x038,
              0x018, 0x0d8, 0xbd8, 0x180, 0x680, 0x380, 0xf80, 0x780, 0x480, 0x080, 0x280, 0x3d8, 0xfd8, 0x7d8, 0x9d8,
              0x1d8, 0x004, 0x001, 0x002, 0x007, 0x003, 0x008]
META_LENGTHS = [11, 8, 8, 8, 8, 7, 6, 5, 5, 5, 5, 6, 5, 6, 7, 7, 9, 12, 10, 11, 11, 12, 12, 11, 11, 11, 12, 12, 12, 12, 12,
                5, 2, 2, 3, 4, 5]


class Bits:   # least significant bit first
    def __init__(self, data):
        self.d, self.p = data, 0

    def bit(self):
        b = self.d[self.p >> 3] >> (self.p & 7) & 1
        self.p += 1
        return b

    def bits(self, n):
        return sum(self.bit() << i for i in range(n))

    def symbol(self, code):   # code: {(bits read first-to-last, length): value}
        c = n = 0
        while (c, n) not in code:
            c, n = c << 1 | self.bit(), n + 1
            if n > 32:
                raise ValueError("bad Huffman code")
        return code[(c, n)]


def canonical(lengths):
    code, c = {}, 0
    for n in range(1, 33):
        for v, l in enumerate(lengths):
            if l == n:
                code[(c, n)] = v
                c += 1
        c <<= 1
    return code


def read_code(b, meta, count):
    lengths, length, i = [0] * count, 0, 0
    while i < count:
        v = b.symbol(meta)
        run = 0
        if v == 31:
            length = -1
        elif v == 32:
            length += 1
        elif v == 33:
            length -= 1
        elif v == 34:
            run = b.bit()
        elif v == 35:
            run = b.bits(3) + 2
        elif v == 36:
            run = b.bits(6) + 10
        else:
            length = v + 1
        for _ in range(run):
            lengths[i] = length
            i += 1
        if i < count:
            lengths[i] = length
        i += 1
    return canonical(lengths)


def unstuff13(data, size):
    b = Bits(data)
    head = b.bits(8)
    if head >> 4:
        raise ValueError("a static StuffIt 13 code set; not needed for this archive")
    meta = {(int(format(c, f"0{n}b")[::-1], 2), n): v for v, (c, n) in enumerate(zip(META_CODES, META_LENGTHS))}
    first = read_code(b, meta, 321)
    second = first if head & 8 else read_code(b, meta, 321)
    offsets = read_code(b, meta, (head & 7) + 10)
    out, code = bytearray(), first
    while len(out) < size:
        v = b.symbol(code)
        if v < 256:
            out.append(v)
            code = first
            continue
        code = second
        if v < 0x13E:
            n = v - 0x100 + 3
        elif v == 0x13E:
            n = b.bits(10) + 65
        elif v == 0x13F:
            n = b.bits(15) + 65
        else:
            break
        k = b.symbol(offsets)
        back = 1 if k == 0 else 2 if k == 1 else (1 << (k - 1)) + b.bits(k - 1) + 1
        for _ in range(n):
            out.append(out[-back])
    return bytes(out[:size])


def unstuff(sit, wanted):
    """StuffIt 1.5 classic: 22 byte archive header, then 112 byte entries; folders open (32) and close (33)."""
    files, i = {}, 22
    while i + 112 <= len(sit) and sit[i + 2]:
        h = sit[i:i + 112]
        name = h[3:3 + h[2]].decode("mac_roman")
        rsize, dsize, rpacked, dpacked = (int.from_bytes(h[o:o + 4], "big") for o in (0x54, 0x58, 0x5C, 0x60))
        if h[0] in (32, 33):
            i += 112
            continue
        if name in wanted:
            data = sit[i + 112 + rpacked:i + 112 + rpacked + dpacked]
            files[name] = unstuff13(data, dsize) if h[1] == 13 else data if h[1] == 0 else None
        i += 112 + rpacked + dpacked
    return files


def smf_sysex(d):
    """Every complete F0..F7 in a Standard MIDI File, split packets joined."""
    out, i = [], 8 + int.from_bytes(d[4:8], "big")
    while i < len(d):
        n = int.from_bytes(d[i + 4:i + 8], "big")
        t, j, run, part = d[i + 8:i + 8 + n], 0, 0, None
        i += 8 + n

        def vlq():
            nonlocal j
            v = 0
            while True:
                v = v << 7 | t[j] & 0x7F
                j += 1
                if t[j - 1] < 0x80:
                    return v
        while j < len(t):
            vlq()
            s = t[j]
            if s >= 0x80:
                j += 1
            else:
                s = run
            if s == 0xFF:
                j += 1
                n = vlq()   # not j += vlq(): that adds to j as it was before vlq() moved it
                j += n
            elif s in (0xF0, 0xF7):
                n = vlq()
                body = t[j:j + n]
                j += n
                part = bytes([0xF0]) + body if s == 0xF0 else part + body if part is not None else None
                if part is not None and part.endswith(b"\xF7"):
                    out.append(part)
                    part = None
            else:
                run = s
                j += 1 if (s & 0xF0) in (0xC0, 0xD0) else 2
    return out


def bulk(ah, al, data):
    body = [len(data) >> 7, len(data) & 0x7F, ah, 0, al] + list(data)
    return bytes([0xF0, 0x43, 0x00, 0x5E] + body + [-sum(body) & 0x7F, 0xF7])


def from_lib(d):
    perfs = [bytearray(d[26 + 400 * k:26 + 400 * (k + 1)]) for k in range(128)]
    for p in perfs:
        for part in range(4):
            p[192 + 52 * part + 0x2A] = max(1, p[192 + 52 * part + 0x2A])   # velocity limit low: 1..127
    voices = [d[26 + 51200 + 608 * k:26 + 51200 + 608 * (k + 1)] for k in range(128)]
    return b"".join(bulk(0x51, k, v) for k, v in enumerate(voices)) + b"".join(bulk(0x11, k, p) for k, p in enumerate(perfs))


def from_smf(d):
    msgs = smf_sysex(d)
    voices = [m for m in msgs if m[3] == 0x5E and m[6] == 0x51]
    perfs = [m for m in msgs if m[3] == 0x5E and m[6] == 0x11]
    return b"".join(voices + perfs)


def main():
    print(f"fetching {URL}")
    hqx = urllib.request.urlopen(URL, timeout=120).read().decode("latin1")
    files = unstuff(unbinhex(hqx), SETS)
    OUT.mkdir(parents=True, exist_ok=True)
    for name, stem in SETS.items():
        if not files.get(name):
            print(f"{name} is not in the archive", file=sys.stderr)
            return 1
        syx = from_lib(files[name]) if name == "WxBank1" else from_smf(files[name])
        n = syx.count(0xF0)
        if n != 256:
            print(f"{name}: {n} bulks, expected 128 voices and 128 performances", file=sys.stderr)
            return 1
        (OUT / f"{stem}.syx").write_bytes(syx)
        print(f"{name} -> presets/bonus/{stem}.syx ({len(syx)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
