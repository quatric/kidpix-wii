# Kid Pix for Wii

**Version:** 0.3.1 · **Coder:** quatric

A Wii homebrew port, written in C with [GRRLIB](https://github.com/GRRLIB/GRRLIB), of the
[jskidpix](https://github.com/vikrum/kidpix) HTML/JS recreation of Kid Pix 1.0
(with [KidPad](https://github.com/chrissotraidis/kidpad) as a second behaviour reference).
Drawing is done by a small software rasteriser into a 600x408 canvas that GRRLIB shows as a texture.

Tools: Wacky Pencil, Line, Rectangle, Oval, Wacky Brush (28 brushes), Electric Mixer (15 effects),
Paint Can (21 textures), Eraser (13 erasers/effects), Alphabet, Rubber Stamps (1120 sprites),
Moving Van and the Undo Guy.

## Download

Grab the latest build from the [latest release](../../releases/latest): `kidpix-wii.zip`. Every push to the main branch refreshes it, and tagged versions get their own entry on the [releases page](../../releases).

1. Extract `kidpix-wii.zip` to the root of your SD card (or USB drive).
2. Open Kid Pix for Wii from the Homebrew Channel.

## Controllers

Same set as GlowWii, plus a USB mouse:

| Device | Cursor | Notes |
| --- | --- | --- |
| Wiimote | IR pointer | A draw, B bigger, 1 undo, 2 alt, +/- colour, D-pad < > tool, ^ v option, HOME menu |
| Nunchuk | stick | Z = B, C = 1 |
| Classic Controller | sticks | A, B, X = 2, Y = 1, ZL/ZR/L/R = colour |
| GameCube pad | stick / C-stick | A, B, X = 2, Y = 1, L/R = colour, Start = menu |
| Wii U Pro (libwupc) | sticks | as Classic |
| Wii U GamePad (libwiidrc) | sticks | as Classic (needs Wii U VC with patched fw.img) |
| DualShock 3 (libsicksaxis) | sticks | Cross = A, Circle = B, Square = 1, Triangle = 2 |
| USB mouse | mouse | left = draw, right = bigger, middle = undo, wheel = colour |
| USB keyboard | arrows | Enter draw, Shift bigger, 1-0 - = tools, Tab colour, Backspace undo, Esc menu, type in Alphabet tool |

"Bigger" (B) enlarges brushes/stamps and constrains rectangles to squares; "alt" (2) copies instead
of moves with the Moving Van and skips outlines on shapes. Hold 2 and press +/- to change palette.

## Saving drawings

The Save tool (top of the tool column, or Menu > Save picture) writes a numbered PNG to
`saves/` next to the app (`/apps/kidpix/saves/kidpix-001.png` on the SD card or USB drive).
The folder is created if it does not exist.

## Building

Needs devkitPro with devkitPPC and libogc, and the portlibs `libpng`, `libjpeg`, `freetype`, `zlib`.

    ./scripts/build_controller_portlibs.sh   # libwiidrc, libwupc, libsicksaxis -> portlibs/ppc
    ./scripts/build_grrlib.sh                # GRRLIB -> portlibs/ppc
    tools/prep_assets.sh path/to/jskidpix    # copies the art/sounds into assets/
    make                                     # boot.dol and the compressed boot-xz.dol
    make dist                                # dist/apps/kidpix -> copy `apps` to the SD card

`make` also writes `boot-xz.dol`, the same program packed into a self-extracting `.dol` a fraction of the size; `make dist` packages it as `boot.dol`. The first build downloads and builds [dolxz](https://github.com/FIX94/dolxz), which needs a host C compiler and liblzma (Debian/Ubuntu: `apt-get install liblzma-dev pkg-config`; macOS: `brew install xz`). `make NO_DOLXZ=1` skips this and uses the plain `.dol`.

The art and sounds are loaded at run time from `assets/` next to `boot.dol`. They are not stored in this
repository: CI downloads them from a pinned jskidpix commit and bundles them, with `boot.dol`, `meta.xml` and
`icon.png`, into the CI artifact and the GitHub release zip (tag `v*`). To install a release, unzip it and copy `apps` to the SD card. `src/textures_data.c`, `src/palettes_data.c` and
`src/font_data.c` are generated from jskidpix by `tools/gen_*.{js,py}` and are committed.

## License

GPL-3.0, like jskidpix, whose texture/palette data and tool behaviour this port is based on.
Kid Pix is a trademark of its owner; this is an unofficial fan port.
