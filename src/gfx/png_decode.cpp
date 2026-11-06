// OSTLERTEKKERS HEN - gfx/png_decode.cpp
#include "gfx/png_decode.h"

#include <algorithm>
#include <cstring>

namespace henu {

namespace {

// ---------------------------------------------------------------- inflate
struct BitReader {
    const uint8_t* p;
    size_t n, pos = 0;
    uint32_t bitBuf = 0;
    int bitCnt = 0;
    bool err = false;
    int bit() {
        if (bitCnt == 0) {
            if (pos >= n) { err = true; return 0; }
            bitBuf = p[pos++];
            bitCnt = 8;
        }
        int b = bitBuf & 1;
        bitBuf >>= 1;
        --bitCnt;
        return b;
    }
    uint32_t bits(int count) {
        uint32_t v = 0;
        for (int i = 0; i < count; ++i) v |= static_cast<uint32_t>(bit()) << i;
        return v;
    }
    void alignByte() { bitCnt = 0; }
};

struct Huffman {
    uint16_t counts[16] = {0};
    uint16_t symbols[320] = {0};
    bool build(const uint8_t* lengths, int n) {
        std::memset(counts, 0, sizeof(counts));
        for (int i = 0; i < n; ++i) counts[lengths[i]]++;
        counts[0] = 0;
        int left = 1;
        for (int len = 1; len < 16; ++len) {
            left <<= 1;
            left -= counts[len];
            if (left < 0) return false;   // over-subscribed
        }
        uint16_t offs[16];
        offs[1] = 0;
        for (int len = 1; len < 15; ++len) offs[len + 1] = static_cast<uint16_t>(offs[len] + counts[len]);
        for (int i = 0; i < n; ++i)
            if (lengths[i]) symbols[offs[lengths[i]]++] = static_cast<uint16_t>(i);
        return true;
    }
    int decode(BitReader& br) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= br.bit();
            int count = counts[len];
            if (code - count < first) return symbols[index + (code - first)];
            index += count;
            first += count;
            first <<= 1;
            code <<= 1;
            if (br.err) return -1;
        }
        return -1;
    }
};

const uint16_t kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                               35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const uint16_t kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
                                257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool inflateBlock(BitReader& br, std::string& out, const Huffman& lit, const Huffman& dist, size_t maxOut) {
    for (;;) {
        int sym = lit.decode(br);
        if (sym < 0 || br.err) return false;
        if (sym < 256) {
            if (out.size() >= maxOut) return false;
            out.push_back(static_cast<char>(sym));
        } else if (sym == 256) {
            return true;
        } else {
            sym -= 257;
            if (sym >= 29) return false;
            size_t len = kLenBase[sym] + br.bits(kLenExtra[sym]);
            int ds = dist.decode(br);
            if (ds < 0 || ds >= 30) return false;
            size_t d = kDistBase[ds] + br.bits(kDistExtra[ds]);
            if (d > out.size() || out.size() + len > maxOut || br.err) return false;
            size_t start = out.size() - d;
            for (size_t i = 0; i < len; ++i) out.push_back(out[start + i]);
        }
    }
}

}  // namespace

Result<std::string> rawInflate(const uint8_t* data, size_t len, size_t maxOut, size_t* consumed) {
    using R = Result<std::string>;
    BitReader br{data, len};
    std::string out;
    Huffman fixedLit, fixedDist;
    {
        uint8_t l[288];
        for (int i = 0; i < 144; ++i) l[i] = 8;
        for (int i = 144; i < 256; ++i) l[i] = 9;
        for (int i = 256; i < 280; ++i) l[i] = 7;
        for (int i = 280; i < 288; ++i) l[i] = 8;
        fixedLit.build(l, 288);
        uint8_t d[30];
        for (int i = 0; i < 30; ++i) d[i] = 5;
        fixedDist.build(d, 30);
    }
    bool last = false;
    while (!last) {
        last = br.bit();
        int type = static_cast<int>(br.bits(2));
        if (br.err) return R::fail(ErrorCode::ParseError, "deflate: truncated");
        if (type == 0) {
            br.alignByte();
            if (br.pos + 4 > len) return R::fail(ErrorCode::ParseError, "deflate: truncated stored block");
            uint16_t n = static_cast<uint16_t>(data[br.pos] | (data[br.pos + 1] << 8));
            uint16_t nn = static_cast<uint16_t>(data[br.pos + 2] | (data[br.pos + 3] << 8));
            br.pos += 4;
            if (static_cast<uint16_t>(~nn) != n || br.pos + n > len || out.size() + n > maxOut)
                return R::fail(ErrorCode::ParseError, "deflate: bad stored block");
            out.append(reinterpret_cast<const char*>(data + br.pos), n);
            br.pos += n;
        } else if (type == 1) {
            if (!inflateBlock(br, out, fixedLit, fixedDist, maxOut)) return R::fail(ErrorCode::ParseError, "deflate: bad data");
        } else if (type == 2) {
            int hlit = static_cast<int>(br.bits(5)) + 257;
            int hdist = static_cast<int>(br.bits(5)) + 1;
            int hclen = static_cast<int>(br.bits(4)) + 4;
            static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            uint8_t cl[19] = {0};
            for (int i = 0; i < hclen; ++i) cl[order[i]] = static_cast<uint8_t>(br.bits(3));
            Huffman clh;
            if (!clh.build(cl, 19)) return R::fail(ErrorCode::ParseError, "deflate: bad code lengths");
            uint8_t lens[320] = {0};
            int idx = 0;
            while (idx < hlit + hdist) {
                int sym = clh.decode(br);
                if (sym < 0 || br.err) return R::fail(ErrorCode::ParseError, "deflate: bad lengths");
                if (sym < 16) {
                    lens[idx++] = static_cast<uint8_t>(sym);
                } else {
                    int rep = 0;
                    uint8_t val = 0;
                    if (sym == 16) {
                        if (idx == 0) return R::fail(ErrorCode::ParseError, "deflate: repeat without value");
                        val = lens[idx - 1];
                        rep = 3 + static_cast<int>(br.bits(2));
                    } else if (sym == 17) {
                        rep = 3 + static_cast<int>(br.bits(3));
                    } else {
                        rep = 11 + static_cast<int>(br.bits(7));
                    }
                    if (idx + rep > hlit + hdist) return R::fail(ErrorCode::ParseError, "deflate: too many lengths");
                    while (rep--) lens[idx++] = val;
                }
            }
            Huffman lit, dist;
            if (!lit.build(lens, hlit) || !dist.build(lens + hlit, hdist))
                return R::fail(ErrorCode::ParseError, "deflate: bad tables");
            if (!inflateBlock(br, out, lit, dist, maxOut)) return R::fail(ErrorCode::ParseError, "deflate: bad data");
        } else {
            return R::fail(ErrorCode::ParseError, "deflate: invalid block type");
        }
    }
    if (consumed) *consumed = br.pos;
    return R::ok(std::move(out));
}

Result<std::string> zlibInflate(const std::string& in, size_t maxOut) {
    using R = Result<std::string>;
    if (in.size() < 6) return R::fail(ErrorCode::ParseError, "zlib: too short");
    const uint8_t* d = reinterpret_cast<const uint8_t*>(in.data());
    if ((d[0] & 0x0F) != 8 || ((d[0] << 8) | d[1]) % 31 != 0 || (d[1] & 0x20))
        return R::fail(ErrorCode::ParseError, "zlib: bad header");
    return rawInflate(d + 2, in.size() - 2, maxOut);
}

namespace {
uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | p[3];
}
int paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    return pb <= pc ? b : c;
}
}  // namespace

Result<Image> decodePng(const std::string& file, int maxDim) {
    using R = Result<Image>;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    if (file.size() < 33 || std::memcmp(file.data(), sig, 8) != 0) return R::fail(ErrorCode::ParseError, "png: bad signature");
    const uint8_t* d = reinterpret_cast<const uint8_t*>(file.data());
    size_t pos = 8;
    int w = 0, h = 0, depth = 0, ctype = 0, interlace = 0;
    std::string idat;
    std::vector<uint32_t> palette;
    std::vector<uint8_t> trns;
    bool sawHeader = false;
    while (pos + 12 <= file.size()) {
        uint32_t len = be32(d + pos);
        if (len > file.size() - pos - 12) return R::fail(ErrorCode::ParseError, "png: truncated chunk");
        const char* type = reinterpret_cast<const char*>(d + pos + 4);
        const uint8_t* body = d + pos + 8;
        if (std::memcmp(type, "IHDR", 4) == 0 && len >= 13) {
            w = static_cast<int>(be32(body));
            h = static_cast<int>(be32(body + 4));
            depth = body[8];
            ctype = body[9];
            interlace = body[12];
            sawHeader = true;
        } else if (std::memcmp(type, "PLTE", 4) == 0) {
            for (uint32_t i = 0; i + 2 < len; i += 3)
                palette.push_back(0xFF000000u | (body[i] << 16) | (body[i + 1] << 8) | body[i + 2]);
        } else if (std::memcmp(type, "tRNS", 4) == 0) {
            trns.assign(body, body + len);
        } else if (std::memcmp(type, "IDAT", 4) == 0) {
            idat.append(reinterpret_cast<const char*>(body), len);
        } else if (std::memcmp(type, "IEND", 4) == 0) {
            break;
        }
        pos += 12 + len;
    }
    if (!sawHeader || w <= 0 || h <= 0 || w > maxDim || h > maxDim) return R::fail(ErrorCode::ParseError, "png: bad size");
    if (interlace) return R::fail(ErrorCode::Unsupported, "png: interlaced images not supported");
    int channels = 0;
    switch (ctype) {
        case 0: channels = 1; break;
        case 2: channels = 3; break;
        case 3: channels = 1; break;
        case 4: channels = 2; break;
        case 6: channels = 4; break;
        default: return R::fail(ErrorCode::ParseError, "png: bad colour type");
    }
    if (!(depth == 8 || depth == 16 || (ctype == 3 && (depth == 1 || depth == 2 || depth == 4)) ||
          (ctype == 0 && (depth == 1 || depth == 2 || depth == 4))))
        return R::fail(ErrorCode::Unsupported, "png: unsupported bit depth");
    const size_t bitsPerPixel = static_cast<size_t>(channels) * depth;
    const size_t stride = (static_cast<size_t>(w) * bitsPerPixel + 7) / 8;
    const size_t bpp = std::max<size_t>(1, bitsPerPixel / 8);
    auto raw = zlibInflate(idat, (stride + 1) * static_cast<size_t>(h) + 16);
    if (!raw) return R::fail(raw.error().code, "png: " + raw.error().message);
    std::string& s = raw.value();
    if (s.size() < (stride + 1) * static_cast<size_t>(h)) return R::fail(ErrorCode::ParseError, "png: image data too short");

    std::vector<uint8_t> prev(stride, 0), cur(stride);
    Image img;
    img.w = w;
    img.h = h;
    img.px.resize(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y) {
        const uint8_t* line = reinterpret_cast<const uint8_t*>(s.data()) + y * (stride + 1);
        const int filter = line[0];
        for (size_t i = 0; i < stride; ++i) {
            int x = line[1 + i];
            int a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0;
            switch (filter) {
                case 0: break;
                case 1: x += a; break;
                case 2: x += b; break;
                case 3: x += (a + b) / 2; break;
                case 4: x += paeth(a, b, c); break;
                default: return R::fail(ErrorCode::ParseError, "png: bad filter");
            }
            cur[i] = static_cast<uint8_t>(x);
        }
        uint32_t* out = &img.px[static_cast<size_t>(y) * w];
        for (int x = 0; x < w; ++x) {
            auto sample = [&](int ch) -> int {   // 8-bit value of channel ch
                if (depth == 8) return cur[static_cast<size_t>(x) * channels + ch];
                if (depth == 16) return cur[(static_cast<size_t>(x) * channels + ch) * 2];
                const size_t bit = static_cast<size_t>(x) * depth;
                int v = (cur[bit / 8] >> (8 - depth - static_cast<int>(bit % 8))) & ((1 << depth) - 1);
                return ctype == 3 ? v : v * 255 / ((1 << depth) - 1);
            };
            uint32_t r = 0, g = 0, b = 0, al = 255;
            if (ctype == 3) {
                int idx = sample(0);
                uint32_t c = idx < static_cast<int>(palette.size()) ? palette[static_cast<size_t>(idx)] : 0xFF000000u;
                r = (c >> 16) & 0xFF; g = (c >> 8) & 0xFF; b = c & 0xFF;
                if (idx < static_cast<int>(trns.size())) al = trns[static_cast<size_t>(idx)];
            } else if (ctype == 0 || ctype == 4) {
                r = g = b = static_cast<uint32_t>(sample(0));
                if (ctype == 4) al = static_cast<uint32_t>(sample(1));
            } else {
                r = static_cast<uint32_t>(sample(0));
                g = static_cast<uint32_t>(sample(1));
                b = static_cast<uint32_t>(sample(2));
                if (ctype == 6) al = static_cast<uint32_t>(sample(3));
            }
            out[x] = (al << 24) | (r << 16) | (g << 8) | b;
        }
        std::swap(prev, cur);
    }
    return R::ok(std::move(img));
}

Image scaleImage(const Image& src, int w, int h) {
    Image out;
    if (src.w <= 0 || src.h <= 0 || w <= 0 || h <= 0) return out;
    out.w = w;
    out.h = h;
    out.px.resize(static_cast<size_t>(w) * h);
    // Area average (good for strong downscaling of 512px icons).
    for (int y = 0; y < h; ++y) {
        const int y0 = y * src.h / h, y1 = std::max(y0 + 1, (y + 1) * src.h / h);
        for (int x = 0; x < w; ++x) {
            const int x0 = x * src.w / w, x1 = std::max(x0 + 1, (x + 1) * src.w / w);
            uint32_t sa = 0, sr = 0, sg = 0, sb = 0, n = 0;
            for (int yy = y0; yy < y1; ++yy)
                for (int xx = x0; xx < x1; ++xx) {
                    uint32_t p = src.px[static_cast<size_t>(yy) * src.w + xx];
                    uint32_t a = p >> 24;
                    sa += a;
                    sr += ((p >> 16) & 0xFF) * a;
                    sg += ((p >> 8) & 0xFF) * a;
                    sb += (p & 0xFF) * a;
                    ++n;
                }
            uint32_t a = sa / n;
            uint32_t r = sa ? sr / sa : 0, g = sa ? sg / sa : 0, b = sa ? sb / sa : 0;
            out.px[static_cast<size_t>(y) * w + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    return out;
}

}  // namespace henu
