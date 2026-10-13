#!/usr/bin/env bash
# Copies the jskidpix art + sounds into assets/ (loaded from the SD/USB app folder at runtime).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="${1:-$ROOT/ref/kidpix}"
rm -rf "$ROOT/assets"; mkdir -p "$ROOT/assets/img" "$ROOT/assets/snd/english"
cp "$SRC"/img/*.png "$ROOT/assets/img/"
cp "$SRC"/snd/*.wav "$ROOT/assets/snd/"
cp "$SRC"/snd/english/*.wav "$ROOT/assets/snd/english/"
du -sh "$ROOT/assets"
