// PS4 HEN ULTIMATE - gfx/font.h
// Pre-rendered bitmap font (.hfnt, made by scripts/make_fonts.py).
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/result.h"

namespace henu {

class Canvas;
struct Color;

class BitmapFont {
public:
    struct Glyph { uint16_t x, y, w, h; int16_t bx, by; int32_t adv64; };

    static Result<BitmapFont> parse(const std::string& bytes);

    int pixelSize() const { return size_; }
    int ascent() const { return ascent_; }
    int lineHeight() const { return line_; }
    size_t glyphCount() const { return glyphs_.size(); }

    float measure(const std::string& utf8) const;
    // Draws with the top of the line box at y.
    void draw(Canvas& c, const std::string& utf8, float x, float y, Color color) const;

private:
    const Glyph* glyph(uint32_t cp) const;
    int32_t kern(uint32_t a, uint32_t b) const;

    int size_ = 0, ascent_ = 0, descent_ = 0, line_ = 0;
    int atlasW_ = 0, atlasH_ = 0;
    std::unordered_map<uint32_t, Glyph> glyphs_;
    std::unordered_map<uint64_t, int32_t> kerns_;
    std::vector<uint8_t> atlas_;
};

// UTF-8 -> code points; invalid bytes become U+FFFD.
std::vector<uint32_t> decodeUtf8(const std::string& s);

}  // namespace henu
