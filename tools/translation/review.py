#!/usr/bin/env python3
"""review.py — a side-by-side sheet for reviewing a translation, and reading the edits back.

    python3 tools/translation/review.py export [--lang pt-br]
        build/translation/review-<lang>.csv: VA, EN, PT, STATUS, NOTE (UTF-8 with BOM: opens right in Excel /
        LibreOffice).  Local only: it holds the game's English text.
    python3 tools/translation/review.py import [--lang pt-br] [csv]
        reads the PT column back (rows whose text changed), checks each edit like merge.py, and updates
        data/lang/<lang>.tsv.  STATUS / NOTE are yours: kept in the sheet, ignored here.
"""
import argparse
import csv
import os

from common import LANG_DIR, WORK, esc, load_strings, problems, read_tsv


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=("export", "import"))
    ap.add_argument("--lang", default="pt-br")
    ap.add_argument("csv", nargs="?")
    a = ap.parse_args()
    lp = os.path.join(LANG_DIR, a.lang + ".tsv")
    cur = read_tsv(lp)
    rows = load_strings()
    sheet = a.csv or os.path.join(WORK, f"review-{a.lang}.csv")

    if a.mode == "export":
        old = {}
        if os.path.exists(sheet):   # keep the reviewer's STATUS / NOTE across exports
            for r in csv.DictReader(open(sheet, encoding="utf-8-sig")):
                old[r["VA"]] = (r.get("STATUS", ""), r.get("NOTE", ""))
        with open(sheet, "w", encoding="utf-8-sig", newline="") as f:
            w = csv.writer(f)
            w.writerow(["VA", "EN", "PT", "STATUS", "NOTE"])
            for va in sorted(cur):
                key = f"0x{va:06x}"
                st, note = old.get(key, ("", ""))
                w.writerow([key, rows[va]["text"] if va in rows else "", cur[va][1], st, note])
        print(f"{len(cur)} rows -> {sheet}")
        return

    changed, bad = 0, []
    for r in csv.DictReader(open(sheet, encoding="utf-8-sig")):
        va = int(r["VA"], 16)
        if va not in cur or r["PT"] == cur[va][1]:
            continue
        p = problems(rows[va]["text"], r["PT"], cur[va][0])
        if p:
            bad.append(f"{r['VA']}: " + "; ".join(p))
            continue
        cur[va] = (cur[va][0], r["PT"])
        changed += 1
    head = [l for l in open(lp, encoding="utf-8") if l.startswith("#")]
    with open(lp, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(head)
        for va in sorted(cur):
            f.write(f"0x{va:06x}\t{cur[va][0]}\t{esc(cur[va][1])}\n")
    print(f"{changed} edits applied to {lp}" + (f"; {len(bad)} rejected:\n  " + "\n  ".join(bad) if bad else ""))


if __name__ == "__main__":
    main()
