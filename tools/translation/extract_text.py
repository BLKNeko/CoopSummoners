#!/usr/bin/env python3
"""extract_text.py — list and count the text of Fortune Summoners (2012 EN, sotes.exe) for a translation.

The EN 2012 edition bakes its text into sotes.exe: NUL-terminated strings in .data (dialogue) and .rdata (names,
menus, item / spell / monster tables).  .rsrc holds none (checked: every "Arche" hit is in .data / .rdata).  SteamStub
encrypts .text only, so the data sections read as they are from the stock Steam exe.

This tool reads YOUR copy of sotes.exe and writes, into OUT (default build/translation/, git-ignored):
  strings.csv   every string found: va, section, category, length, words, text  (game text: never commit / share)
  summary.txt   the counts per category + samples
It changes nothing.  The categories are heuristics (see classify); `tech` and `jp` are not meant for translation.

    python3 tools/translation/extract_text.py <game dir or sotes.exe> [OUT]
"""
import csv
import hashlib
import os
import re
import struct
import sys
from collections import Counter, defaultdict

SUPPORTED_SHA256 = "7d779f2eb02b3c603857fedbc52be6973ac3b0b2c5c1bc696122ddac89fb9f1b"
MIN_LEN = 2

# ---------------------------------------------------------------- PE sections (no pefile dependency)


def pe_sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe:pe + 4] == b"PE\0\0", "not a PE file"
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    opt = struct.unpack_from("<H", data, pe + 20)[0]
    base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    out = []
    for i in range(nsec):
        o = pe + 24 + opt + 40 * i
        name = data[o:o + 8].rstrip(b"\0").decode()
        vsize, va, rsize, raw = struct.unpack_from("<IIII", data, o + 8)
        out.append((name, base + va, vsize, raw, rsize))
    return base, out

# ---------------------------------------------------------------- classification

# substrings that mark engine / runtime / debug strings, not text a player reads
TECH_MARKERS = (
    "Microsoft", "Visual C++", "runtime", "Runtime", "R60", "DOMAIN error", "SING error", "TLOSS error",
    "DirectDraw", "DirectInput", "DirectSound", "Direct3D", "DDERR", "DIERR", "DSERR", "HRESULT",
    "failed", "Failed", "FAILED", "error code", "Error code", "Assert", "assert", "Exception", "exception",
    "Segment", "SetNext", "Initialize", "Release()", "CreateSurface", "GetDC", "Lock(", "Blt(", "Flip(",
    "0x%", "%08", "%x", "%p", "SOFTWARE\\", "Software\\", "HKEY", ".dll", ".DLL", ".exe", ".EXE", ".bmp", ".BMP",
    ".wav", ".WAV", ".wma", ".dat", ".sdt", ".tmp", ".log", ".txt", ".ini", ".htm", "\\", "/", "::", "__",
    "Lizsoft", "LIZSOFT", "CLASS_", "MS Sans", "MS Shell", "Courier New", "Times New Roman", "Arial",
)
TECH_RE = [
    re.compile(r"^[A-Za-z_][A-Za-z0-9_]*\(\)$"),           # Func()
    re.compile(r"^[a-z]+[A-Z][A-Za-z0-9]*$"),              # camelCase identifier
    re.compile(r"^(?:[A-Z][a-z0-9]+){2,}[A-Z]?[a-z0-9]*$"),  # CamelCase identifier with 2+ humps (SetNextSegment)
    re.compile(r"^[A-Z0-9_]{3,}$"),                        # CONSTANT_NAME / all-caps tokens
    re.compile(r"^[a-z0-9_]+$"),                           # lower-case identifiers / locale names
    re.compile(r"^[%\d\s.,:;+\-*/=<>()\[\]{}#|!?\"']+$"),     # formats / punctuation only
    re.compile(r"^[^A-Za-z]*$"),                           # no letters at all
]


def is_sjis_text(b):
    if not any(c >= 0x80 for c in b):
        return False
    try:
        s = b.decode("cp932")
    except UnicodeDecodeError:
        return False
    return any("　" <= ch <= "鿿" or "＀" <= ch <= "￯" for ch in s)


def classify(b):
    """jp / tech / dialogue (a sentence) / label (a short name or menu word) / junk"""
    if is_sjis_text(b):
        return "jp"
    if any(c >= 0x80 for c in b):
        return "junk"                                      # high bytes that aren't Japanese: binary data
    s = b.decode("ascii")
    t = s.strip()
    letters = sum(ch.isalpha() for ch in t)
    if letters < 2:
        return "junk"
    if letters / max(1, len(t)) < 0.5 and " " not in t:
        return "junk"                                      # mostly symbols / digits
    if any(m in t for m in TECH_MARKERS) or any(r.match(t) for r in TECH_RE):
        return "tech"
    words = re.findall(r"[A-Za-z']+", t)
    if len(words) >= 4 or re.search(r"[.!?…]$", t) and len(words) >= 2:
        return "dialogue"
    if t[0].isupper() or t[0] in "\"'([" or " " in t:
        return "label"
    return "junk"


def word_count(t):
    return len(re.findall(r"[A-Za-z][A-Za-z'\-]*", re.sub(CODE_RE, " ", t)))


# the game's codes inside text: printf fields (%s %d, %n = line break), @@KEY = the configured key's name, #N colours
CODE_RE = r"%[-+ #0]*\d*(?:\.\d+)?[sdiuxXcfn]|@@[A-Za-z0-9]+|#[A-Za-z0-9]|\n|\r"

# ---------------------------------------------------------------- main


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    src = sys.argv[1]
    exe = os.path.join(src, "sotes.exe") if os.path.isdir(src) else src
    root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(root, "build", "translation")
    os.makedirs(out, exist_ok=True)

    data = open(exe, "rb").read()
    sha = hashlib.sha256(data).hexdigest()
    base, secs = pe_sections(data)

    rows = []
    for name, va, vsize, raw, rsize in secs:
        if name not in (".data", ".rdata"):
            continue
        blob = data[raw:raw + min(vsize, rsize)]
        # a string = printable run (tab / newline allowed, plus high bytes for SJIS) ended by NUL, starting after a NUL
        for m in re.finditer(rb"(?<=\x00)([\x09\x0a\x0d\x20-\x7e\x80-\xfe]{%d,})\x00" % MIN_LEN, blob):
            b = m.group(1)
            cat = classify(b)
            text = b.decode("cp932", errors="replace") if cat == "jp" else b.decode("latin-1")
            rows.append((va + m.start(1), name, cat, len(b), word_count(text) if cat in ("dialogue", "label") else 0, text))

    with open(os.path.join(out, "strings.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["va", "section", "category", "bytes", "words", "text"])
        for va, sec, cat, n, wc, text in rows:
            w.writerow([f"0x{va:06x}", sec, cat, n, wc, text])

    by = defaultdict(list)
    for r in rows:
        by[r[2]].append(r)
    lines = []
    p = lines.append
    p(f"sotes.exe: {exe}")
    p(f"sha256 {sha} ({'supported 2012 EN build' if sha == SUPPORTED_SHA256 else 'NOT the supported build'})")
    p("")
    p(f"{'category':<10} {'strings':>8} {'unique':>8} {'words':>9} {'uniq.words':>10} {'chars':>9}  section split")
    for cat in ("dialogue", "label", "jp", "tech", "junk"):
        rs = by.get(cat, [])
        uniq = {}
        for r in rs:
            uniq.setdefault(r[5], r)
        words = sum(r[4] for r in rs)
        uwords = sum(r[4] for r in uniq.values())
        chars = sum(len(t) for t in uniq)
        split = Counter(r[1] for r in rs)
        p(f"{cat:<10} {len(rs):>8} {len(uniq):>8} {words:>9} {uwords:>10} {chars:>9}  "
          + ", ".join(f"{k} {v}" for k, v in sorted(split.items())))
    tr = [r for r in rows if r[2] in ("dialogue", "label")]
    tu = {r[5] for r in tr}
    p("")
    p(f"TO TRANSLATE (dialogue + label): {len(tr)} strings, {len(tu)} unique, "
      f"{sum(word_count(t) for t in tu)} words in unique strings, {sum(len(t) for t in tu)} characters")
    ph = Counter()
    for t in tu:
        for m in re.findall(CODE_RE, t):
            ph[m] += 1
    p("placeholders / control codes in those strings: " + (", ".join(f"{k!r} x{v}" for k, v in ph.most_common(15)) or "none"))
    lens = sorted(len(t) for t in tu)
    if lens:
        p(f"length of unique strings: median {lens[len(lens) // 2]}, 90% <= {lens[int(len(lens) * 0.9)]}, max {lens[-1]}")
    p("")
    for cat in ("dialogue", "label", "jp", "tech", "junk"):
        rs = by.get(cat, [])
        if not rs:
            continue
        p(f"--- samples: {cat}")
        step = max(1, len(rs) // 12)
        for r in rs[::step][:12]:
            p(f"  0x{r[0]:06x} {r[1]:<6} {r[5][:110]!r}")
    txt = "\n".join(lines)
    open(os.path.join(out, "summary.txt"), "w", encoding="utf-8").write(txt + "\n")
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    print(txt)
    print(f"\nwritten: {os.path.join(out, 'strings.csv')} and summary.txt (game text: keep local, don't commit)")


if __name__ == "__main__":
    main()
