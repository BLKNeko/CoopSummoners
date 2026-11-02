#!/usr/bin/env python3
"""trace_p2.py — derive a co-op test trace from a recorded one.

The recorder writes each arrow together with its numpad twin (e.g. [75, 203] = numpad 4 + Left), and the co-op P2
keyboard map is the numpad, so with --coop a recorded trace would move P2 along with P1.  This strips the numpad
scancodes (P1 is unchanged: the arrows remain) and overlays P2 holds.

  trace_p2.py IN OUT [--hold FRAME:LEN:DIK ...] [--end FRAME]
"""
import argparse, json

NUMPAD = {0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("src"); ap.add_argument("dst")
    ap.add_argument("--hold", action="append", default=[], help="FRAME:LEN:DIK (DIK hex or dec)")
    ap.add_argument("--end", type=int, help="stop the trace at this frame")
    a = ap.parse_args()
    head, levels, end = [], [], None
    for line in open(a.src):
        s = line.strip()
        if not s.startswith("{") or '"frame"' not in s:
            head.append(line.rstrip("\n")); continue
        d = json.loads(s)
        if d.get("end"):
            end = d["frame"]; continue
        levels.append((d["frame"], [k for k in d["keys"] if k not in NUMPAD]))
    if a.end is not None:
        end = a.end
        levels = [l for l in levels if l[0] < end]
    holds = []
    for h in a.hold:
        f, n, k = h.split(":")
        holds.append((int(f), int(f) + int(n), int(k, 0)))
    points = sorted({f for f, _ in levels} | {h[0] for h in holds} | {h[1] for h in holds})
    out, li, cur, last = [], 0, [], None
    for f in points:
        if end is not None and f >= end: break
        while li < len(levels) and levels[li][0] <= f:
            cur = levels[li][1]; li += 1
        keys = list(cur) + [k for s, e, k in holds if s <= f < e and k not in cur]
        if keys != last:
            out.append({"frame": f, "keys": keys}); last = keys
    with open(a.dst, "w") as o:
        for l in head: o.write(l + "\n")
        o.write(f"# trace_p2.py: numpad stripped; P2 holds {a.hold}\n")
        for d in out: o.write(json.dumps(d, separators=(",", ":")) + "\n")
        o.write(json.dumps({"frame": end, "keys": [], "end": 1}, separators=(",", ":")) + "\n")

if __name__ == "__main__":
    main()
