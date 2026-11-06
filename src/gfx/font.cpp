// PS4 HEN ULTIMATE - gfx/font.cpp
#include "gfx/font.h"

#include <cmath>
#include <cstring>

#include "gfx/canvas.h"

namespace henu {

namespace {
class Reader {
public:
    explicit Reader(const std::string& b) : b_(b) {}
    bool ok() const { return ok_; }
    size_t pos() const { return p_; }
    template <typename T> T get() {
        T v{};
        if (p_ + sizeof(T) > b_.size()) { ok_ = false; return v; }
        std::memcpy(&v, b_.data() + p_, sizeof(T));   // file is little endian, as are host and PS4
        p_ += sizeof(T);
        return v;
    }
private:
    const std::string& b_;
    size_t p_ = 0;
    bool ok_ = true;
};
}  // namespace

std::vector<uint32_t> decodeUtf8(const std::string& s) {
    std::vector<uint32_t> out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t cp;
        size_t n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 0x6) { cp = c & 0x1F; n = 2; }
        else if ((c >> 4) == 0xE) { cp = c & 0x0F; n = 3; }
        else if ((c >> 3) == 0x1E) { cp = c & 0x07; n = 4; }
        else { out.push_back(0xFFFD); ++i; continue; }
        if (i + n > s.size()) { out.push_back(0xFFFD); break; }
        bool bad = false;
        for (size_t k = 1; k < n; ++k) {
            const unsigned char cc = static_cast<unsigned char>(s[i + k]);
            if ((cc >> 6) != 0x2) { bad = true; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (bad) { out.push_back(0xFFFD); ++i; continue; }
        out.push_back(cp);
        i += n;
    }
    return out;
}

Result<BitmapFont> BitmapFont::parse(const std::string& bytes) {
    using R = Result<BitmapFont>;
    if (bytes.size() < 28 || bytes.compare(0, 4, "HFNT") != 0) return R::fail(ErrorCode::ParseError, "not an HFNT file");
    Reader r(bytes);
    r.get<uint32_t>();   // magic
    if (r.get<uint32_t>() != 1) return R::fail(ErrorCode::ParseError, "unsupported HFNT version");
    BitmapFont f;
    f.size_ = r.get<uint16_t>();
    f.ascent_ = r.get<int16_t>();
    f.descent_ = r.get<int16_t>();
    f.line_ = r.get<int16_t>();
    const uint32_t nGlyph = r.get<uint32_t>(), nKern = r.get<uint32_t>();
    f.atlasW_ = r.get<uint16_t>();
    f.atlasH_ = r.get<uint16_t>();
    if (!r.ok() || nGlyph > 20000 || nKern > 200000 || f.atlasW_ == 0 || f.atlasH_ == 0)
        return R::fail(ErrorCode::ParseError, "bad HFNT header");
    for (uint32_t i = 0; i < nGlyph; ++i) {
        uint32_t cp = r.get<uint32_t>();
        Glyph g;
        g.x = r.get<uint16_t>(); g.y = r.get<uint16_t>(); g.w = r.get<uint16_t>(); g.h = r.get<uint16_t>();
        g.bx = r.get<int16_t>(); g.by = r.get<int16_t>(); g.adv64 = r.get<int32_t>();
        if (g.x + g.w > f.atlasW_ || g.y + g.h > f.atlasH_) return R::fail(ErrorCode::ParseError, "glyph outside atlas");
        f.glyphs_[cp] = g;
    }
    for (uint32_t i = 0; i < nKern; ++i) {
        uint32_t a = r.get<uint32_t>(), b = r.get<uint32_t>();
        int32_t adj = r.get<int32_t>();
        f.kerns_[(static_cast<uint64_t>(a) << 32) | b] = adj;
    }
    const size_t atlasBytes = static_cast<size_t>(f.atlasW_) * f.atlasH_;
    if (!r.ok() || r.pos() + atlasBytes != bytes.size()) return R::fail(ErrorCode::ParseError, "HFNT size mismatch");
    f.atlas_.assign(bytes.begin() + static_cast<long>(r.pos()), bytes.end());
    return R::ok(std::move(f));
}

const BitmapFont::Glyph* BitmapFont::glyph(uint32_t cp) const {
    auto it = glyphs_.find(cp);
    if (it != glyphs_.end()) return &it->second;
    it = glyphs_.find('?');
    return it != glyphs_.end() ? &it->second : nullptr;
}

int32_t BitmapFont::kern(uint32_t a, uint32_t b) const {
    auto it = kerns_.find((static_cast<uint64_t>(a) << 32) | b);
    return it == kerns_.end() ? 0 : it->second;
}

float BitmapFont::measure(const std::string& utf8) const {
    int64_t w = 0;
    uint32_t prev = 0;
    for (uint32_t cp : decodeUtf8(utf8)) {
        const Glyph* g = glyph(cp);
        if (!g) continue;
        if (prev) w += kern(prev, cp);
        w += g->adv64;
        prev = cp;
    }
    return static_cast<float>(w) / 64.f;
}

void BitmapFont::draw(Canvas& c, const std::string& utf8, float x, float y, Color color) const {
    int64_t pen = static_cast<int64_t>(std::lround(x * 64));
    const int top = static_cast<int>(std::lround(y));
    uint32_t prev = 0;
    for (uint32_t cp : decodeUtf8(utf8)) {
        const Glyph* g = glyph(cp);
        if (!g) continue;
        if (prev) pen += kern(prev, cp);
        if (g->w && g->h) {
            const int gx = static_cast<int>((pen + 32) >> 6) + g->bx;
            c.blitMask(atlas_.data(), atlasW_, g->x, g->y, g->w, g->h, gx, top + g->by, color);
        }
        pen += g->adv64;
        prev = cp;
    }
}

}  // namespace henu
