#!/usr/bin/env python3
"""merge.py — check translated batches and merge them into data/lang/<lang>[.names].tsv.

Input: build/translation/out/*.tsv, lines "VA <TAB> TEXT" (the translator's output for a batch).
Each line is checked against the English original (common.problems): the game's codes (%s %d %mN% %w @@KEY #N)
kept, the byte cap (a fixed buffer the port copies it into, or the table field it is written into in place), only
characters the game can draw (Windows-1252).  Good lines go into the language's files (common.write_lang: the table
names apart in <lang>.names.tsv, then the en-names overlay); bad ones are listed in
build/translation/merge-problems.txt and left out.  A spell's shouted name (SPELL NAME in capitals) follows its
translated name when the English is exactly the name in capitals.

    python3 tools/translation/merge.py [--lang pt-br] [files...]
"""
import argparse
import glob
import os

from common import (LANG_DIR, WORK, cp1252_len, entry_cap, load_strings, load_tables, port_refs, problems, read_lang,
                    unesc, write_lang)

# typographic marks the original text never uses -> its plain style
PLAIN = {"“": '"', "”": '"', "‘": "'", "’": "'", "…": "...", "—": "-", "–": "-", " ": " "}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", default="pt-br")
    ap.add_argument("files", nargs="*")
    a = ap.parse_args()
    files = a.files or sorted(glob.glob(os.path.join(WORK, "out", "*.tsv")))

    rows = load_strings()
    _, capped = port_refs()
    tables = load_tables()
    cur = read_lang(a.lang)

    good, bad = 0, []
    for fn in files:
        for n, line in enumerate(open(fn, encoding="utf-8-sig"), 1):
            line = line.rstrip("\n").rstrip("\r")
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            try:
                va = int(parts[0], 16)
            except ValueError:
                bad.append(f"{os.path.basename(fn)}:{n}: no VA: {line[:80]}")
                continue
            pt = unesc(parts[-1])
            for k, v in PLAIN.items():
                pt = pt.replace(k, v)
            if va not in rows:
                bad.append(f"{os.path.basename(fn)}:{n}: 0x{va:06x} is not a string of the game")
                continue
            en = rows[va]["text"]
            cap = entry_cap(va, rows, capped, tables)[1]
            p = problems(en, pt, cap)
            if p:
                bad.append(f"{os.path.basename(fn)}:{n}: 0x{va:06x}: " + "; ".join(p) + f"\n    EN: {en}\n    PT: {pt}")
                continue
            cur[va] = (cap, pt)
            good += 1

    # shouted spell names = the translated name in capitals (when it fits)
    shouts = 0
    for va, t in tables.items():
        src = t["upper_of"]
        if src is not None and src in cur:
            up = cur[src][1].upper()
            if cp1252_len(up) <= t["cap"]:
                cur[va] = (t["cap"], up)
                shouts += 1

    bad += write_lang(a.lang, cur, rows, capped, tables)
    pp = os.path.join(WORK, "merge-problems.txt")
    open(pp, "w", encoding="utf-8").write("\n".join(bad) + "\n")
    print(f"merged {good} lines from {len(files)} files (+{shouts} shouted spell names); {len(cur)} strings in "
          f"{os.path.join(LANG_DIR, a.lang)}[.names].tsv; {len(bad)} problems" + (f" -> {pp}" if bad else ""))


if __name__ == "__main__":
    main()
