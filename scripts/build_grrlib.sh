#!/usr/bin/env bash
# Builds GRRLIB into ./portlibs/ppc (same layout as build_controller_portlibs.sh).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PFX="$ROOT/portlibs/ppc"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
git clone --quiet --depth 1 https://github.com/GRRLIB/GRRLIB "$WORK/GRRLIB"
make -C "$WORK/GRRLIB/GRRLIB" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu)"
mkdir -p "$PFX/lib" "$PFX/include/grrlib"
cp "$WORK/GRRLIB/GRRLIB/GRRLIB/libgrrlib.a" "$PFX/lib/"
cp "$WORK/GRRLIB/GRRLIB/GRRLIB/grrlib.h" "$PFX/include/"
cp "$WORK/GRRLIB/GRRLIB/GRRLIB/grrlib/"*.h "$PFX/include/grrlib/"
echo "Built GRRLIB in $PFX"
