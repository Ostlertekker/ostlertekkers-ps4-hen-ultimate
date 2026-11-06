#!/usr/bin/env python3
"""PS4 HEN ULTIMATE - pre-renders bitmap font atlases (.hfnt) from Inter.

Run from project root:  python3 scripts/make_fonts.py
Needs Pillow. Output: assets/fonts/<name>.hfnt (read by src/gfx/font.cpp).

Why pre-rendered: the PS4 build then needs no FreeType/SDL_ttf, and text
measurement on host tests is identical to the console.

Format (little endian):
  char[4] "HFNT", u32 version=1
  u16 pixelSize, i16 ascent, i16 descent, i16 lineHeight
  u32 glyphCount, u32 kernCount, u16 atlasW, u16 atlasH
  glyphCount x { u32 codepoint, u16 x, u16 y, u16 w, u16 h,
                 i16 bearingX, i16 bearingY (from top of line box), i32 advance64 }
  kernCount  x { u32 left, u32 right, i32 adjust64 }
  atlasW*atlasH bytes of 8-bit coverage
"""
import os
import struct

from PIL import Image, ImageDraw, ImageFont

FONT_DIR = "assets/fonts"
SYMBOL_FONT = os.environ.get("HENU_SYMBOL_FONT", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf")

# name, source file, pixel size  (must match App::loadFonts)
FACES = [
    ("inter-regular-20", "Inter-Regular.otf", 20),
    ("inter-regular-26", "Inter-Regular.otf", 26),
    ("inter-semibold-26", "Inter-SemiBold.otf", 26),
    ("inter-bold-18", "Inter-Bold.otf", 18),
    ("inter-bold-28", "Inter-Bold.otf", 28),
    ("inter-bold-34", "Inter-Bold.otf", 34),
    ("inter-bold-44", "Inter-Bold.otf", 44),
    ("inter-bold-72", "Inter-Bold.otf", 72),
]

TEXT_CHARS = [chr(c) for c in range(32, 127)] + [chr(c) for c in range(160, 256)] + list("–—·•…‹›←→€„“”‚‘’")
SYMBOLS = list("✕○△□◂▸")          # PS button glyphs + arrows, from DejaVu Sans
KERN_CHARS = [chr(c) for c in range(32, 127)] + list("ÄÖÜäöüß")


def render_glyph(font, ch, ascent):
    l, t, r, b = font.getbbox(ch, anchor="ls")
    w, h = max(0, r - l), max(0, b - t)
    adv = font.getlength(ch)
    if w == 0 or h == 0:
        return None, 0, 0, adv
    img = Image.new("L", (w, h), 0)
    ImageDraw.Draw(img).text((-l, -t), ch, font=font, fill=255, anchor="ls")
    return img, l, ascent + t, adv


def build(name, src, size):
    font = ImageFont.truetype(os.path.join(FONT_DIR, src), size)
    sym = ImageFont.truetype(SYMBOL_FONT, size)
    ascent, descent = font.getmetrics()
    line = ascent + descent
    glyphs = []
    for ch in TEXT_CHARS + SYMBOLS:
        f = sym if ch in SYMBOLS else font
        img, bx, by, adv = render_glyph(f, ch, ascent)
        glyphs.append((ord(ch), img, bx, by, adv))

    # shelf packing
    atlas_w = 1024
    x = y = shelf = 0
    placed = []
    for cp, img, bx, by, adv in glyphs:
        w, h = (img.size if img else (0, 0))
        if x + w + 1 > atlas_w:
            x, y, shelf = 0, y + shelf + 1, 0
        placed.append((cp, x, y, w, h, bx, by, adv, img))
        x += w + 1
        shelf = max(shelf, h)
    atlas_h = y + shelf + 1
    atlas = Image.new("L", (atlas_w, atlas_h), 0)
    for cp, gx, gy, w, h, bx, by, adv, img in placed:
        if img:
            atlas.paste(img, (gx, gy))

    kerns = []
    for a in KERN_CHARS:
        la = font.getlength(a)
        for b in KERN_CHARS:
            adj = font.getlength(a + b) - la - font.getlength(b)
            if abs(adj) >= 0.25:
                kerns.append((ord(a), ord(b), int(round(adj * 64))))

    out = bytearray()
    out += b"HFNT" + struct.pack("<I", 1)
    out += struct.pack("<Hhhh", size, ascent, descent, line)
    out += struct.pack("<IIHH", len(placed), len(kerns), atlas_w, atlas_h)
    for cp, gx, gy, w, h, bx, by, adv, img in placed:
        out += struct.pack("<IHHHHhhi", cp, gx, gy, w, h, bx, by, int(round(adv * 64)))
    for a, b, adj in kerns:
        out += struct.pack("<IIi", a, b, adj)
    out += atlas.tobytes()
    path = os.path.join(FONT_DIR, name + ".hfnt")
    with open(path, "wb") as fh:
        fh.write(out)
    return path, len(placed), len(kerns), atlas_w, atlas_h, len(out)


if __name__ == "__main__":
    for face in FACES:
        p, n, k, w, h, size = build(*face)
        print(f"{p}: {n} glyphs, {k} kern pairs, atlas {w}x{h}, {size // 1024} KB")
