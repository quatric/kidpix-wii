# Kid Pix for Wii

A Wii homebrew port, written in C with [GRRLIB](https://github.com/GRRLIB/GRRLIB), of the
[jskidpix](https://github.com/vikrum/kidpix) HTML/JS recreation of Kid Pix 1.0
(with [KidPad](https://github.com/chrissotraidis/kidpad) as a second behaviour reference).
Drawing is done by a small software rasteriser into a 600x408 canvas that GRRLIB shows as a texture.

Tools: Wacky Pencil, Line, Rectangle, Oval, Wacky Brush (28 brushes), Electric Mixer (15 effects),
Paint Can (21 textures), Eraser (13 erasers/effects), Alphabet, Rubber Stamps (1120 sprites),
Moving Van and the Undo Guy.

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
    make                                     # boot.dol
    make dist                                # dist/apps/kidpix -> copy `apps` to the SD card

The art and sounds are loaded at run time from `assets/` next to `boot.dol`; they are not part of this
repository (CI therefore publishes only `boot.dol`, `meta.xml` and `icon.png`; tag `v*` makes a GitHub release zip). `src/textures_data.c`, `src/palettes_data.c` and
`src/font_data.c` are generated from jskidpix by `tools/gen_*.{js,py}` and are committed.

## License

GPL-3.0, like jskidpix, whose texture/palette data and tool behaviour this port is based on.
Kid Pix is a trademark of its owner; this is an unofficial fan port.
