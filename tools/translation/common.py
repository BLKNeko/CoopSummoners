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
KEEP_RE = re.compile(r"%[-+ #0]*\d*(?:\.\d+)?[sdiuxXcf]|%m\d*%?|%w|@@[A-Za-z0-9]+|#[0-9]")
# %n = a manual line break: a translation may move or drop it
BREAK = "%n"

CP1252_EXTRA = set("€‚ƒ„…†‡ˆ‰Š‹ŒŽ‘’“”•–—˜™š›œžŸ")


def load_strings():
    """strings.csv (extract_text.py) as a dict va -> row"""
    path = os.path.join(WORK, "strings.csv")
    if not os.path.exists(path):
        raise SystemExit(f"{path} is missing: run tools/translation/extract_text.py <game dir> first")
    return {int(r["va"], 16): r for r in csv.DictReader(open(path, encoding="utf-8"))}


def port_refs():
    """(VAs used as D(0x...) in src/, VAs copied into fixed buffers by strcpy / sprintf / ...)"""
    src = "".join(open(f, encoding="utf-8", errors="replace").read() for f in glob.glob(os.path.join(ROOT, "src", "*.c")))
    dlit = {int(x, 16) for x in re.findall(r"D\((0x[0-9a-fA-F]{6})\)", src)}
    cap = {int(x, 16) for x in re.findall(
        r"(?:strcpy|strcat|sprintf|wsprintfA|lstrcpyA|strncpy)\w*\([^;]*?D\((0x[0-9a-fA-F]{6})\)", src)}
    return dlit, cap


def esc(t):
    return t.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r")


def unesc(t):
    out, i = [], 0
    while i < len(t):
        if t[i] == "\\" and i + 1 < len(t):
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
