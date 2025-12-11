#!/usr/bin/env python3
"""
Check for mixed "priority" values in 8x8 tiles of a PNG.
This script converts the image to RGBA and reports tiles where non-transparent
pixels have differing alpha values (common cause of SGDK rescomp "different priority" errors).

Usage: python tools/check_tiles_priority.py path/to/image.png

Requires: Pillow (pip install Pillow)
"""
import sys
from PIL import Image

TILE_W = 8
TILE_H = 8


def check_image(path):
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    tiles_x = w // TILE_W
    tiles_y = h // TILE_H
    problems = []
    for ty in range(tiles_y):
        for tx in range(tiles_x):
            alphas = set()
            pixels = []
            for y in range(ty * TILE_H, (ty + 1) * TILE_H):
                for x in range(tx * TILE_W, (tx + 1) * TILE_W):
                    r, g, b, a = im.getpixel((x, y))
                    if a == 0:
                        continue
                    alphas.add(a)
                    pixels.append((x, y, r, g, b, a))
            if len(alphas) > 1:
                problems.append({
                    'tile': (tx, ty),
                    'alphas': sorted(alphas),
                    'pixels': pixels[:10]
                })
    return problems


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python tools/check_tiles_priority.py path/to/image.png")
        sys.exit(2)
    path = sys.argv[1]
    probs = check_image(path)
    if not probs:
        print("No mixed-alpha priority issues found (per 8x8 tile).")
        sys.exit(0)
    print(f"Found {len(probs)} tiles with mixed alpha values:")
    for p in probs:
        tx, ty = p['tile']
        print(f"- Tile {tx},{ty} (tile origin pixel {tx*TILE_W},{ty*TILE_H}) alphas={p['alphas']}")
        for px in p['pixels']:
            x,y,r,g,b,a = px
            print(f"    pixel: [{x},{y}] rgba=({r},{g},{b},{a})")
    print("\nFix: open the image in Aseprite/GIMP and ensure each 8x8 tile's non-transparent pixels share the same alpha/priority value (flatten/merge layers or re-export with consistent palette/alpha).")
    sys.exit(1)
