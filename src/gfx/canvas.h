// OSTLERTEKKERS HEN - gfx/canvas.h
// CPU rasterizer on a 32-bit A8R8G8B8 pixel buffer (same layout the PS4
// VideoOut framebuffer uses). Anti-aliased rounded rectangles via 4 vertical
// sub-samples with exact horizontal coverage per sub-row.
#pragma once

#include <cstdint>
#include <vector>

#include "platform/platform.h"

namespace henu {

class Canvas {
public:
    Canvas(int w, int h);
    // Non-owning view of rows [y0, y1) of `parent`: every draw call only
    // touches those rows. Views of disjoint bands can draw in parallel.
    Canvas(Canvas& parent, int y0, int y1);
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    int width() const { return w_; }
    int height() const { return h_; }
    int bandBegin() const { return cy0_; }
    int bandEnd() const { return cy1_; }
    uint32_t* data() { return p_; }
    const uint32_t* data() const { return p_; }
    uint32_t pixel(int x, int y) const { return p_[static_cast<size_t>(y) * w_ + x]; }

    void clear(Color c);
    void fillRoundRect(const Rect& r, Color c, float radius);
    void gradientRoundRect(const Rect& r, Color from, Color to, float radius);   // diagonal TL -> BR
    void strokeRoundRect(const Rect& r, Color c, float width, float radius);
    // 8-bit coverage mask (e.g. glyph) tinted with c, top-left at (dx, dy).
    void blitMask(const uint8_t* mask, int stride, int sx, int sy, int w, int h, int dx, int dy, Color c);
    void copyFrom(const Canvas& other);
    // Even-odd fill of closed polygons, 4 sub-rows with exact horizontal coverage.
    void fillPath(const std::vector<std::vector<IRenderer::Pt>>& contours, Color c);
    // Bilinear-scaled image (A8R8G8B8, straight alpha) into dst, optional rounded clip.
    void drawImage(const uint32_t* px, int iw, int ih, const Rect& dst, float alpha, float radius);

    static uint32_t pack(Color c) {
        return (static_cast<uint32_t>(c.a) << 24) | (static_cast<uint32_t>(c.r) << 16) |
               (static_cast<uint32_t>(c.g) << 8) | c.b;
    }
    static Color unpack(uint32_t v) {
        return {static_cast<uint8_t>(v >> 16), static_cast<uint8_t>(v >> 8), static_cast<uint8_t>(v),
                static_cast<uint8_t>(v >> 24)};
    }

private:
    struct Shape { float x0, y0, x1, y1, r; };
    void paint(const Shape& outer, const Shape* inner, Color from, Color to, bool gradient, const Rect& gr);
    static void blend(uint32_t& dst, Color c, int a8);
    // Constant colour over n pixels (a8 = 0..255).
    static void blendSpan(uint32_t* dst, int n, Color c, int a8);

    int w_, h_;
    int cy0_ = 0, cy1_ = 0;    // writable rows
    std::vector<uint32_t> own_;
    uint32_t* p_ = nullptr;
};

}  // namespace henu
