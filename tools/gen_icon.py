#!/usr/bin/env python3
"""Builds the Homebrew Channel icon (128x48, transparent) from the 1991 Kid Pix logo.

usage: gen_icon.py tools/icon/1991kidpixlogo.webp icon.png

The white background connected to the image edges becomes transparent; white
enclosed by the artwork (the eye, the spiral) is kept.
"""
import sys
from collections import deque
from PIL import Image

W, H = 128, 48
src = Image.open(sys.argv[1]).convert("RGBA")
w, h = src.size
px = src.load()

def is_bg(c):
    return c[3] == 0 or min(c[:3]) >= 235

seen = bytearray(w * h)
todo = deque()
for x in range(w):
    todo.extend(((x, 0), (x, h - 1)))
for y in range(h):
    todo.extend(((0, y), (w - 1, y)))
while todo:
    x, y = todo.popleft()
    if seen[y * w + x] or not is_bg(px[x, y]):
        continue
    seen[y * w + x] = 1
    px[x, y] = (255, 255, 255, 0)
    if x > 0: todo.append((x - 1, y))
    if x < w - 1: todo.append((x + 1, y))
    if y > 0: todo.append((x, y - 1))
    if y < h - 1: todo.append((x, y + 1))

art = src.crop(src.getbbox())
scale = min(W / art.width, H / art.height)
size = (max(1, round(art.width * scale)), max(1, round(art.height * scale)))
art = art.convert("RGBa").resize(size, Image.LANCZOS).convert("RGBA")

icon = Image.new("RGBA", (W, H), (0, 0, 0, 0))
icon.alpha_composite(art, ((W - size[0]) // 2, (H - size[1]) // 2))
icon.save(sys.argv[2], optimize=True)
