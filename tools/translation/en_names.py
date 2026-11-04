#!/usr/bin/env python3
"""en_names.py — the "keep original names" overlay: data/lang/<lang>.en-names.tsv.

With the option on, the game loads <lang>.tsv and this overlay instead of <lang>.names.tsv: the tables' names
(items, spells, monsters, places) stay as the game has them, and every translated sentence that names one gets it
back in English ("Você aprendeu Água Curativa!" -> "Você aprendeu Heal Water!").  A name is put back only where the
English original names it too, in the casing the original uses there (shouted spells stay in capitals).

The name pairs: every translated table name (<lang>.names.tsv) + the names of the glossary
(tools/translation/glossary-pt-br.md: the spell table, and the place / item / creature rows of the terms table
listed in GLOSSARY_NAMES).  merge.py and review.py rewrite the overlay after each change; by hand:

    python3 tools/translation/en_names.py [--lang pt-br]
"""
import argparse
import os
import re

from common import HEAD, LANG_DIR, ROOT, cp1252_len, esc, load_strings, read_tsv

# rows of the glossary's terms table that are names of places, items or creatures (the rest are common words)
GLOSSARY_NAMES = {
    "Town of Tonkiness", "Weathervane Tower", "Korat Village / Barness Village", "Chartreux City",
    "Chartreux Catacombs / Catacombs", "Chartreux Guardpost", "Loch Carudale", "Mayberk Manor",
    "Minasa-Ratis Magic School / Magic School", "North Gate", "First Street", "Warm Breeze Inn", "Wind Hill",
    "Topah Herb / Herb", "Mentama Candy / Candy", "Mark of Heroism", "Black Steel", "Grand Gourmand",
    "Myconid", "Mothbee", "Merkid(s) / merfolk",
}
# the short halves of those rows are common words in a sentence ("as catacumbas", "uma erva"): not names
NOT_NAMES = {"Catacombs", "Herb", "Candy", "Magic School", "merfolk"}


def glossary_pairs():
    """(en, pt) name pairs from the glossary"""
    path = os.path.join(ROOT, "tools", "translation", "glossary-pt-br.md")
    out, section = [], ""
    for line in open(path, encoding="utf-8"):
        if line.startswith("## "):
            section = line[3:].strip()
            continue
        m = re.match(r"^\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|\s*$", line)
        if not m or m.group(1) in ("Inglês", "---"):
            continue
        en, pt = m.group(1), m.group(2)
        if not (section.startswith("Magias") or section == "Termos" and en in GLOSSARY_NAMES):
            continue
        en = re.sub(r"\s*\([^)]*\)", "", en)
        pt = re.sub(r"\s*\([^)]*\)", "", pt)
        es, ps = [x.strip() for x in en.split(" / ")], [x.strip() for x in pt.split(" / ")]
        if len(es) == len(ps):
            out += [(e, p) for e, p in zip(es, ps) if e and p and e != p and e not in NOT_NAMES]
    return out


def put_back(en_text, pt_text, pairs):
    """pt_text with every pair's PT name that en_text names in English put back in English (None: nothing to do)"""
    out, hits = pt_text, 0
    for en, pt in pairs:
        for m in list(re.finditer(re.escape(en), en_text, re.I))[:1]:
            orig = m.group(0)   # as the English writes it there (Diamonddust / Diamond Dust)
            base = orig if orig[:1].isupper() and not orig.isupper() else en

            def repl(mm, base=base):   # shouted in Portuguese -> shouted in English; a lower-case word isn't a name
                s = mm.group(0)
                if not s[:1].isupper():
                    return s
                return base.upper() if s.isupper() and len(s) > 1 else base
            new = re.sub(r"(?<![\w])" + re.escape(pt) + r"(?![\w])", repl, out, flags=re.I)
            if new != out:
                out, hits = new, hits + 1
    return out if hits else None


def write_overlay(lang, main, names, rows):
    """main / names: va -> (MAX field as written, text) of <lang>.tsv / <lang>.names.tsv"""
    pairs = {(rows[va]["text"], t) for va, (_, t) in names.items() if va in rows and rows[va]["text"] != t}
    pairs |= set(glossary_pairs())
    pairs = sorted(pairs, key=lambda p: -len(p[1]))   # "Água Curativa II" before "Água Curativa"
    over = {}
    for va, (mx, t) in main.items():
        if va not in rows:
            continue
        new = put_back(rows[va]["text"], t, pairs)
        cap = int(mx.lstrip("@") or 0)
        if new is not None and new != t and (not cap or cp1252_len(new) <= cap):
            over[va] = (mx, new)
    path = os.path.join(LANG_DIR, lang + ".en-names.tsv")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEAD.format(lang=lang, what=", overlay: the names kept in English"))
        for va in sorted(over):
            f.write(f"0x{va:06x}\t{over[va][0]}\t{esc(over[va][1])}\n")
    return len(over)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", default="pt-br")
    a = ap.parse_args()

    def raw(path):   # va -> (MAX as written, text)
        out = {}
        if os.path.exists(path):
            for line in open(path, encoding="utf-8-sig"):
                p = line.rstrip("\r\n").split("\t", 2)
                if len(p) == 3 and not line.startswith("#"):
                    out[int(p[0], 16)] = (p[1], read_one(p[2]))
        return out

    def read_one(t):
        from common import unesc
        return unesc(t)

    rows = load_strings()
    main_ = raw(os.path.join(LANG_DIR, a.lang + ".tsv"))
    names = raw(os.path.join(LANG_DIR, a.lang + ".names.tsv"))
    n = write_overlay(a.lang, main_, names, rows)
    print(f"{n} strings in {os.path.join(LANG_DIR, a.lang + '.en-names.tsv')}")


if __name__ == "__main__":
    main()
