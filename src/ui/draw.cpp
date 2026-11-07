// OSTLERTEKKERS HEN - ui/draw.cpp
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

std::string ellipsize(IRenderer& r, FontId f, const std::string& text, float maxWidth) {
    if (r.measureText(f, text) <= maxWidth) return text;
    const std::string dots = "\xE2\x80\xA6";
    std::string s = text;
    while (!s.empty()) {
        // drop one UTF-8 code point from the end
        size_t i = s.size() - 1;
        while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
        s.resize(i);
        while (!s.empty() && s.back() == ' ') s.pop_back();
        if (r.measureText(f, s + dots) <= maxWidth) return s + dots;
    }
    return dots;
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

namespace {
float fxHash(int i, int salt) {
    uint32_t x = static_cast<uint32_t>(i) * 2654435761u + static_cast<uint32_t>(salt) * 40503u + 101u;
    x ^= x >> 15; x *= 0x2c1b3c6dU; x ^= x >> 12; x *= 0x297a2d39U; x ^= x >> 15;
    return (x & 0xFFFFFF) / 16777215.0f;
}
void circle(IRenderer& r, float x, float y, float rad, Color c) { r.fillRect({x - rad, y - rad, 2 * rad, 2 * rad}, c, rad); }

// Moving starfield + drifting nebula. Two parallax layers scroll left.
void drawGalaxy(IRenderer& r, const Theme& t, double time) {
    r.gradientRect({0, 0, kScreenW, kScreenH}, {6, 7, 16, 255}, mix(Color{6, 7, 16, 255}, t.accent, 0.12f), 0);
    const float ft = static_cast<float>(time);
    // Nebula blobs.
    for (int i = 0; i < 7; ++i) {
        const float bx = std::fmod(fxHash(i, 1) * kScreenW + ft * (6 + 10 * fxHash(i, 2)), kScreenW + 600.f) - 300.f;
        const float by = fxHash(i, 3) * kScreenH;
        const float rad = 220 + 260 * fxHash(i, 4);
        const Color c = mix(t.accent, t.accent2, fxHash(i, 5));
        circle(r, bx, by, rad, withAlpha(c, 0.05f + 0.03f * std::sin(ft * 0.4f + i)));
    }
    // Two star layers.
    for (int layer = 0; layer < 2; ++layer) {
        const int n = 90;
        const float speed = layer ? 42.f : 20.f, sz = layer ? 2.3f : 1.4f;
        for (int i = 0; i < n; ++i) {
            const int s = i + layer * 1000;
            float x = std::fmod(fxHash(s, 6) * (kScreenW + 40.f) - ft * speed, kScreenW + 40.f);
            if (x < 0) x += kScreenW + 40.f;
            const float y = fxHash(s, 7) * kScreenH;
            const float tw = 0.5f + 0.5f * std::sin(ft * (1.5f + 2 * fxHash(s, 8)) + i);
            circle(r, x - 20, y, sz * (0.6f + 0.6f * fxHash(s, 9)), withAlpha({235, 240, 255, 255}, 0.25f + 0.6f * tw));
        }
    }
}

// PS3 XMB-inspired flowing wave bands.
void drawWave(IRenderer& r, const Theme& t, double time) {
    r.gradientRect({0, 0, kScreenW, kScreenH}, t.background, mix(t.background, t.accent, 0.18f), 0);
    const float ft = static_cast<float>(time);
    for (int b = 0; b < 5; ++b) {
        const Color c = withAlpha(mix(t.accent, t.accent2, b / 5.f), 0.06f + 0.015f * b);
        const float baseY = 160 + b * 190;
        const float amp = 60 + 20 * b;
        float px = 0, py = baseY;
        for (int x = 0; x <= kScreenW; x += 40) {
            const float y = baseY + std::sin(x * 0.004f + ft * (0.6f + 0.12f * b) + b) * amp;
            if (x > 0) {
                // thick segment as a quad
                r.fillPath({{{px, py}, {static_cast<float>(x), y}, {static_cast<float>(x), y + 10}, {px, py + 10}}}, c);
            }
            px = static_cast<float>(x);
            py = y;
        }
    }
}

// Soft vertical aurora curtains.
void drawAurora(IRenderer& r, const Theme& t, double time) {
    r.gradientRect({0, 0, kScreenW, kScreenH}, {4, 8, 14, 255}, {10, 16, 28, 255}, 0);
    const float ft = static_cast<float>(time);
    for (int i = 0; i < 9; ++i) {
        const float base = (i + 0.5f) / 9.f * kScreenW;
        const float x = base + std::sin(ft * 0.5f + i * 1.3f) * 160;
        const float w = 90 + 50 * fxHash(i, 2);
        const Color c = mix(t.accent, t.accent2, fxHash(i, 3));
        for (int k = 0; k < 5; ++k) {
            const float a = 0.05f * (1 - k * 0.15f) * (0.6f + 0.4f * std::sin(ft * 0.8f + i + k));
            r.fillRect({x - w / 2 + k * 4, 0, w - k * 8, kScreenH}, withAlpha(c, a), w / 2);
        }
    }
}
}  // namespace

bool backgroundIsAnimated(const Theme& t) { return t.backgroundFx != "theme"; }

void drawBackground(IRenderer& r, const Theme& t, double time, bool animate) {
    if (t.backgroundFx == "galaxy") { drawGalaxy(r, t, time); return; }
    if (t.backgroundFx == "wave") { drawWave(r, t, time); return; }
    if (t.backgroundFx == "aurora") { drawAurora(r, t, time); return; }
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
    if (t.rgbBorder) {
        // Animated rainbow: hue rotates over time, slight offset top vs. bottom.
        const float h = static_cast<float>(time) * 0.25f;
        const Color a = rainbow(h), b = rainbow(h + 0.5f);
        r.strokeRect(inflate(rc, 7), withAlpha(a, 0.40f), 7, radius + 7);
        r.strokeRect(inflate(rc, 3), withAlpha(mix(a, b, 0.5f), 0.85f), 3.5f, radius + 3);
        r.strokeRect(inflate(rc, 1), withAlpha({255, 255, 255, 255}, 0.9f), 2.0f, radius + 1);
        return;
    }
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
