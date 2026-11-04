#!/usr/bin/env python3
"""tables.py — the game's text TABLES (phase 2 of a translation): strings the code reads at base + row * stride.

The port reaches a table through its base (D(0x6c0cd0)[184 * id]: the item table), never through the string's own
VA, so the D() mapping of phase 1 can't replace them: a translation writes them IN PLACE into the mapped data image
before the game starts (src/lang.h, '@' caps).  In place = the text must fit the string's field: its cap is
min(field size - 1, the bytes up to the next non-zero byte after the English string) — the field size from the
row layouts (docs/findings/exe-data-interpreted.md table D / T5, the dramatist proof), the gap guarding a field
that holds a second string or data inside the declared size.

Group "names" = the names a player may prefer in English (items, spells / moves, monsters, places): the launcher's
"keep original names" option loads the translation without them (see en_names.py for the names inside sentences).

    python3 tools/translation/tables.py <game dir or sotes.exe>
writes build/translation/tables.csv: va, cap, group, kind, upper_of (the VA of the name a shouted SPELL NAME is the
upper case of, when the English is exactly that).
"""
import csv
import os
import sys

from common import WORK, load_strings
from extract_text import pe_sections

# (kind, group, first string VA, row stride, last VA (inclusive bound), field size)
COLUMNS = [
    ("status name", "text", 0x628798, 0x8c, 0x629058, 0x50),        # status rows 0x628708 (140 B): name +0x90
    ("monster name", "names", 0x67ad6e, 0xdc, 0x682600, 0x50),      # base stats 0x67ac58 (220 B): name +0x116
    ("monster name 2", "names", 0x67adbe, 0xdc, 0x682600, 0x50),    #   second name +0x166 (the gap bounds it)
    ("spell name", "names", 0x6826c4, 0x300, 0x68f700, 0x50),       # skills 0x6826c0 (768 B): name +4
    ("spell shout", "names", 0x682714, 0x300, 0x68f700, 0x70),      #   name in capitals +0x54 (cast caption)
    ("spell desc", "text", 0x682784, 0x300, 0x68f800, 0x200),       #   description +0xc4
    ("area name", "names", 0x69384c, 0x40, 0x694100, 0x28),         # AREA 0x693848 (64 B): name +4
    ("speaker", "text", 0x6b6eb0, 0x34, 0x6b7ee0, 0x20),            # dramatist 0x6b6ea8 (52 B): name +8 (+0x28 next)
    ("letter title", "text", 0x6b7efc, 0x4bc, 0x6c0cc0, 0x80),      # messages 0x6b7ee8 (1212 B): title +0x14
    ("letter body", "text", 0x6b7f7c, 0x4bc, 0x6c0cc0, 0x428),      #   body +0x94
    ("item name", "names", 0x6c0cd4, 0x2e0, 0x6e7a00, 0x50),        # items 0x6c0cd0 (736 B): name +4
    ("item desc", "text", 0x6c0d7c, 0x2e0, 0x6e7a00, 0x200),        #   description +0xac
]
# loose strings read from a table / pointer the port doesn't name (cap = the gap)
SINGLES = [
    ("letter sign", "text", 0x6bdd70, 0x80),     # "- Chartreux Merchant Guild": inside the ad letter's body field
    ("inn name", "text", 0x6a0810, 0x38),
    ("spellbook help", "text", 0x8a3258, 0x100),
]
# books (W_book_read): {title [0x28], 40 pages [0x804]}; a title is a string 0x28 before a page
BOOKS = (0x6e7f50, 0x850000)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    src = sys.argv[1]
    exe = os.path.join(src, "sotes.exe") if os.path.isdir(src) else src
    data = open(exe, "rb").read()
    _, secs = pe_sections(data)

    def rd(va, n):
        for _, sva, vs, raw, rs in secs:
            if sva <= va < sva + max(vs, rs):
                o = va - sva
                b = data[raw + o:raw + min(o + n, rs)]
                return b + b"\0" * (n - len(b))
        raise KeyError(hex(va))

    def gap(va):   # the bytes up to the next non-zero byte after the string, less its NUL
        b = rd(va, 0x1000)
        i = b.index(b"\0")
        while i < len(b) and b[i] == 0:
            i += 1
        return i - 1

    rows = load_strings()
    # the letter titles follow a non-zero dword (the author's handle), so extract_text (strings after a NUL) misses
    # them: add them to strings.csv
    added = []
    for i in range(29):
        va = 0x6b7efc + 0x4bc * i
        b = rd(va, 0x80)
        s = b[:b.index(b"\0")]
        if len(s) >= 2 and va not in rows and all(0x20 <= c < 0x7f for c in s):
            t = s.decode("latin-1")
            rows[va] = {"va": f"0x{va:06x}", "section": ".rdata", "category": "label", "bytes": str(len(s)),
                        "words": str(len(t.split())), "text": t}
            added.append(rows[va])
    if added:
        with open(os.path.join(WORK, "strings.csv"), "a", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=["va", "section", "category", "bytes", "words", "text"])
            w.writerows(added)
    text = {va for va, r in rows.items() if r["category"] in ("dialogue", "label")}
    out = {}

    def add(va, kind, group, field, any_kind=False):
        if (va in text or any_kind and va in rows) and len(rows[va]["text"]) >= 2 and va not in out:
            out[va] = [min(field - 1, gap(va)), group, kind, ""]

    for kind, group, first, stride, last, field in COLUMNS:
        va = first
        while va <= last:
            add(va, kind, group, field)
            va += stride
    for kind, group, va, field in SINGLES:
        add(va, kind, group, field)
    for va in sorted(rows):   # (any category: "Magical Girl Merrin Part 1/12" reads as 'tech' to extract_text)
        if BOOKS[0] <= va < BOOKS[1] and va not in out:
            if va + 0x28 in text and "%n" not in rows[va]["text"] and len(rows[va]["text"]) < 0x28:
                add(va, "book title", "text", 0x28, any_kind=True)
                p = va + 0x28
                while p < BOOKS[1] and p - va < 0x28 + 40 * 0x804:
                    add(p, "book page", "text", 0x800)
                    p += 0x804
    # a shouted spell name that is exactly the capitals of its row's name: derived from the name's translation
    for va, e in out.items():
        if e[2] == "spell shout" and va - 0x50 in out and rows[va]["text"] == rows[va - 0x50]["text"].upper():
            e[3] = f"0x{va - 0x50:06x}"

    path = os.path.join(WORK, "tables.csv")
    with open(path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["va", "cap", "group", "kind", "upper_of"])
        for va in sorted(out):
            w.writerow([f"0x{va:06x}"] + out[va])
    kinds = {}
    for e in out.values():
        kinds[e[2]] = kinds.get(e[2], 0) + 1
    print(f"{len(out)} table strings -> {path}")
    for k, n in kinds.items():
        print(f"  {k:15} {n}")


if __name__ == "__main__":
    main()
