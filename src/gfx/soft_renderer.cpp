// OSTLERTEKKERS HEN - gfx/soft_renderer.cpp
#include "gfx/soft_renderer.h"

#include <algorithm>
#include <cmath>

#include "gfx/png_decode.h"

namespace henu {

SoftwareRenderer::SoftwareRenderer(IFileSystem& fs, int width, int height)
    : fs_(fs), canvas_(width, height) {}

SoftwareRenderer::~SoftwareRenderer() { setThreads(1); }

void SoftwareRenderer::setThreads(int n) {
    n = std::max(1, std::min(n, 8));
    if (n == threads()) return;
    {
        std::lock_guard<std::mutex> l(m_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) t.join();
    workers_.clear();
    stop_ = false;
    for (int i = 1; i < n; ++i) workers_.emplace_back([this]() { workerLoop(); });
}

const Canvas& SoftwareRenderer::canvas() const {
    flush();
    return canvas_;
}

void SoftwareRenderer::beginFrame(Color clear) {
    texts_.clear();
    cmds_.clear();
    pendingClear_ = true;
    clearColor_ = clear;
}

void SoftwareRenderer::flushClear() {
    if (!pendingClear_) return;
    Cmd c{};
    c.kind = Cmd::Clear;
    c.a = clearColor_;
    push(std::move(c));
    pendingClear_ = false;
}

void SoftwareRenderer::push(Cmd c) {
    const int h = canvas_.height();
    if (c.kind == Cmd::Clear || c.kind == Cmd::CopyLayer) {
        c.y0 = 0;
        c.y1 = h;
    } else if (c.kind == Cmd::Path) {
        float lo = 1e9f, hi = -1e9f;
        for (const auto& ct : c.path)
            for (const auto& p : ct) { lo = std::min(lo, p.y); hi = std::max(hi, p.y); }
        c.y0 = static_cast<int>(std::floor(lo)) - 1;
        c.y1 = static_cast<int>(std::ceil(hi)) + 1;
    } else {
        c.y0 = static_cast<int>(std::floor(c.r.y)) - 1;
        c.y1 = static_cast<int>(std::ceil(c.r.y + c.r.h)) + 1;
    }
    c.y0 = std::max(0, c.y0);
    c.y1 = std::min(h, c.y1);
    if (c.y1 <= c.y0) return;
    cmds_.push_back(std::move(c));
}

void SoftwareRenderer::run(Canvas& band, const Cmd& c) const {
    switch (c.kind) {
        case Cmd::Clear: band.clear(c.a); break;
        case Cmd::Fill: band.fillRoundRect(c.r, c.a, c.f); break;
        case Cmd::Gradient: band.gradientRoundRect(c.r, c.a, c.b, c.f); break;
        case Cmd::Stroke: band.strokeRoundRect(c.r, c.a, c.f2, c.f); break;
        case Cmd::Path: band.fillPath(c.path, c.a); break;
        case Cmd::Text: fonts_[c.id - 1].draw(band, c.text, c.r.x, c.f, c.a); break;
        case Cmd::Texture: {
            const Tex& t = textures_[c.id - 1];
            band.drawImage(t.px.data(), t.w, t.h, c.r, c.f, c.f2);
            break;
        }
        case Cmd::CopyLayer: if (layer_) band.copyFrom(*layer_); break;
    }
}

void SoftwareRenderer::workerLoop() {
    uint64_t seen = 0;
    for (;;) {
        {
            std::unique_lock<std::mutex> l(m_);
            cv_.wait(l, [&]() { return stop_ || job_ != seen; });
            if (stop_) return;
            seen = job_;
        }
        const int h = canvas_.height();
        for (int b; (b = nextBand_.fetch_add(1)) < bands_;) {
            Canvas band(canvas_, b * h / bands_, (b + 1) * h / bands_);
            for (const Cmd& c : cmds_)
                if (c.y1 > band.bandBegin() && c.y0 < band.bandEnd()) run(band, c);
        }
        {
            std::lock_guard<std::mutex> l(m_);
            if (--busy_ == 0) doneCv_.notify_all();
        }
    }
}

void SoftwareRenderer::flush() const {
    if (cmds_.empty()) return;
    if (workers_.empty() || cmds_.size() < 2) {
        for (const Cmd& c : cmds_) run(canvas_, c);
        cmds_.clear();
        return;
    }
    const int h = canvas_.height();
    {
        std::lock_guard<std::mutex> l(m_);
        bands_ = std::max(1, std::min(h, threads() * 6));
        nextBand_ = 0;
        busy_ = static_cast<int>(workers_.size());
        ++job_;
    }
    cv_.notify_all();
    for (int b; (b = nextBand_.fetch_add(1)) < bands_;) {
        Canvas band(canvas_, b * h / bands_, (b + 1) * h / bands_);
        for (const Cmd& c : cmds_)
            if (c.y1 > band.bandBegin() && c.y0 < band.bandEnd()) run(band, c);
    }
    {
        std::unique_lock<std::mutex> l(m_);
        doneCv_.wait(l, [&]() { return busy_ == 0; });
    }
    cmds_.clear();
}

void SoftwareRenderer::endFrame() {
    flushClear();
    flush();
    ++frames_;
    if (present_) present_(canvas_);
}

void SoftwareRenderer::fillRect(const Rect& r, Color c, float radius) {
    if (r.w <= 0 || r.h <= 0 || c.a == 0) return;
    flushClear();
    Cmd k{};
    k.kind = Cmd::Fill;
    k.r = r;
    k.a = c;
    k.f = radius;
    push(std::move(k));
}

void SoftwareRenderer::gradientRect(const Rect& r, Color from, Color to, float radius) {
    flushClear();
    Cmd k{};
    k.kind = Cmd::Gradient;
    k.r = r;
    k.a = from;
    k.b = to;
    k.f = radius;
    push(std::move(k));
}

void SoftwareRenderer::strokeRect(const Rect& r, Color c, float width, float radius) {
    flushClear();
    Cmd k{};
    k.kind = Cmd::Stroke;
    k.r = r;
    k.a = c;
    k.f2 = width;
    k.f = radius;
    push(std::move(k));
}

Result<TextureId> SoftwareRenderer::loadPng(const std::string& path) {
    auto bytes = fs_.readFile(path, 16 * 1024 * 1024);
    if (!bytes) return Result<TextureId>::fail(bytes.error().code, bytes.error().message);
    auto img = decodePng(bytes.value());
    if (!img) return Result<TextureId>::fail(img.error().code, path + ": " + img.error().message);
    return Result<TextureId>::ok(createTexture(img.value().w, img.value().h, img.value().px.data()));
}

TextureId SoftwareRenderer::createTexture(int w, int h, const uint32_t* argb) {
    if (w <= 0 || h <= 0 || !argb) return 0;
    flush();   // pending commands may still reference textures_
    Tex t;
    t.w = w;
    t.h = h;
    t.px.assign(argb, argb + static_cast<size_t>(w) * h);
    for (size_t i = 0; i < textures_.size(); ++i)
        if (textures_[i].w == 0) { textures_[i] = std::move(t); return static_cast<TextureId>(i + 1); }
    textures_.push_back(std::move(t));
    return static_cast<TextureId>(textures_.size());
}

void SoftwareRenderer::drawTexture(TextureId id, const Rect& dst, float alpha, float radius) {
    if (id == 0 || id > textures_.size() || textures_[id - 1].w == 0) return;
    flushClear();
    Cmd k{};
    k.kind = Cmd::Texture;
    k.r = dst;
    k.f = alpha;
    k.f2 = radius;
    k.id = id;
    push(std::move(k));
}

void SoftwareRenderer::freeTexture(TextureId id) {
    if (id == 0 || id > textures_.size()) return;
    flush();
    textures_[id - 1] = Tex();
}

void SoftwareRenderer::fillPath(const std::vector<std::vector<Pt>>& contours, Color c) {
    if (c.a == 0 || contours.empty()) return;
    flushClear();
    Cmd k{};
    k.kind = Cmd::Path;
    k.a = c;
    k.path = contours;
    push(std::move(k));
}

Result<FontId> SoftwareRenderer::loadFont(const std::string& path, float size) {
    auto bytes = fs_.readFile(path, 8 * 1024 * 1024);
    if (!bytes) return Result<FontId>::fail(bytes.error().code, bytes.error().message);
    auto font = BitmapFont::parse(bytes.value());
    if (!font) return Result<FontId>::fail(font.error().code, path + ": " + font.error().message);
    if (std::lround(size) != font.value().pixelSize())
        return Result<FontId>::fail(ErrorCode::InvalidArgument, path + ": size mismatch");
    flush();
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
    Cmd k{};
    k.kind = Cmd::Text;
    k.r = {dx, y - font.lineHeight(), 0, 3.f * font.lineHeight()};   // rows only
    k.f = y;
    k.a = c;
    k.id = f;
    k.text = utf8;
    push(std::move(k));
}

float SoftwareRenderer::measureText(FontId f, const std::string& utf8) {
    if (f == 0 || f > fonts_.size()) return 0;
    return fonts_[f - 1].measure(utf8);
}

bool SoftwareRenderer::useCachedLayer(uint64_t key) {
    if (key != 0 && layer_ && layerKey_ == key) {
        cmds_.clear();               // replaces the pending clear
        pendingClear_ = false;
        Cmd k{};
        k.kind = Cmd::CopyLayer;
        push(std::move(k));
        return true;
    }
    flushClear();
    recordingKey_ = key;
    return false;
}

void SoftwareRenderer::endLayer() {
    if (recordingKey_ == 0) return;
    flush();
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
