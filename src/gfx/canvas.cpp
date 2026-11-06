// PS4 HEN ULTIMATE - gfx/canvas.cpp
#include "gfx/canvas.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace henu {

namespace {
constexpr int kSub = 4;   // vertical sub-samples per pixel row

// Horizontal extent of a rounded rectangle at height yc. Empty -> l >= r.
inline void span(float x0, float y0, float x1, float y1, float r, float yc, float& l, float& rr) {
    if (yc < y0 || yc >= y1) { l = 1; rr = 0; return; }
    float dy = 0;
    if (yc < y0 + r) dy = y0 + r - yc;
    else if (yc > y1 - r) dy = yc - (y1 - r);
    float dx = 0;
    if (dy > 0) {
        if (dy >= r) { l = 1; rr = 0; return; }
        dx = r - std::sqrt(r * r - dy * dy);
    }
    l = x0 + dx;
    rr = x1 - dx;
}

inline float overlap(float a0, float a1, float b0, float b1) {
    float lo = a0 > b0 ? a0 : b0, hi = a1 < b1 ? a1 : b1;
    return hi > lo ? hi - lo : 0.f;
}

// 4x4 ordered dither offsets (Bayer), in 1/16 steps of one 8-bit level, scaled to 16.16.
const int32_t kBayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};

// t in 0..65536; dither in 0..65535 (sub-level offset) avoids visible banding.
inline uint8_t lerpFix(uint8_t a, uint8_t b, int32_t t, int32_t dither) {
    const int32_t v = (static_cast<int32_t>(a) << 16) + (static_cast<int32_t>(b) - a) * t + dither;
    const int32_t r = v >> 16;
    return static_cast<uint8_t>(r < 0 ? 0 : (r > 255 ? 255 : r));
}
}  // namespace

Canvas::Canvas(int w, int h) : w_(w), h_(h), px_(static_cast<size_t>(w) * h, 0xFF000000u) {}

void Canvas::clear(Color c) {
    c.a = 255;
    std::fill(px_.begin(), px_.end(), pack(c));
}

void Canvas::copyFrom(const Canvas& o) {
    if (o.w_ == w_ && o.h_ == h_) std::memcpy(px_.data(), o.px_.data(), px_.size() * sizeof(uint32_t));
}

void Canvas::blend(uint32_t& dst, Color c, int a8) {
    if (a8 <= 0) return;
    if (a8 >= 255) { dst = pack({c.r, c.g, c.b, 255}); return; }
    const int ia = 255 - a8;
    const int dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    const int r = (c.r * a8 + dr * ia + 127) / 255;
    const int g = (c.g * a8 + dg * ia + 127) / 255;
    const int b = (c.b * a8 + db * ia + 127) / 255;
    dst = 0xFF000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

void Canvas::paint(const Shape& o, const Shape* in, Color from, Color to, bool gradient, const Rect& gr) {
    const int yStart = std::max(0, static_cast<int>(std::floor(o.y0)));
    const int yEnd = std::min(h_, static_cast<int>(std::ceil(o.y1)));
    const float gw = gr.w > 0 ? gr.w : 1, gh = gr.h > 0 ? gr.h : 1;

    for (int y = yStart; y < yEnd; ++y) {
        float lo[kSub], ro[kSub], li[kSub], ri[kSub];
        float minL = 1e9f, maxR = -1e9f, maxL = -1e9f, minR = 1e9f;
        bool allOuter = true, allInner = in != nullptr;
        float maxLi = -1e9f, minRi = 1e9f;
        for (int s = 0; s < kSub; ++s) {
            const float yc = y + (s + 0.5f) / kSub;
            span(o.x0, o.y0, o.x1, o.y1, o.r, yc, lo[s], ro[s]);
            if (lo[s] >= ro[s]) { allOuter = false; } else {
                minL = std::min(minL, lo[s]); maxR = std::max(maxR, ro[s]);
                maxL = std::max(maxL, lo[s]); minR = std::min(minR, ro[s]);
            }
            if (in) {
                span(in->x0, in->y0, in->x1, in->y1, in->r, yc, li[s], ri[s]);
                if (li[s] >= ri[s]) allInner = false;
                else { maxLi = std::max(maxLi, li[s]); minRi = std::min(minRi, ri[s]); }
            }
        }
        if (maxR < minL) continue;   // row empty

        const int xs = std::max(0, static_cast<int>(std::floor(minL)));
        const int xe = std::min(w_, static_cast<int>(std::ceil(maxR)));
        // Fully covered run (outer covers all sub-rows), fully empty run (inner covers all).
        const int fullA = allOuter ? static_cast<int>(std::ceil(maxL)) : 1 << 30;
        const int fullB = allOuter ? static_cast<int>(std::floor(minR)) : -(1 << 30);
        const int holeA = allInner ? static_cast<int>(std::ceil(maxLi)) : 1 << 30;
        const int holeB = allInner ? static_cast<int>(std::floor(minRi)) : -(1 << 30);

        uint32_t* row = &px_[static_cast<size_t>(y) * w_];
        const float ty = ((y + 0.5f) - gr.y) / gh;
        // Gradient: t(x) = (x - gr.x + 0.5) / (2 gw) + ty / 2, linear in x -> 16.16 fixed point.
        int32_t tFix = 0, tStep = 0;
        if (gradient) {
            tFix = static_cast<int32_t>(((((xs + 0.5f) - gr.x) / gw + ty) * 0.5f) * 65536.f);
            tStep = static_cast<int32_t>((0.5f / gw) * 65536.f);
        }
        for (int x = xs; x < xe; ++x, tFix += tStep) {
            if (in && x >= holeA && x < holeB) { tFix += tStep * (holeB - 1 - x); x = holeB - 1; continue; }
            float cov;
            if (!in && x >= fullA && x < fullB) {
                cov = 1.f;
            } else {
                float acc = 0;
                for (int s = 0; s < kSub; ++s) {
                    if (lo[s] < ro[s]) acc += overlap(static_cast<float>(x), x + 1.f, lo[s], ro[s]);
                    if (in && li[s] < ri[s]) acc -= overlap(static_cast<float>(x), x + 1.f, li[s], ri[s]);
                }
                cov = acc / kSub;
                if (cov <= 0.002f) continue;
            }
            Color c = from;
            if (gradient) {
                const int32_t t = tFix < 0 ? 0 : (tFix > 65536 ? 65536 : tFix);
                const int32_t d = kBayer[((y & 3) << 2) | (x & 3)] * 4096 + 2048;
                c = {lerpFix(from.r, to.r, t, d), lerpFix(from.g, to.g, t, d), lerpFix(from.b, to.b, t, d),
                     lerpFix(from.a, to.a, t, 32768)};
            }
            blend(row[x], c, static_cast<int>(c.a * cov + 0.5f));
        }
    }
}

void Canvas::fillRoundRect(const Rect& r, Color c, float radius) {
    if (r.w <= 0 || r.h <= 0 || c.a == 0) return;
    const float rad = std::max(0.f, std::min(radius, std::min(r.w, r.h) / 2));
    paint({r.x, r.y, r.x + r.w, r.y + r.h, rad}, nullptr, c, c, false, r);
}

void Canvas::gradientRoundRect(const Rect& r, Color from, Color to, float radius) {
    if (r.w <= 0 || r.h <= 0) return;
    const float rad = std::max(0.f, std::min(radius, std::min(r.w, r.h) / 2));
    paint({r.x, r.y, r.x + r.w, r.y + r.h, rad}, nullptr, from, to, true, r);
}

void Canvas::strokeRoundRect(const Rect& r, Color c, float width, float radius) {
    if (r.w <= 0 || r.h <= 0 || width <= 0 || c.a == 0) return;
    const float rad = std::max(0.f, std::min(radius, std::min(r.w, r.h) / 2));
    const Shape outer{r.x, r.y, r.x + r.w, r.y + r.h, rad};
    if (r.w <= 2 * width || r.h <= 2 * width) { paint(outer, nullptr, c, c, false, r); return; }
    const Shape inner{r.x + width, r.y + width, r.x + r.w - width, r.y + r.h - width, std::max(0.f, rad - width)};
    paint(outer, &inner, c, c, false, r);
}

void Canvas::blitMask(const uint8_t* mask, int stride, int sx, int sy, int w, int h, int dx, int dy, Color c) {
    for (int j = 0; j < h; ++j) {
        const int y = dy + j;
        if (y < 0 || y >= h_) continue;
        const uint8_t* m = mask + static_cast<size_t>(sy + j) * stride + sx;
        uint32_t* row = &px_[static_cast<size_t>(y) * w_];
        for (int i = 0; i < w; ++i) {
            const int x = dx + i;
            if (x < 0 || x >= w_ || m[i] == 0) continue;
            blend(row[x], c, (m[i] * c.a + 127) / 255);
        }
    }
}

}  // namespace henu
