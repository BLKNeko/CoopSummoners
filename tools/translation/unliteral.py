#!/usr/bin/env python3
"""unliteral.py — point the port's inlined game strings back at the game's data, so translations reach them.

The decompile sometimes inlined a string of sotes.exe's data as a C literal ("The paper on the wall says '%s'.").
The port then never reads that VA, so --lang can't translate it.  This rewrites such a literal as
((const char *)D(0xVA)): with no translation D() returns the same bytes (identity), so the game is unchanged.
Only literals that (a) equal exactly one translatable string of the game, (b) have a translation in
data/lang/<lang>.tsv that differs, (c) sit in a file that defines D(), outside comments.  Re-run merge.py after
it: the new D() references can bring byte caps (strcpy into fixed buffers).

    python3 tools/translation/unliteral.py [--lang pt-br] [--dry-run]
"""
import argparse
import collections
import glob
import os
import re

from common import LANG_DIR, ROOT, load_strings, read_tsv

LIT = re.compile(r'(?<![\\\w])"((?:[^"\\\n]|\\.)*)"')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", default="pt-br")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    rows = load_strings()
    tr = read_tsv(os.path.join(LANG_DIR, a.lang + ".tsv"))
    by_text = collections.defaultdict(list)
    for va, r in rows.items():
        if r["category"] in ("dialogue", "label"):
            by_text[r["text"]].append(va)

    total = 0
    for f in sorted(glob.glob(os.path.join(ROOT, "src", "*.c"))):
        src = open(f, encoding="utf-8", errors="replace", newline="").read()
        if not re.search(r"#define D\(|static const (?:void|char) \*D\(", src):
            continue
        out, pos, n = [], 0, 0
        for m in LIT.finditer(src):
            try:
                txt = bytes(m.group(1), "latin-1").decode("unicode_escape")
            except (UnicodeDecodeError, UnicodeEncodeError):
                continue
            vas = by_text.get(txt, [])
            if len(txt) < 3 or len(vas) != 1 or (vas[0] in tr and tr[vas[0]][1] == txt):
                continue   # not one game string, or deliberately kept in English
            if re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", txt) and sum(ch.isupper() for ch in txt) >= 2:
                continue   # an identifier (API / function names for GetProcAddress): never translated
            ls = src.rfind("\n", 0, m.start()) + 1
            before = src[ls:m.start()]
            if "/*" in before and "*/" not in before[before.rfind("/*"):] or "//" in before:
                continue   # inside a comment
            if re.search(r"#\s*define|\bapi\s*\(|iat_lookup|GetProcAddress|LoadLibrary|GetModuleHandle|lstrcmp|strcmp|"
                         r"FindResource|RegQuery|RegOpen|fopen|CreateFile", before):
                continue   # a name the code looks up or compares: must stay the game's own bytes
            if a.dry_run:
                print(f"  {os.path.basename(f)}:{src.count(chr(10), 0, m.start()) + 1}: 0x{vas[0]:06x} {txt[:70]!r}")
            out.append(src[pos:m.start()])
            out.append(f"((const char *)D(0x{vas[0]:06x}))")
            pos = m.end()
            n += 1
        if n:
            out.append(src[pos:])
            total += n
            print(f"{os.path.relpath(f, ROOT)}: {n}")
            if not a.dry_run:
                open(f, "w", encoding="utf-8", newline="").write("".join(out))
    print(f"{total} literals {'would be ' if a.dry_run else ''}rewritten")


if __name__ == "__main__":
    main()
