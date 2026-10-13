#!/usr/bin/env python3
"""Draws the Homebrew Channel icon (128x48) from scratch -> icon.png"""
import sys, math
from PIL import Image, ImageDraw, ImageFont
W, H = 128, 48
im = Image.new("RGB", (W, H), (255, 255, 255))
d = ImageDraw.Draw(im)
for x in range(W):                                   # rainbow band
    hue = x / W
    r = int(127 + 127 * math.sin(6.283 * (hue)))
    g = int(127 + 127 * math.sin(6.283 * (hue + 1 / 3)))
    b = int(127 + 127 * math.sin(6.283 * (hue + 2 / 3)))
    d.line([(x, 0), (x, 9)], fill=(r, g, b))
    d.line([(x, H - 8), (x, H - 1)], fill=(r, g, b))
font = ImageFont.load_default(size=22)
d.text((8, 11), "Kid Pix", font=font, fill=(40, 40, 200))
d.text((10, 13), "Kid Pix", font=font, fill=(255, 200, 40))
d.text((9, 12), "Kid Pix", font=font, fill=(220, 40, 60))
small = ImageFont.load_default(size=9)
d.text((9, 36), "for Wii", font=small, fill=(40, 40, 40))
for i, (cx, cy, c) in enumerate([(100, 30, (0, 160, 60)), (112, 26, (240, 120, 0)), (92, 24, (60, 60, 220))]):
    d.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], fill=c, outline=(0, 0, 0))
d.rectangle([0, 0, W - 1, H - 1], outline=(0, 0, 0))
im.save(sys.argv[1])
