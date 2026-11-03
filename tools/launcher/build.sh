#!/usr/bin/env bash
# tools/launcher/build.sh — the co-op launcher: build/launcher/CoopSummoners.exe.
#   --package   also build/launcher/CoopSummoners.zip = the launcher + build/rhost/opensummoners-standalone.exe
#               + LEIA-ME / README text (build the mod first: bash tools/rhost/build.sh).  No game files.
#   nix develop .#ci --command bash tools/launcher/build.sh [--package]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="$ROOT/tools/launcher"
OUT="$ROOT/build/launcher"
mkdir -p "$OUT"

(cd "$SRC" && i686-w64-mingw32-windres launcher.rc -O coff -o "$OUT/launcher.res")
i686-w64-mingw32-gcc -std=gnu11 -O2 -Wall -Wextra -municode -mwindows -static -s \
    -o "$OUT/CoopSummoners.exe" "$SRC/launcher.c" "$OUT/launcher.res" \
    -lcomctl32 -lshell32 -lole32 -luuid
echo "built $OUT/CoopSummoners.exe"

[ "${1:-}" = "--package" ] || exit 0
MOD="$ROOT/build/rhost/opensummoners-standalone.exe"
[ -f "$MOD" ] || { echo "build the mod first: bash tools/rhost/build.sh" >&2; exit 1; }
PKG="$OUT/pkg"
rm -rf "$PKG" "$OUT/CoopSummoners.zip"
mkdir -p "$PKG"
cp "$OUT/CoopSummoners.exe" "$MOD" "$PKG/"
cp "$SRC/README-package.txt" "$PKG/README.txt"
(cd "$PKG" && zip -q -9 "$OUT/CoopSummoners.zip" CoopSummoners.exe opensummoners-standalone.exe README.txt)
echo "packaged $OUT/CoopSummoners.zip"
