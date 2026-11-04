"""Shared helpers for the translation tools (tools/translation/*.py)."""
import csv
import glob
import os
import re

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
WORK = os.path.join(ROOT, "build", "translation")          # local only: holds the game's English text
LANG_DIR = os.path.join(ROOT, "data", "lang")               # committed: translated text only

# the game's codes inside text, which a translation must keep (as a multiset):
#   %s %d %3d ... printf fields, %mN% a glyph value, %w a wait; @@KEY the configured key's name; #N colours
#   @@ + an SJIS arrow (0x81 0xa8..0xab, here as the Latin-1 characters \x81 ¨©ª«) [+ K] = an arrow key icon
KEEP_RE = re.compile(r"%[-+ #0]*\d*(?:\.\d+)?[sdiuxXcf]|%m\d*%?|%w|@@\x81[\xa8-\xab]K?|@@[A-Za-z0-9+\-]+|#[0-9]|\x81[\x40-\xfc]")
# %n = a manual line break: a translation may move or drop it
BREAK = "%n"

CP1252_EXTRA = set("€‚ƒ„…†‡ˆ‰Š‹ŒŽ‘’“”•–—˜™š›œžŸ")


def load_strings():
    """strings.csv (extract_text.py) as a dict va -> row"""
    path = os.path.join(WORK, "strings.csv")
    if not os.path.exists(path):
        raise SystemExit(f"{path} is missing: run tools/translation/extract_text.py <game dir> first")
    return {int(r["va"], 16): r for r in csv.DictReader(open(path, encoding="utf-8"))}


# where a copied string lands -> the bytes that destination holds (text included NUL); others: the English length
#   the scene runner's choice records: +0x5a0 + 0x58 * i {text, ..., +0x5f0 / +0x5f4 ints} -> 0x50 bytes
DEST_CAP = [(re.compile(r"\+\s*(?:1440|0x5a0)\b|F32\(\s*c\s*,\s*0x5a0\s*\)"), 0x50 - 1)]


def port_refs():
    """(VAs the port reads through g_eng.data: D(0x...) / S(0x...) in src/,
        {VA: byte cap or None} for those copied into fixed buffers by strcpy / sprintf / ... (None = English length))"""
    src = "".join(open(f, encoding="utf-8", errors="replace").read() for f in glob.glob(os.path.join(ROOT, "src", "*.c")))
    dlit = {int(x, 16) for x in re.findall(r"\b[DS]\((0x[0-9a-fA-F]{6})u?\)", src)}
    cap = {}
    for m in re.finditer(r"(?:strcpy|strcat|sprintf|wsprintfA|lstrcpyA|strncpy)\w*\(\s*([^;]*?),[^;]*?\b[DS]\((0x[0-9a-fA-F]{6})u?\)", src):
        va = int(m.group(2), 16)
        c = next((n for r, n in DEST_CAP if r.search(m.group(1))), None)
        if c is None and re.match(r"\w+\(\s*\(char \*\)&F32\(\s*c\s*,\s*0x5a0\s*\)", m.group(0)):
            c = 0x50 - 1   # the first choice record (the comma inside F32() cut group 1 short)
        if va in cap and cap[va] is not None and c is not None:
            cap[va] = min(cap[va], c)
        else:
            cap[va] = None if (va in cap and cap[va] is None) or c is None else c
    # strcpy(NAME, D(..)) into a stack buffer the decompile declares with its size ("char *NAME = ...; /* [N] at
    # ebp-... */" or "char NAME[N]"): the cap is N - 1 when every copy of that string goes into such a buffer
    sized = {}
    for f in glob.glob(os.path.join(ROOT, "src", "*.c")):
        lines = open(f, encoding="utf-8", errors="replace").read().split("\n")
        for i, line in enumerate(lines):
            for m in re.finditer(r"strcpy\(\s*(?:\(\s*char\s*\*\s*\)\s*)?([A-Za-z_]\w*)\s*,[^;]*?\b[DS]\((0x[0-9a-fA-F]{6})u?\)", line):
                name, va = m.group(1), int(m.group(2), 16)
                size = None
                for j in range(i, max(-1, i - 800), -1):
                    d = re.search(r"\b%s\s*=\s*\(char \*\)\(frame_[^;]*;\s*/\*\s*\[(\d+)\]" % re.escape(name), lines[j]) \
                        or re.search(r"\bchar\s+%s\s*\[(\d+)\]" % re.escape(name), lines[j])
                    if d:
                        size = int(d.group(1))
                        break
                sized.setdefault(va, []).append(size)
    for va, sizes in sized.items():
        if va in cap and cap[va] is None and None not in sizes:
            n_copies = len(re.findall(r"(?:strcpy|strcat|sprintf|wsprintfA|lstrcpyA|strncpy)\w*\([^;]*?\b[DS]\(0x%06xu?\)" % va, src))
            if n_copies == len(sizes):   # no strcat / sprintf / other destination for this string
                cap[va] = min(sizes) - 1
    # strcpy((char *)&F32(c, 0x5a0), D(..)): the first choice record (the comma inside F32() hides it from above)
    for m in re.finditer(r"strcpy\(\s*\(char \*\)&F32\(\s*c\s*,\s*0x5a0\s*\)\s*,[^;]*?\b[DS]\((0x[0-9a-fA-F]{6})u?\)", src):
        va = int(m.group(1), 16)
        if cap.get(va, 0) is None and sum(1 for _ in re.finditer(r"\(\s*[^;]*?,[^;]*?\b[DS]\(0x%06x" % va, src)) == 1:
            cap[va] = 0x50 - 1
    return dlit, cap


def cap_of(va, rows, caps):
    """the byte cap of a string (0 = any length)"""
    if va not in caps:
        return 0
    return caps[va] if caps[va] is not None else int(rows[va]["bytes"])


def esc(t):
    t = t.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r")
    # C1 bytes (0x80..0x9f: an SJIS lead such as the arrows' 0x81) have no character of their own: write \xHH
    return re.sub(r"[\x80-\x9f]", lambda m: "\\x%02x" % ord(m.group()), t)


def unesc(t):
    out, i = [], 0
    while i < len(t):
        if t[i] == "\\" and i + 3 < len(t) + 0 and t[i + 1] == "x" and re.fullmatch(r"[0-9a-fA-F]{2}", t[i + 2:i + 4]):
            out.append(chr(int(t[i + 2:i + 4], 16)))
            i += 4
        elif t[i] == "\\" and i + 1 < len(t):
            out.append({"n": "\n", "r": "\r", "t": "\t"}.get(t[i + 1], t[i + 1]))
            i += 2
        else:
            out.append(t[i])
            i += 1
    return "".join(out)


def cp1252_len(t):
    return len(t.encode("cp1252", errors="replace"))


def problems(en, pt, cap):
    """what's wrong with a translation (empty list = fine)"""
    p = []
    if sorted(KEEP_RE.findall(en)) != sorted(KEEP_RE.findall(pt)):
        p.append(f"codes differ: en {sorted(KEEP_RE.findall(en))} pt {sorted(KEEP_RE.findall(pt))}")
    bad = sorted({ch for ch in pt if ord(ch) > 0xff and ch not in CP1252_EXTRA})
    if bad:
        p.append("characters the game can't draw: " + "".join(bad))
    if cap and cp1252_len(pt) > cap:
        p.append(f"too long: {cp1252_len(pt)} bytes, the buffer holds {cap}")
    if pt.strip() == "":
        p.append("empty")
    return p


def read_tsv(path):
    """va -> (cap, text) from a translation TSV (VA <TAB> MAX <TAB> TEXT)"""
    out = {}
    for line in open(path, encoding="utf-8-sig"):
        line = line.rstrip("\n").rstrip("\r")
        if not line or line.startswith("#"):
            continue
        parts = line.split("\t", 2)
        if len(parts) != 3:
            continue
        out[int(parts[0], 16)] = (int(parts[1] or 0), unesc(parts[2]))
    return out
