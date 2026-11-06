// OSTLERTEKKERS HEN - gfx/canvas.cpp
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

Canvas::Canvas(int w, int h)
    : w_(w), h_(h), cy0_(0), cy1_(h), own_(static_cast<size_t>(w) * h, 0xFF000000u), p_(own_.data()) {}

Canvas::Canvas(Canvas& parent, int y0, int y1)
    : w_(parent.w_), h_(parent.h_), cy0_(std::max(parent.cy0_, y0)), cy1_(std::min(parent.cy1_, y1)), p_(parent.p_) {
    if (cy1_ < cy0_) cy1_ = cy0_;
}

void Canvas::clear(Color c) {
    c.a = 255;
    std::fill(p_ + static_cast<size_t>(cy0_) * w_, p_ + static_cast<size_t>(cy1_) * w_, pack(c));
}

void Canvas::copyFrom(const Canvas& o) {
    if (o.w_ != w_ || o.h_ != h_) return;
    std::memcpy(p_ + static_cast<size_t>(cy0_) * w_, o.p_ + static_cast<size_t>(cy0_) * w_,
                static_cast<size_t>(cy1_ - cy0_) * w_ * sizeof(uint32_t));
}

// x / 255 for two 8-bit channels packed in 0x00FF00FF lanes (exact for x <= 255 * 255).
static inline uint32_t div255x2(uint32_t v) {
    v += 0x00800080u;
    return ((v + ((v >> 8) & 0x00FF00FFu)) >> 8) & 0x00FF00FFu;
}

void Canvas::blend(uint32_t& dst, Color c, int a8) {
    if (a8 <= 0) return;
    if (a8 >= 255) { dst = 0xFF000000u | (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b; return; }
    const uint32_t a = static_cast<uint32_t>(a8), ia = 255 - a;
    const uint32_t d = dst;
    const uint32_t rb = div255x2(((d & 0x00FF00FFu) * ia) + ((static_cast<uint32_t>(c.r) << 16) | c.b) * a);
    const uint32_t g = div255x2((((d >> 8) & 0xFFu) * ia) + static_cast<uint32_t>(c.g) * a);
    dst = 0xFF000000u | rb | (g << 8);
}

void Canvas::blendSpan(uint32_t* dst, int n, Color c, int a8) {
    if (n <= 0 || a8 <= 0) return;
    if (a8 >= 255) {
        std::fill(dst, dst + n, 0xFF000000u | (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b);
        return;
    }
    const uint32_t a = static_cast<uint32_t>(a8), ia = 255 - a;
    const uint32_t srb = ((static_cast<uint32_t>(c.r) << 16) | c.b) * a, sg = static_cast<uint32_t>(c.g) * a;
    for (int i = 0; i < n; ++i) {
        const uint32_t d = dst[i];
        const uint32_t rb = div255x2((d & 0x00FF00FFu) * ia + srb);
        const uint32_t g = div255x2(((d >> 8) & 0xFFu) * ia + sg);
        dst[i] = 0xFF000000u | rb | (g << 8);
    }
}

void Canvas::paint(const Shape& o, const Shape* in, Color from, Color to, bool gradient, const Rect& gr) {
    const int yStart = std::max(cy0_, static_cast<int>(std::floor(o.y0)));
    const int yEnd = std::min(cy1_, static_cast<int>(std::ceil(o.y1)));
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

        uint32_t* row = &p_[static_cast<size_t>(y) * w_];
        const float ty = ((y + 0.5f) - gr.y) / gh;
        // Gradient: t(x) = (x - gr.x + 0.5) / (2 gw) + ty / 2, linear in x -> 16.16 fixed point.
        int32_t tFix = 0, tStep = 0;
        if (gradient) {
            tFix = static_cast<int32_t>(((((xs + 0.5f) - gr.x) / gw + ty) * 0.5f) * 65536.f);
            tStep = static_cast<int32_t>((0.5f / gw) * 65536.f);
        }
        for (int x = xs; x < xe; ++x, tFix += tStep) {
            if (in && x >= holeA && x < holeB) { tFix += tStep * (holeB - 1 - x); x = holeB - 1; continue; }
            if (!gradient && !in && x == fullA && fullB > fullA) {
                const int e = std::min(fullB, xe);
                blendSpan(row + x, e - x, from, from.a);
                x = e - 1;
                continue;
            }
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
        if (y < cy0_ || y >= cy1_) continue;
        const uint8_t* m = mask + static_cast<size_t>(sy + j) * stride + sx;
        uint32_t* row = &p_[static_cast<size_t>(y) * w_];
        for (int i = 0; i < w; ++i) {
            const int x = dx + i;
            if (x < 0 || x >= w_ || m[i] == 0) continue;
            blend(row[x], c, (m[i] * c.a + 127) / 255);
        }
    }
}

void Canvas::fillPath(const std::vector<std::vector<IRenderer::Pt>>& contours, Color c) {
    if (c.a == 0) return;
    // Edge list (top-down), sorted by top y; scanned with an active edge list.
    struct Edge { float y0, y1, x0, dxdy; };
    std::vector<Edge> edges;
    float minY = 1e9f, maxY = -1e9f;
    for (const auto& ct : contours) {
        const size_t n = ct.size();
        for (size_t i = 0; i < n; ++i) {
            IRenderer::Pt a = ct[i], b = ct[(i + 1) % n];
            if (a.y == b.y) continue;
            if (a.y > b.y) std::swap(a, b);
            edges.push_back({a.y, b.y, a.x, (b.x - a.x) / (b.y - a.y)});
            minY = std::min(minY, a.y);
            maxY = std::max(maxY, b.y);
        }
    }
    const int yStart = std::max(cy0_, static_cast<int>(std::floor(minY)));
    const int yEnd = std::min(cy1_, static_cast<int>(std::ceil(maxY)));
    if (yEnd <= yStart || edges.empty()) return;
    std::sort(edges.begin(), edges.end(), [](const Edge& l, const Edge& r) { return l.y0 < r.y0; });
    std::vector<const Edge*> active;
    size_t nextEdge = 0;
    std::vector<float> xs;
    std::vector<float> cov(static_cast<size_t>(w_) + 2, 0.f);
    for (int y = yStart; y < yEnd; ++y) {
        int lo = w_, hi = -1;
        for (int s = 0; s < kSub; ++s) {
            const float yc = y + (s + 0.5f) / kSub;
            while (nextEdge < edges.size() && edges[nextEdge].y0 <= yc) active.push_back(&edges[nextEdge++]);
            xs.clear();
            size_t keep = 0;
            for (size_t i = 0; i < active.size(); ++i) {
                const Edge* e = active[i];
                if (e->y1 <= yc) continue;          // finished: drop
                active[keep++] = e;
                xs.push_back(e->x0 + (yc - e->y0) * e->dxdy);
            }
            active.resize(keep);
            std::sort(xs.begin(), xs.end());
            for (size_t i = 0; i + 1 < xs.size(); i += 2) {
                const float x0 = std::max(0.f, xs[i]), x1 = std::min(static_cast<float>(w_), xs[i + 1]);
                if (x1 <= x0) continue;
                const int ix0 = static_cast<int>(x0), ix1 = static_cast<int>(x1);
                lo = std::min(lo, ix0);
                hi = std::max(hi, std::min(ix1, w_ - 1));
                if (ix0 == ix1) { cov[static_cast<size_t>(ix0)] += x1 - x0; continue; }
                cov[static_cast<size_t>(ix0)] += (ix0 + 1) - x0;
                for (int x = ix0 + 1; x < ix1; ++x) cov[static_cast<size_t>(x)] += 1.f;
                if (ix1 < w_) cov[static_cast<size_t>(ix1)] += x1 - ix1;
            }
        }
        if (hi < lo) continue;
        uint32_t* row = &p_[static_cast<size_t>(y) * w_];
        for (int x = lo; x <= hi; ++x) {
            float v = cov[static_cast<size_t>(x)] / kSub;
            cov[static_cast<size_t>(x)] = 0;
            if (v <= 0.002f) continue;
            if (v > 1) v = 1;
            blend(row[x], c, static_cast<int>(c.a * v + 0.5f));
        }
    }
}

void Canvas::drawImage(const uint32_t* px, int iw, int ih, const Rect& dst, float alpha, float radius) {
    if (!px || iw <= 0 || ih <= 0 || dst.w <= 0 || dst.h <= 0 || alpha <= 0) return;
    const int x0 = std::max(0, static_cast<int>(std::floor(dst.x)));
    const int y0 = std::max(cy0_, static_cast<int>(std::floor(dst.y)));
    const int x1 = std::min(w_, static_cast<int>(std::ceil(dst.x + dst.w)));
    const int y1 = std::min(cy1_, static_cast<int>(std::ceil(dst.y + dst.h)));
    const float rad = std::max(0.f, std::min(radius, std::min(dst.w, dst.h) / 2));
    const float sx = iw / dst.w, sy = ih / dst.h;
    const int a255 = static_cast<int>(std::min(1.f, alpha) * 255 + 0.5f);
    for (int y = y0; y < y1; ++y) {
        float l = dst.x, r = dst.x + dst.w;
        if (rad > 0) span(dst.x, dst.y, dst.x + dst.w, dst.y + dst.h, rad, y + 0.5f, l, r);
        if (l >= r) continue;
        const float fy = (y + 0.5f - dst.y) * sy - 0.5f;
        const int iy = std::max(0, std::min(ih - 1, static_cast<int>(std::floor(fy))));
        const int iy2 = std::min(ih - 1, iy + 1);
        const int wy = static_cast<int>((fy - std::floor(fy)) * 256) & 255;
        uint32_t* row = &p_[static_cast<size_t>(y) * w_];
        for (int x = x0; x < x1; ++x) {
            const float xc = x + 0.5f;
            if (xc < l || xc > r) continue;
            const float fx = (xc - dst.x) * sx - 0.5f;
            const int ix = std::max(0, std::min(iw - 1, static_cast<int>(std::floor(fx))));
            const int ix2 = std::min(iw - 1, ix + 1);
            const int wx = static_cast<int>((fx - std::floor(fx)) * 256) & 255;
            const uint32_t p00 = px[static_cast<size_t>(iy) * iw + ix], p10 = px[static_cast<size_t>(iy) * iw + ix2];
            const uint32_t p01 = px[static_cast<size_t>(iy2) * iw + ix], p11 = px[static_cast<size_t>(iy2) * iw + ix2];
            auto ch = [&](int shift) {
                const int a = (p00 >> shift) & 0xFF, b = (p10 >> shift) & 0xFF;
                const int c0 = (p01 >> shift) & 0xFF, d = (p11 >> shift) & 0xFF;
                const int top = a * (256 - wx) + b * wx, bot = c0 * (256 - wx) + d * wx;
                return (top * (256 - wy) + bot * wy) >> 16;
            };
            const Color c{static_cast<uint8_t>(ch(16)), static_cast<uint8_t>(ch(8)), static_cast<uint8_t>(ch(0)), 255};
            blend(row[x], c, ch(24) * a255 / 255);
        }
    }
}

}  // namespace henu
