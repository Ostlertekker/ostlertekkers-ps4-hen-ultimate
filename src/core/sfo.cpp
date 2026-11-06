// OSTLERTEKKERS HEN - core/sfo.cpp
#include "core/sfo.h"

#include <cstring>
#include <vector>

namespace henu {

namespace {
uint32_t rd32(const std::string& b, size_t o) {
    uint32_t v;
    std::memcpy(&v, b.data() + o, 4);
    return v;
}
uint16_t rd16(const std::string& b, size_t o) {
    uint16_t v;
    std::memcpy(&v, b.data() + o, 2);
    return v;
}
void wr32(std::string& b, uint32_t v) { b.append(reinterpret_cast<const char*>(&v), 4); }
void wr16(std::string& b, uint16_t v) { b.append(reinterpret_cast<const char*>(&v), 2); }
}  // namespace

Result<Sfo> parseSfo(const std::string& b) {
    using R = Result<Sfo>;
    if (b.size() < 20 || b.compare(0, 4, std::string("\0PSF", 4)) != 0) return R::fail(ErrorCode::ParseError, "not a param.sfo");
    const uint32_t keyStart = rd32(b, 8), dataStart = rd32(b, 12), count = rd32(b, 16);
    if (count > 1024 || keyStart > b.size() || dataStart > b.size() || 20 + static_cast<size_t>(count) * 16 > b.size())
        return R::fail(ErrorCode::ParseError, "bad sfo header");
    Sfo out;
    for (uint32_t i = 0; i < count; ++i) {
        const size_t e = 20 + static_cast<size_t>(i) * 16;
        const uint16_t keyOff = rd16(b, e), fmt = rd16(b, e + 2);
        const uint32_t len = rd32(b, e + 4), dataOff = rd32(b, e + 12);
        const size_t kp = static_cast<size_t>(keyStart) + keyOff;
        if (kp >= b.size()) return R::fail(ErrorCode::ParseError, "key outside file");
        const size_t kend = b.find('\0', kp);
        if (kend == std::string::npos) return R::fail(ErrorCode::ParseError, "unterminated key");
        const std::string key = b.substr(kp, kend - kp);
        const size_t dp = static_cast<size_t>(dataStart) + dataOff;
        if (dp > b.size() || len > b.size() - dp) return R::fail(ErrorCode::ParseError, "data outside file");
        if (fmt == 0x0404) {
            if (len < 4) return R::fail(ErrorCode::ParseError, "short int");
            out.ints[key] = static_cast<int32_t>(rd32(b, dp));
        } else if (fmt == 0x0204 || fmt == 0x0004) {
            std::string v = b.substr(dp, len);
            const size_t nul = v.find('\0');
            if (nul != std::string::npos) v.resize(nul);
            out.strings[key] = v;
        }
    }
    return R::ok(out);
}

std::string writeSfo(const Sfo& sfo) {
    struct E { std::string key; bool isInt; std::string s; int64_t i; };
    std::vector<E> es;
    for (const auto& kv : sfo.strings) es.push_back({kv.first, false, kv.second, 0});
    for (const auto& kv : sfo.ints) es.push_back({kv.first, true, "", kv.second});
    std::string keys, data, index;
    for (const auto& e : es) {
        const uint32_t len = e.isInt ? 4 : static_cast<uint32_t>(e.s.size() + 1);
        const uint32_t maxLen = e.isInt ? 4 : ((len + 3) & ~3u);
        wr16(index, static_cast<uint16_t>(keys.size()));
        wr16(index, e.isInt ? 0x0404 : 0x0204);
        wr32(index, len);
        wr32(index, maxLen);
        wr32(index, static_cast<uint32_t>(data.size()));
        keys += e.key;
        keys += '\0';
        if (e.isInt) {
            wr32(data, static_cast<uint32_t>(e.i));
        } else {
            data += e.s;
            data.append(maxLen - e.s.size(), '\0');
        }
    }
    while (keys.size() % 4) keys += '\0';
    std::string out("\0PSF", 4);
    wr32(out, 0x101);
    const uint32_t keyStart = 20 + static_cast<uint32_t>(index.size());
    wr32(out, keyStart);
    wr32(out, keyStart + static_cast<uint32_t>(keys.size()));
    wr32(out, static_cast<uint32_t>(es.size()));
    return out + index + keys + data;
}

}  // namespace henu
