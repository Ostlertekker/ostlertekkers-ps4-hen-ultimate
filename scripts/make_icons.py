#!/usr/bin/env python3
"""PS4 HEN ULTIMATE - generates sce_sys/icon0.png (512x512) and pic1.png (1920x1080).
Own artwork (gradient + monogram), Inter font (OFL). Run from project root."""
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

def gradient(w, h, c1, c2):
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    t = (xx / (w - 1) + yy / (h - 1)) / 2
    a, b = np.array(c1, np.float32), np.array(c2, np.float32)
    return Image.fromarray((a * (1 - t[..., None]) + b * t[..., None]).astype(np.uint8), "RGB")

def glow(img, cx, cy, r, color, alpha):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).ellipse([cx - r, cy - r, cx + r, cy + r], fill=color + (alpha,))
    layer = layer.filter(ImageFilter.GaussianBlur(r * 0.35))
    img.alpha_composite(layer)

bold = "assets/fonts/Inter-Bold.otf"

# icon0: 512x512
icon = gradient(512, 512, (36, 30, 84), (8, 11, 20)).convert("RGBA")
glow(icon, 380, 110, 210, (139, 124, 255), 150)
glow(icon, 120, 430, 160, (66, 183, 239), 90)
d = ImageDraw.Draw(icon)
d.rounded_rectangle([96, 96, 416, 416], radius=72, fill=(169, 154, 255, 255))
inner = gradient(300, 300, (190, 178, 255), (86, 80, 201)).convert("RGBA")
mask = Image.new("L", (300, 300), 0)
ImageDraw.Draw(mask).rounded_rectangle([0, 0, 299, 299], radius=64, fill=255)
icon.paste(inner, (106, 106), mask)
d.text((256, 250), "H", font=ImageFont.truetype(bold, 210), fill=(255, 255, 255, 255), anchor="mm")
d.text((256, 462), "HEN ULTIMATE", font=ImageFont.truetype(bold, 40), fill=(244, 246, 255, 255), anchor="mm")
icon.convert("RGB").save("sce_sys/icon0.png")

# pic1: 1920x1080 background
bg = gradient(1920, 1080, (16, 14, 40), (8, 11, 20)).convert("RGBA")
glow(bg, 1500, 120, 620, (139, 124, 255), 110)
glow(bg, 300, 980, 480, (66, 183, 239), 60)
d = ImageDraw.Draw(bg)
d.text((120, 860), "PS4 HEN ULTIMATE", font=ImageFont.truetype(bold, 84), fill=(244, 246, 255, 235))
d.text((124, 960), "ALL IN ONE · DASHBOARD", font=ImageFont.truetype(bold, 30), fill=(160, 150, 255, 220))
bg.convert("RGB").save("sce_sys/pic1.png")
print("sce_sys/icon0.png", Image.open("sce_sys/icon0.png").size, "sce_sys/pic1.png", Image.open("sce_sys/pic1.png").size)
