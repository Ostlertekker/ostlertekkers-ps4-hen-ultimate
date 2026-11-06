// PS4 HEN ULTIMATE - ui/draw.cpp
#include "ui/draw.h"

namespace henu {

float Fonts::sizeOf(FontId f) const {
    if (f == small) return 20;
    if (f == body || f == bodyBold) return 26;
    if (f == label) return 18;
    if (f == title) return 34;
    if (f == tile) return 30;
    if (f == h2) return 44;
    if (f == h1) return 72;
    return 26;
}

std::vector<std::string> wrapText(IRenderer& r, FontId f, const std::string& text, float maxWidth) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        std::string para = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        std::string line;
        size_t i = 0;
        while (i < para.size()) {
            size_t sp = para.find(' ', i);
            std::string word = para.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
            std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && r.measureText(f, candidate) > maxWidth) {
                lines.push_back(line);
                line = word;
            } else {
                line = candidate;
            }
            if (sp == std::string::npos) break;
            i = sp + 1;
        }
        lines.push_back(line);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return lines;
}

void drawBackground(IRenderer& r, const Theme& t, double time, bool animate) {
    const Color top = t.background;
    const Color bottom = mix(t.background, t.accent, t.dark ? 0.10f : 0.06f);
    r.gradientRect({0, 0, kScreenW, kScreenH}, top, bottom, 0);
    if (t.ambient == "none") return;
    // Soft ambient orb (concentric circles with falling alpha).
    // Static on purpose: the background is cached per theme (see App::frame).
    (void)time;
    (void)animate;
    const float cx = 1500, cy = 40;
    for (int i = 0; i < 6; ++i) {
        float rad = 520.f - i * 70.f;
        Color c = withAlpha(mix(t.accent, t.accent2, i / 6.f), 0.035f + i * 0.012f);
        r.fillRect({cx - rad, cy - rad, rad * 2, rad * 2}, c, rad);
    }
}

void drawPanel(IRenderer& r, const Theme& t, const Rect& rc, float radius) {
    r.fillRect(rc, withAlpha(t.surface, t.surfaceOpacity), radius);
    r.strokeRect(rc, t.line, 1.5f, radius);
}

float pillWidth(IRenderer& r, const Fonts& f, const std::string& text) {
    return r.measureText(f.label, text) + 28;
}

float drawPill(IRenderer& r, const Fonts& f, const std::string& text, float x, float y, Color bg, Color fg) {
    float w = pillWidth(r, f, text);
    r.fillRect({x, y, w, 34}, bg, 17);
    r.drawText(f.label, text, x + 14, y + 7, fg);
    return w;
}

void drawFocusRing(IRenderer& r, const Theme& t, const Rect& rc, float radius, double time) {
    (void)time;   // static ring: lets the app skip redraws while idle
    r.strokeRect(inflate(rc, 6), withAlpha(t.accent, 0.35f), 6, radius + 6);
    r.strokeRect(inflate(rc, 1), withAlpha(t.focus, 0.95f), 3.5f, radius + 1);
}

float drawButtonHint(IRenderer& r, const Fonts& f, const Theme& t, const std::string& glyph,
                     const std::string& label, float x, float y) {
    const float d = 34;
    r.fillRect({x, y, d, d}, withAlpha(t.text, 0.12f), d / 2);
    r.drawText(f.label, glyph, x + d / 2, y + 7, t.text, TextAlign::Center);
    float w = r.measureText(f.small, label);
    r.drawText(f.small, label, x + d + 10, y + 5, t.textMuted);
    return d + 10 + w + 28;
}

}  // namespace henu
