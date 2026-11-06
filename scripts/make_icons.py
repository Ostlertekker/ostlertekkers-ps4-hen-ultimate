#!/usr/bin/env python3
"""OSTLERTEKKERS HEN - generates sce_sys/icon0.png (512x512) and pic1.png (1920x1080).
Own artwork: glowing pentagram in a ring (inverted, point down), Inter font (OFL).
Run from project root."""
import math

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

SS = 4   # supersampling for smooth edges


def gradient(w, h, c1, c2):
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    t = (xx / (w - 1) + yy / (h - 1)) / 2
    a, b = np.array(c1, np.float32), np.array(c2, np.float32)
    return Image.fromarray((a * (1 - t[..., None]) + b * t[..., None]).astype(np.uint8), "RGB")


def radial(w, h, cx, cy, r, inner, outer):
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    t = np.clip(np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2) / r, 0, 1)[..., None]
    a, b = np.array(inner, np.float32), np.array(outer, np.float32)
    return Image.fromarray((a * (1 - t) + b * t).astype(np.uint8), "RGB")


def pentagram_layer(size, cx, cy, radius, stroke, color):
    """RGBA layer with ring + inverted pentagram, drawn supersampled."""
    big = Image.new("RGBA", (size[0] * SS, size[1] * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    R, w = radius * SS, stroke * SS
    X, Y = cx * SS, cy * SS
    d.ellipse([X - R, Y - R, X + R, Y + R], outline=color, width=int(w))
    sr = R - w * 1.6
    pts = [(X + sr * math.cos(math.pi / 2 + 2 * math.pi * i / 5),
            Y + sr * math.sin(math.pi / 2 + 2 * math.pi * i / 5)) for i in range(5)]
    order = [0, 2, 4, 1, 3, 0]
    for i in range(5):
        d.line([pts[order[i]], pts[order[i + 1]]], fill=color, width=int(w))
    for p in pts:   # rounded joints
        d.ellipse([p[0] - w / 2, p[1] - w / 2, p[0] + w / 2, p[1] + w / 2], fill=color)
    return big.resize(size, Image.LANCZOS)


def add_glowing_logo(img, cx, cy, radius, stroke, core, glow_color):
    glow = pentagram_layer(img.size, cx, cy, radius, stroke * 2.6, glow_color + (200,))
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(radius * 0.09)))
    img.alpha_composite(pentagram_layer(img.size, cx, cy, radius, stroke, core + (255,)))


bold = "assets/fonts/Inter-Bold.otf"

# icon0: 512x512
icon = radial(512, 512, 256, 230, 360, (88, 10, 18), (6, 4, 8)).convert("RGBA")
add_glowing_logo(icon, 256, 226, 168, 13, (255, 236, 220), (255, 40, 40))
d = ImageDraw.Draw(icon)
d.text((256, 448), "OSTLERTEKKERS", font=ImageFont.truetype(bold, 46), fill=(255, 240, 232, 255), anchor="mm")
d.text((256, 490), "HEN", font=ImageFont.truetype(bold, 30), fill=(255, 92, 82, 255), anchor="mm")
icon.convert("RGB").save("sce_sys/icon0.png")

# pic1: 1920x1080 background shown behind the PS4 home tile
bg = gradient(1920, 1080, (30, 4, 10), (5, 4, 8)).convert("RGBA")
add_glowing_logo(bg, 1460, 470, 330, 22, (255, 228, 214), (220, 20, 30))
d = ImageDraw.Draw(bg)
d.text((120, 840), "OSTLERTEKKERS HEN", font=ImageFont.truetype(bold, 96), fill=(255, 240, 232, 240))
d.text((126, 960), "HOMEBREW DASHBOARD · GOLDHEN", font=ImageFont.truetype(bold, 30), fill=(255, 92, 82, 230))
bg.convert("RGB").save("sce_sys/pic1.png")
print("sce_sys/icon0.png", Image.open("sce_sys/icon0.png").size, "sce_sys/pic1.png", Image.open("sce_sys/pic1.png").size)
