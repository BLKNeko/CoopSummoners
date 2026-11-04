#!/usr/bin/env python3
"""make_batches.py — split the translatable strings into batches for translating.

Phase 1 = the strings the port reaches through D(0x...) (see src/lang.h): the dialogue and many menu words.
Strings copied into fixed buffers get a byte cap = their English length (the buffer is at least that big).
Batches keep the game's order (VA order = scene order), so each batch carries its own context.

    python3 tools/translation/make_batches.py [--size 200] [--lang pt-br]
writes build/translation/batches/NNN.tsv (VA, CAP, KIND, EN) — local only: English text — skipping VAs already
translated in data/lang/<lang>.tsv.
"""
import argparse
import os

from common import LANG_DIR, WORK, esc, load_strings, port_refs, read_tsv


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--size", type=int, default=200)
    ap.add_argument("--lang", default="pt-br")
    a = ap.parse_args()

    rows = load_strings()
    dlit, cap = port_refs()
    done = {}
    lp = os.path.join(LANG_DIR, a.lang + ".tsv")
    if os.path.exists(lp):
        done = read_tsv(lp)
    todo = [va for va in sorted(rows) if rows[va]["category"] in ("dialogue", "label") and va in dlit and va not in done]

    out = os.path.join(WORK, "batches")
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        if f.endswith(".tsv"):
            os.remove(os.path.join(out, f))
    n = 0
    for i in range(0, len(todo), a.size):
        n += 1
        with open(os.path.join(out, f"{n:03d}.tsv"), "w", encoding="utf-8", newline="\n") as f:
            f.write("# VA\tCAP\tKIND\tEN   (CAP = max bytes, 0 = any length; KIND = dialogue / label)\n")
            for va in todo[i:i + a.size]:
                r = rows[va]
                c = int(r["bytes"]) if va in cap else 0
                f.write(f"0x{va:06x}\t{c}\t{r['category']}\t{esc(r['text'])}\n")
    words = sum(int(rows[va]["words"]) for va in todo)
    print(f"{len(todo)} strings ({words} words) in {n} batches of <= {a.size} -> {out}"
          f"  ({len(done)} already in {lp if done else 'no translation file yet'})")


if __name__ == "__main__":
    main()
