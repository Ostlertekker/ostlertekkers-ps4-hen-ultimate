// PS4 HEN ULTIMATE - gfx/soft_renderer.cpp
#include "gfx/soft_renderer.h"

#include <algorithm>
#include <cmath>

namespace henu {

SoftwareRenderer::SoftwareRenderer(IFileSystem& fs, int width, int height)
    : fs_(fs), canvas_(width, height) {}

void SoftwareRenderer::beginFrame(Color clear) {
    texts_.clear();
    pendingClear_ = true;
    clearColor_ = clear;
}

void SoftwareRenderer::flushClear() {
    if (!pendingClear_) return;
    canvas_.clear(clearColor_);
    pendingClear_ = false;
}

void SoftwareRenderer::endFrame() {
    flushClear();
    ++frames_;
    if (present_) present_(canvas_);
}

void SoftwareRenderer::fillRect(const Rect& r, Color c, float radius) { flushClear();
    canvas_.fillRoundRect(r, c, radius); }

void SoftwareRenderer::gradientRect(const Rect& r, Color from, Color to, float radius) {
    flushClear();
    canvas_.gradientRoundRect(r, from, to, radius);
}

void SoftwareRenderer::strokeRect(const Rect& r, Color c, float width, float radius) {
    flushClear();
    canvas_.strokeRoundRect(r, c, width, radius);
}

Result<TextureId> SoftwareRenderer::loadPng(const std::string& path) {
    // PNG decoding (covers, backgrounds) follows with the modules that need it.
    return Result<TextureId>::fail(ErrorCode::Unsupported, "PNG textures not implemented yet: " + path);
}

void SoftwareRenderer::drawTexture(TextureId, const Rect&, float) {}
void SoftwareRenderer::freeTexture(TextureId) {}

Result<FontId> SoftwareRenderer::loadFont(const std::string& path, float size) {
    auto bytes = fs_.readFile(path, 8 * 1024 * 1024);
    if (!bytes) return Result<FontId>::fail(bytes.error().code, bytes.error().message);
    auto font = BitmapFont::parse(bytes.value());
    if (!font) return Result<FontId>::fail(font.error().code, path + ": " + font.error().message);
    if (std::lround(size) != font.value().pixelSize())
        return Result<FontId>::fail(ErrorCode::InvalidArgument, path + ": size mismatch");
    fonts_.push_back(std::move(font.value()));
    return Result<FontId>::ok(static_cast<FontId>(fonts_.size()));
}

void SoftwareRenderer::drawText(FontId f, const std::string& utf8, float x, float y, Color c, TextAlign align) {
    if (f == 0 || f > fonts_.size() || utf8.empty()) return;
    texts_.push_back(utf8);
    flushClear();
    const BitmapFont& font = fonts_[f - 1];
    float dx = x;
    if (align != TextAlign::Left) {
        const float w = font.measure(utf8);
        dx -= align == TextAlign::Center ? w / 2 : w;
    }
    font.draw(canvas_, utf8, dx, y, c);
}

float SoftwareRenderer::measureText(FontId f, const std::string& utf8) {
    if (f == 0 || f > fonts_.size()) return 0;
    return fonts_[f - 1].measure(utf8);
}

bool SoftwareRenderer::useCachedLayer(uint64_t key) {
    if (key != 0 && layer_ && layerKey_ == key) {
        canvas_.copyFrom(*layer_);   // replaces the pending clear
        pendingClear_ = false;
        return true;
    }
    flushClear();
    recordingKey_ = key;
    return false;
}

void SoftwareRenderer::endLayer() {
    if (recordingKey_ == 0) return;
    if (!layer_) layer_.reset(new Canvas(canvas_.width(), canvas_.height()));
    layer_->copyFrom(canvas_);
    layerKey_ = recordingKey_;
    recordingKey_ = 0;
}

// ------------------------------------------------------------------ PNG
namespace {
uint32_t crcTable[256];
bool crcInit = false;
uint32_t crc32(const unsigned char* d, size_t n, uint32_t c = 0xFFFFFFFFu) {
    if (!crcInit) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t v = i;
            for (int k = 0; k < 8; ++k) v = (v & 1) ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crcTable[i] = v;
        }
        crcInit = true;
    }
    for (size_t i = 0; i < n; ++i) c = crcTable[(c ^ d[i]) & 0xFF] ^ (c >> 8);
    return c;
}
void be32(std::string& s, uint32_t v) {
    s += static_cast<char>(v >> 24); s += static_cast<char>(v >> 16);
    s += static_cast<char>(v >> 8);  s += static_cast<char>(v);
}
void chunk(std::string& out, const char* type, const std::string& data) {
    be32(out, static_cast<uint32_t>(data.size()));
    std::string td = std::string(type, 4) + data;
    out += td;
    be32(out, crc32(reinterpret_cast<const unsigned char*>(td.data()), td.size()) ^ 0xFFFFFFFFu);
}
}  // namespace

std::string encodePng(const Canvas& c) {
    const int w = c.width(), h = c.height();
    std::string raw;
    raw.reserve(static_cast<size_t>(h) * (w * 3 + 1));
    for (int y = 0; y < h; ++y) {
        raw += '\0';   // filter: none
        for (int x = 0; x < w; ++x) {
            uint32_t p = c.pixel(x, y);
            raw += static_cast<char>((p >> 16) & 0xFF);
            raw += static_cast<char>((p >> 8) & 0xFF);
            raw += static_cast<char>(p & 0xFF);
        }
    }
    // zlib stream with stored blocks
    std::string z = "\x78\x01";
    uint32_t a = 1, b = 0;
    for (unsigned char ch : raw) { a = (a + ch) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw.size() || off == 0;) {
        const size_t n = std::min<size_t>(65535, raw.size() - off);
        const bool last = off + n >= raw.size();
        z += static_cast<char>(last ? 1 : 0);
        z += static_cast<char>(n & 0xFF); z += static_cast<char>(n >> 8);
        z += static_cast<char>(~n & 0xFF); z += static_cast<char>((~n >> 8) & 0xFF);
        z.append(raw, off, n);
        off += n;
        if (last) break;
    }
    be32(z, (b << 16) | a);

    std::string png = "\x89PNG\r\n\x1a\n";
    std::string ihdr;
    be32(ihdr, static_cast<uint32_t>(w));
    be32(ihdr, static_cast<uint32_t>(h));
    ihdr += '\x08'; ihdr += '\x02'; ihdr += '\0'; ihdr += '\0'; ihdr += '\0';   // 8-bit RGB
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", "");
    return png;
}

}  // namespace henu
