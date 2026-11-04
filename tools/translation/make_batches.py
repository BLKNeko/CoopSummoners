#!/usr/bin/env python3
"""make_batches.py — split the translatable strings into batches for translating.

Phase 1 = the strings the port reaches through D(0x...) (see src/lang.h): the dialogue and many menu words;
phase 2 = the table strings (tables.py: names, descriptions, letters, books), once build/translation/tables.csv exists.
Strings copied into fixed buffers get a byte cap = their English length (the buffer is at least that big).
Batches keep the game's order (VA order = scene order), so each batch carries its own context.

    python3 tools/translation/make_batches.py [--size 200] [--lang pt-br]
writes build/translation/batches/NNN.tsv (VA, CAP, KIND, EN) — local only: English text — skipping VAs already
translated in data/lang/<lang>.tsv.
"""
import argparse
import os

from common import WORK, entry_cap, esc, load_strings, load_tables, port_refs, read_lang


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=200)
    ap.add_argument("--lang", default="pt-br")
    a = ap.parse_args()

    rows = load_strings()
    dlit, cap = port_refs()
    tables = load_tables()   # phase 2: the table strings (tables.py), written in place
    done = read_lang(a.lang)
    want = dlit | {va for va, t in tables.items() if t["upper_of"] is None}   # (shouted names follow their name)
    todo = [va for va in sorted(rows) if va not in done and va in want
            and (rows[va]["category"] in ("dialogue", "label") or va in tables)]

    out = os.path.join(WORK, "batches")
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        if f.endswith(".tsv"):
            os.remove(os.path.join(out, f))
    n = 0
    for i in range(0, len(todo), a.size):
        n += 1
        with open(os.path.join(out, f"{n:03d}.tsv"), "w", encoding="utf-8", newline="\n") as f:
            f.write("# VA\tCAP\tKIND\tEN   (CAP = max bytes, 0 = any length; KIND = dialogue / label / a table"
                    " column: item name, spell desc, ...)\n")
            for va in todo[i:i + a.size]:
                r = rows[va]
                c = entry_cap(va, rows, cap, tables)[1]
                kind = tables[va]["kind"] if va in tables else r["category"]
                f.write(f"0x{va:06x}\t{c}\t{kind}\t{esc(r['text'])}\n")
    words = sum(int(rows[va]["words"]) for va in todo)
    print(f"{len(todo)} strings ({words} words) in {n} batches of <= {a.size} -> {out}"
          f"  ({len(done)} already translated)")


if __name__ == "__main__":
    main()
