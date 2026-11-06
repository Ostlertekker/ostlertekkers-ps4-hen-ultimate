// OSTLERTEKKERS HEN - ui/logo.cpp
#include "ui/logo.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace henu {

namespace {
constexpr float kPi = 3.14159265358979f;

std::vector<IRenderer::Pt> strip(IRenderer::Pt a, IRenderer::Pt b, float w) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 0.001f) return {};
    const float nx = -dy / len * w / 2, ny = dx / len * w / 2;
    // Extend slightly along the line so the points stay sharp-ish but closed.
    const float ex = dx / len * w * 0.15f, ey = dy / len * w * 0.15f;
    return {{a.x + nx - ex, a.y + ny - ey}, {b.x + nx + ex, b.y + ny + ey},
            {b.x - nx + ex, b.y - ny + ey}, {a.x - nx - ex, a.y - ny - ey}};
}

std::vector<IRenderer::Pt> circle(float cx, float cy, float r, int n) {
    std::vector<IRenderer::Pt> pts;
    pts.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const float a = 2 * kPi * i / n;
        pts.push_back({cx + r * std::cos(a), cy + r * std::sin(a)});
    }
    return pts;
}
}  // namespace

void drawPentagram(IRenderer& r, float cx, float cy, float radius, const LogoStyle& s) {
    const float w = std::max(1.5f, radius * s.strokeRatio);
    const float starR = s.showRing ? radius - w * 1.6f : radius;
    const int segments = std::max(24, static_cast<int>(radius * 0.9f));
    const float p = std::max(0.f, std::min(1.f, s.progress));
    if (s.showRing && p > 0) {
        if (p >= 1) {
            r.fillPath({circle(cx, cy, radius, segments), circle(cx, cy, radius - w, segments)}, s.ring);
        } else {
            // Partial ring: arc as a polygon band.
            std::vector<IRenderer::Pt> band;
            const int n = std::max(2, static_cast<int>(segments * p));
            const float a0 = -kPi / 2 + s.rotation;
            for (int i = 0; i <= n; ++i) {
                const float a = a0 + 2 * kPi * p * i / n;
                band.push_back({cx + radius * std::cos(a), cy + radius * std::sin(a)});
            }
            for (int i = n; i >= 0; --i) {
                const float a = a0 + 2 * kPi * p * i / n;
                band.push_back({cx + (radius - w) * std::cos(a), cy + (radius - w) * std::sin(a)});
            }
            r.fillPath({band}, s.ring);
        }
    }
    IRenderer::Pt pts[5];
    const float start = (s.pointDown ? kPi / 2 : -kPi / 2) + s.rotation;
    for (int i = 0; i < 5; ++i) {
        const float a = start + 2 * kPi * i / 5;
        pts[i] = {cx + starR * std::cos(a), cy + starR * std::sin(a)};
    }
    // Strokes 0->2->4->1->3->0, drawn progressively.
    const int order[6] = {0, 2, 4, 1, 3, 0};
    const float total = p * 5;
    for (int i = 0; i < 5; ++i) {
        const float f = std::min(1.f, total - i);
        if (f <= 0) break;
        IRenderer::Pt a = pts[order[i]], b = pts[order[i + 1]];
        b = {a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f};
        auto st = strip(a, b, w);
        if (!st.empty()) r.fillPath({st}, s.star);
    }
}

}  // namespace henu
