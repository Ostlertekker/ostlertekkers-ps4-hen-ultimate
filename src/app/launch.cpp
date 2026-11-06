// OSTLERTEKKERS HEN - app/launch.cpp
#include "app/launch.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include "ui/logo.h"

namespace henu {

namespace {
constexpr float kPi = 3.14159265358979f;
float clamp01(double v) { return static_cast<float>(v < 0 ? 0 : (v > 1 ? 1 : v)); }
float ramp(double t, double a, double b) { return clamp01((t - a) / (b - a)); }
float easeOutBack(float x) {
    const float c1 = 1.70158f, c3 = c1 + 1;
    return 1 + c3 * std::pow(x - 1, 3.f) + c1 * std::pow(x - 1, 2.f);
}
}  // namespace

LaunchSequence::LaunchSequence(std::string styleId, std::string title, std::string titleId, TextureId icon)
    : boot_(std::move(styleId)), title_(std::move(title)), titleId_(std::move(titleId)), icon_(icon) {}

void LaunchSequence::draw(IRenderer& r, const Fonts& f, double t, const std::string& statusText) const {
    const float W = static_cast<float>(kScreenW), H = static_cast<float>(kScreenH);
    const Color accent = boot_.accent();
    const double hit = kLaunchHitSeconds;
    if (t >= launchAt()) {
        r.fillRect({0, 0, W, H}, {0, 0, 0, 255}, 0);
        if (!statusText.empty()) r.drawText(f.small, statusText, W / 2, H - 120, {150, 150, 160, 255}, TextAlign::Center);
        return;
    }
    // Scenery of the boot style at its "glowing" phase, dimmed. Drawn once and
    // cached as a layer: the full-screen effects are the expensive part.
    const uint64_t key = 0x1A0C0000u + std::hash<std::string>()(boot_.style()) % 0xFFFF + 1;
    if (!r.useCachedLayer(key)) {
        boot_.drawBackdrop(r, 3.0);
        r.fillRect({0, 0, W, H}, {0, 0, 0, 165}, 0);
        r.endLayer();
    }

    const float cx = W / 2, cy = 440;
    const float in = easeOutBack(ramp(t, 0.0, 0.55));
    // Rotating pentagram ring behind the icon.
    {
        LogoStyle ls;
        const float ringA = in * (1 - ramp(t, hit, hit + 0.2));
        ls.star = withAlpha(accent, 0.55f * ringA);
        ls.ring = withAlpha(accent, 0.8f * ringA);
        ls.strokeRatio = 0.035f;
        ls.rotation = static_cast<float>(t) * 0.9f;
        const float pulse = 1 + 0.04f * std::sin(static_cast<float>(t) * 9);
        if (ringA > 0.01f) {
            drawPentagram(r, cx, cy, 250 * in * pulse, ls);
            ls.star = ls.ring = withAlpha(accent, 0.12f * ringA);
            ls.strokeRatio = 0.12f;
            drawPentagram(r, cx, cy, 262 * in * pulse, ls);
        }
    }
    // Icon card, zooms towards the viewer after the hit.
    const float zoom = t > hit ? 1 + 1.6f * std::pow(ramp(t, hit, hit + 0.45), 2.f) : 1.f;
    const float size = 230 * in * zoom;
    const float iconAlpha = 1 - ramp(t, hit + 0.1, hit + 0.45);
    const Rect card{cx - size / 2, cy - size / 2, size, size};
    if (size > 2 && iconAlpha > 0.02f) {
        r.fillRect({card.x - 10, card.y - 10, card.w + 20, card.h + 20}, withAlpha(accent, 0.35f * iconAlpha), 34 * zoom);
        if (icon_) r.drawTexture(icon_, card, iconAlpha, 26 * zoom);
        else {
            r.fillRect(card, withAlpha({30, 30, 38, 255}, iconAlpha), 26 * zoom);
            LogoStyle ls;
            ls.star = ls.ring = withAlpha(accent, iconAlpha);
            drawPentagram(r, cx, cy, size * 0.36f, ls);
        }
    }
    // Title + id.
    const float ta = ramp(t, 0.25, 0.6) * (1 - ramp(t, hit, hit + 0.3));
    if (ta > 0) {
        const float ty = cy + 300 + 30 * (1 - ta);
        r.drawText(f.title, title_, cx, ty, withAlpha({255, 255, 255, 255}, ta), TextAlign::Center);
        r.drawText(f.label, titleId_, cx, ty + 70, withAlpha(accent, ta), TextAlign::Center);
    }
    // Light streaks converging before the hit.
    if (t < hit) {
        const float k = ramp(t, 0.3, hit);
        for (int i = 0; i < 24; ++i) {
            const float a = 2 * kPi * i / 24 + static_cast<float>(t) * 0.4f;
            const float d0 = 900 * (1 - k) + 290, len = 120 + 200 * k;
            const float x0 = cx + std::cos(a) * d0, y0 = cy + std::sin(a) * d0;
            const float x1 = cx + std::cos(a) * (d0 - len), y1 = cy + std::sin(a) * (d0 - len);
            const float nx = -std::sin(a) * 2.5f, ny = std::cos(a) * 2.5f;
            r.fillPath({{{x0 + nx, y0 + ny}, {x1 + nx, y1 + ny}, {x1 - nx, y1 - ny}, {x0 - nx, y0 - ny}}},
                       withAlpha(accent, 0.5f * k));
        }
    }
    // Impact flash, then fade to black.
    if (t >= hit) {
        const float fl = static_cast<float>(std::exp(-(t - hit) * 9));
        if (fl > 0.02f) r.fillRect({0, 0, W, H}, withAlpha(mix(accent, {255, 255, 255, 255}, 0.6f), 0.85f * fl), 0);
    }
    const float out = ramp(t, launchAt() - 0.45, launchAt());
    if (out > 0) r.fillRect({0, 0, W, H}, withAlpha({0, 0, 0, 255}, out), 0);
}

}  // namespace henu
