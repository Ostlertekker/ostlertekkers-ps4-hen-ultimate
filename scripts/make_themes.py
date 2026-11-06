#!/usr/bin/env python3
"""OSTLERTEKKERS HEN - generates the bundled themes (assets/themes/*.json).
All themes are original colour sets (no third-party artwork).
Run from project root: python3 scripts/make_themes.py
"""
import json
import os


def mix(a, b, t):
    a = [int(a[i:i + 2], 16) for i in (1, 3, 5)]
    b = [int(b[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join("%02x" % round(x + (y - x) * t) for x, y in zip(a, b))


# id, de, en, category, mode, background, accent, accent2, ambient
THEMES = [
    # PS5-inspired
    ("midnight", "Mitternacht", "Midnight", "ps5", "dark", "#080b14", "#8b7cff", "#42b7ef", "orb"),
    ("cosmic-blue", "Kosmisch Blau", "Cosmic Blue", "ps5", "dark", "#060c1c", "#3d7bff", "#7fb2ff", "orb"),
    ("starlight", "Sternenlicht", "Starlight", "ps5", "dark", "#0b0d18", "#c9d2ff", "#7c8cff", "particles"),
    ("deep-space", "Tiefer Raum", "Deep Space", "ps5", "dark", "#05070d", "#5b6cff", "#22d3ee", "orb"),
    # Dark
    ("aurora", "Polarlicht", "Aurora", "dark", "dark", "#071a20", "#48c9ad", "#7fd6ff", "waves"),
    ("crimson", "Karmesin", "Crimson", "dark", "dark", "#170b12", "#d85b78", "#f2a65a", "orb"),
    ("arctic", "Arktis", "Arctic", "dark", "dark", "#0b1727", "#73c9ff", "#a8e6ff", "none"),
    ("obsidian", "Obsidian", "Obsidian", "dark", "dark", "#0a0a0c", "#9aa3b5", "#d4d9e3", "none"),
    ("forest-night", "Waldnacht", "Forest Night", "dark", "dark", "#08140e", "#4fbf7a", "#b7e36a", "waves"),
    # Neon
    ("synthwave", "Synthwave", "Synthwave", "neon", "dark", "#1b1035", "#ff69c8", "#5ce1ff", "particles"),
    ("cyber-lime", "Cyber Limette", "Cyber Lime", "neon", "dark", "#0b1209", "#b6ff3c", "#00e5a8", "particles"),
    ("laser-magenta", "Laser Magenta", "Laser Magenta", "neon", "dark", "#140818", "#ff2bd6", "#8f5bff", "orb"),
    ("electric-cyan", "Elektro Cyan", "Electric Cyan", "neon", "dark", "#03121a", "#00e1ff", "#3dffb4", "particles"),
    ("hardtekk-red", "Hardtekk Rot", "Hardtekk Red", "neon", "dark", "#120606", "#ff2a2a", "#ffb12a", "orb"),
    # Minimal
    ("graphite", "Graphit", "Graphite", "minimal", "dark", "#121316", "#e6e8ec", "#8f96a3", "none"),
    ("slate", "Schiefer", "Slate", "minimal", "dark", "#0f1720", "#8fb3d9", "#c4d4e6", "none"),
    ("mono-gold", "Mono Gold", "Mono Gold", "minimal", "dark", "#0e0d0b", "#d9b56a", "#f0dcae", "none"),
    ("nordic", "Nordisch", "Nordic", "minimal", "dark", "#111820", "#88c0d0", "#a3be8c", "none"),
    # Gaming
    ("ember", "Glut", "Ember", "gaming", "dark", "#150b06", "#ff7a1a", "#ffcf3d", "orb"),
    ("toxic", "Toxisch", "Toxic", "gaming", "dark", "#0a1206", "#7dff3c", "#c4ff3c", "particles"),
    ("royal", "Königlich", "Royal", "gaming", "dark", "#0f0a1c", "#b28cff", "#ffd36b", "orb"),
    ("ocean-strike", "Ozean", "Ocean Strike", "gaming", "dark", "#04121c", "#1fa3ff", "#29ffd0", "waves"),
    # Light
    ("cloud", "Wolke", "Cloud", "light", "light", "#eef1f7", "#4b5bff", "#00a3c4", "none"),
    ("sand", "Sand", "Sand", "light", "light", "#f4efe6", "#b5651d", "#2f7d6d", "none"),
]


def theme(tid, de, en, cat, mode, bg, acc, acc2, amb):
    dark = mode == "dark"
    if dark:
        text, muted, line, surface = "#f4f6ff", "#929bb1", "#ffffff17", mix(bg, "#ffffff", 0.06)
        tiles = [[mix(bg, acc if i % 2 == 0 else acc2, 0.50 - 0.04 * i), mix(bg, acc, 0.16)] for i in range(6)]
        surface_alt = mix(bg, acc, 0.18)
        focus = "#ffffff"
    else:
        text, muted, line, surface = "#141824", "#5a6274", "#0000001f", "#ffffff"
        tiles = [[mix("#ffffff", acc if i % 2 == 0 else acc2, 0.30 - 0.03 * i), mix("#ffffff", acc, 0.10)] for i in range(6)]
        surface_alt = mix("#ffffff", acc, 0.14)
        focus = mix(acc, "#000000", 0.25)
    if tid == "midnight":
        tiles = [["#493b91", "#26234d"], ["#1c5c83", "#182a49"], ["#813c78", "#3d244e"],
                 ["#236d69", "#183b43"], ["#80602e", "#40331f"], ["#454d65", "#252d40"]]
    return {
        "schemaVersion": 1, "id": tid, "name": {"de": de, "en": en},
        "author": "OSTLERTEKKERS HEN", "license": "GPL-3.0-or-later",
        "category": cat, "mode": mode,
        "colors": {"background": bg, "surface": surface, "surfaceAlt": surface_alt, "text": text,
                   "textMuted": muted, "accent": acc, "accent2": acc2, "focus": focus, "line": line,
                   "ok": "#2fae73" if not dark else "#48c98c", "warn": "#c98a16" if not dark else "#e5b65e",
                   "error": "#d23c4f" if not dark else "#e85c6a"},
        "tileGradients": tiles,
        "surfaceOpacity": 0.92 if dark else 1.0,
        "cornerRadius": 22,
        "background": {"type": "gradient", "dim": 0.4},
        "animation": {"speed": 1.0, "focusScale": 1.05, "ambient": amb},
    }


if __name__ == "__main__":
    out = "assets/themes"
    for f in os.listdir(out):
        if f.endswith(".json"):
            os.remove(os.path.join(out, f))
    for t in THEMES:
        with open(os.path.join(out, t[0] + ".json"), "w", encoding="utf-8") as fh:
            fh.write(json.dumps(theme(*t), indent=2, ensure_ascii=False) + "\n")
    print(len(THEMES), "themes written")
