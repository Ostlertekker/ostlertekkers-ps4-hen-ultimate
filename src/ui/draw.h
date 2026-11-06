// PS4 HEN ULTIMATE - ui/draw.h
// Shared drawing helpers and animation utilities. All coordinates are in
// the logical 1920x1080 space.
#pragma once

#include <cmath>
#include <string>
#include <vector>

#include "platform/platform.h"
#include "themes/theme.h"

namespace henu {

constexpr float kScreenW = 1920.0f;
constexpr float kScreenH = 1080.0f;
constexpr float kMarginX = 96.0f;

struct Fonts {
    FontId small = 0;     // 20 regular
    FontId body = 0;      // 26 regular
    FontId bodyBold = 0;  // 26 semibold
    FontId label = 0;     // 18 bold (eyebrows, badges)
    FontId title = 0;     // 34 bold
    FontId tile = 0;      // 30 bold
    FontId h2 = 0;        // 44 bold
    FontId h1 = 0;        // 72 bold
    float sizeOf(FontId f) const;
};

inline Color withAlpha(Color c, float a) {
    float v = std::fmax(0.f, std::fmin(1.f, a)) * c.a;
    c.a = static_cast<uint8_t>(v + 0.5f);
    return c;
}

inline Color mix(Color a, Color b, float t) {
    auto l = [t](uint8_t x, uint8_t y) {
        return static_cast<uint8_t>(std::lround(x + (static_cast<float>(y) - x) * t));
    };
    return {l(a.r, b.r), l(a.g, b.g), l(a.b, b.b), l(a.a, b.a)};
}

inline Rect inflate(const Rect& r, float d) { return {r.x - d, r.y - d, r.w + 2 * d, r.h + 2 * d}; }
inline Rect scaleAround(const Rect& r, float s) {
    float w = r.w * s, h = r.h * s;
    return {r.x - (w - r.w) / 2, r.y - (h - r.h) / 2, w, h};
}

// Frame-rate independent exponential approach toward target.
inline float approach(float value, float target, float dt, float speed) {
    if (speed <= 0) return target;
    return target + (value - target) * std::exp(-speed * dt);
}

// Shortens text to fit maxWidth, appending "…" (UTF-8 safe).
std::string ellipsize(IRenderer& r, FontId f, const std::string& text, float maxWidth);

// Word wrap by measured width; keeps explicit '\n'.
std::vector<std::string> wrapText(IRenderer& r, FontId f, const std::string& text, float maxWidth);

void drawBackground(IRenderer& r, const Theme& t, double time, bool animate);
void drawPanel(IRenderer& r, const Theme& t, const Rect& rc, float radius);
// Small pill with text; returns its width.
float drawPill(IRenderer& r, const Fonts& f, const std::string& text, float x, float y,
               Color bg, Color fg);
float pillWidth(IRenderer& r, const Fonts& f, const std::string& text);
void drawFocusRing(IRenderer& r, const Theme& t, const Rect& rc, float radius, double time);

// Button hint like "(X) Open": circle glyph drawn as shape + label.
float drawButtonHint(IRenderer& r, const Fonts& f, const Theme& t, const std::string& glyph,
                     const std::string& label, float x, float y);

}  // namespace henu
