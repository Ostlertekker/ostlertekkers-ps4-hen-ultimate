// PS4 HEN ULTIMATE - cheats/cheat_db.cpp
#include "cheats/cheat_db.h"

#include <algorithm>
#include <cctype>

#include "cheats/cheat_naming.h"
#include "core/json.h"
#include "games/game_library.h"

namespace henu {

namespace {
std::string xmlUnescape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') { o += s[i]; continue; }
        struct E { const char* n; char c; } es[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
        bool done = false;
        for (const auto& e : es) {
            size_t n = std::char_traits<char>::length(e.n);
            if (s.compare(i, n, e.n) == 0) { o += e.c; i += n - 1; done = true; break; }
        }
        if (!done) o += s[i];
    }
    return o;
}

// Value of attr="..." inside tag text.
std::string attr(const std::string& tag, const std::string& name) {
    const std::string key = name + "=\"";
    size_t p = 0;
    while ((p = tag.find(key, p)) != std::string::npos) {
        if (p == 0 || tag[p - 1] == ' ' || tag[p - 1] == '\t' || tag[p - 1] == '\n') {
            size_t s = p + key.size(), e = tag.find('"', s);
            if (e == std::string::npos) return "";
            return xmlUnescape(tag.substr(s, e - s));
        }
        p += key.size();
    }
    return "";
}

// SHN files exist as UTF-8 and as UTF-16 LE/BE with BOM; normalize to UTF-8.
std::string toUtf8(const std::string& b) {
    if (b.size() < 2) return b;
    const unsigned char b0 = static_cast<unsigned char>(b[0]), b1 = static_cast<unsigned char>(b[1]);
    const bool le = b0 == 0xFF && b1 == 0xFE, be = b0 == 0xFE && b1 == 0xFF;
    if (!le && !be) return b;
    std::string out;
    for (size_t i = 2; i + 1 < b.size(); i += 2) {
        uint32_t u = le ? (static_cast<unsigned char>(b[i]) | (static_cast<unsigned char>(b[i + 1]) << 8))
                        : ((static_cast<unsigned char>(b[i]) << 8) | static_cast<unsigned char>(b[i + 1]));
        if (u >= 0xD800 && u <= 0xDBFF && i + 3 < b.size()) {
            uint32_t lo = le ? (static_cast<unsigned char>(b[i + 2]) | (static_cast<unsigned char>(b[i + 3]) << 8))
                             : ((static_cast<unsigned char>(b[i + 2]) << 8) | static_cast<unsigned char>(b[i + 3]));
            if (lo >= 0xDC00 && lo <= 0xDFFF) { u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); i += 2; }
        }
        if (u < 0x80) out += static_cast<char>(u);
        else if (u < 0x800) { out += static_cast<char>(0xC0 | (u >> 6)); out += static_cast<char>(0x80 | (u & 0x3F)); }
        else if (u < 0x10000) {
            out += static_cast<char>(0xE0 | (u >> 12)); out += static_cast<char>(0x80 | ((u >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (u & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (u >> 18)); out += static_cast<char>(0x80 | ((u >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((u >> 6) & 0x3F)); out += static_cast<char>(0x80 | (u & 0x3F));
        }
    }
    return out;
}

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
}  // namespace

Result<CheatFile> parseCheatFile(const std::string& fileName, const std::string& rawBytes) {
    const std::string& bytes = rawBytes;
    using R = Result<CheatFile>;
    auto id = parseCheatFileName(fileName);
    if (!id) return R::fail(id.error().code, fileName + ": " + id.error().message);
    CheatFile f;
    f.id = id.value();
    f.fileName = fileName;

    if (f.id.format == CheatFormat::Mc4) {
        f.readable = false;
        f.note = "mc4";
        return R::ok(f);
    }
    if (f.id.format == CheatFormat::Json) {
        auto j = parseJson(bytes);
        if (!j) return R::fail(ErrorCode::ParseError, fileName + ": " + j.error().message);
        const Json& root = j.value();
        f.gameName = root.get("name").asString();
        f.contentTitleId = root.get("id").asString();
        f.contentVersion = root.get("version").asString();
        f.process = root.get("process").asString();
        for (const auto& c : root.get("credits").asArray())
            if (c.isString()) f.credits.push_back(c.asString());
        for (const auto& m : root.get("mods").asArray()) {
            CheatEntry e;
            e.name = m.get("name").asString();
            e.description = m.get("description").asString();
            e.type = m.get("type").asString();
            if (e.name.empty()) continue;
            e.category = categorize(e.name);
            f.cheats.push_back(e);
        }
        if (!root.has("mods")) return R::fail(ErrorCode::ParseError, fileName + ": no 'mods' array");
    } else {   // SHN (XML, UTF-8 or UTF-16)
        const std::string bytes = toUtf8(rawBytes);
        const size_t t = bytes.find("<Trainer");
        if (t == std::string::npos) return R::fail(ErrorCode::ParseError, fileName + ": no <Trainer>");
        const size_t te = bytes.find('>', t);
        const std::string trainer = bytes.substr(t, te == std::string::npos ? std::string::npos : te - t);
        f.gameName = attr(trainer, "Game");
        f.contentTitleId = attr(trainer, "Cusa");
        f.contentVersion = attr(trainer, "Version");
        f.process = attr(trainer, "Process");
        const std::string moder = attr(trainer, "Moder");
        if (!moder.empty()) f.credits.push_back(moder);
        size_t p = 0;
        while ((p = bytes.find("<Cheat ", p)) != std::string::npos) {
            const size_t e = bytes.find('>', p);
            if (e == std::string::npos) break;
            const std::string tag = bytes.substr(p, e - p);
            CheatEntry c;
            c.name = attr(tag, "Text");
            c.description = attr(tag, "Description");
            if (!c.name.empty()) {
                c.category = categorize(c.name);
                f.cheats.push_back(c);
            }
            p = e;
        }
    }
    f.readable = true;
    if (!f.contentTitleId.empty() && f.contentTitleId != f.id.titleId) f.note = "id-mismatch";
    else if (!f.contentVersion.empty() && f.contentVersion != f.id.version.toString()) f.note = "version-mismatch";
    return R::ok(f);
}

void CheatDatabase::scan(IFileSystem& fs, const std::string& root) {
    files_.clear();
    errors_.clear();
    scanned_ = true;
    for (const char* sub : {"json", "shn", "mc4"}) {
        const std::string dir = root + "/" + sub;
        if (!fs.exists(dir)) continue;
        auto entries = fs.list(dir);
        if (!entries) { errors_.push_back(dir + ": " + entries.error().message); continue; }
        for (const auto& e : entries.value()) {
            if (e.isDir) continue;
            auto nameCheck = parseCheatFileName(e.name);
            if (!nameCheck) {
                // Report files that look like cheats but have a name GoldHEN cannot use.
                const size_t dot = e.name.rfind('.');
                const std::string ext = dot == std::string::npos ? "" : e.name.substr(dot + 1);
                if (ext == sub) errors_.push_back(e.name + ": invalid file name (" + nameCheck.error().message + ")");
                continue;
            }
            const std::string path = dir + "/" + e.name;
            std::string bytes;
            if (std::string(sub) != "mc4") {
                auto b = fs.readFile(path, 4 * 1024 * 1024);
                if (!b) { errors_.push_back(path + ": " + b.error().message); continue; }
                bytes = b.value();
            }
            auto f = parseCheatFile(e.name, bytes);
            if (!f) { errors_.push_back(f.error().message); continue; }
            f.value().path = path;
            files_.push_back(f.value());
        }
    }
    std::sort(files_.begin(), files_.end(), [](const CheatFile& a, const CheatFile& b) {
        return a.fileName < b.fileName;
    });
}

size_t CheatDatabase::count(CheatFormat f) const {
    return static_cast<size_t>(std::count_if(files_.begin(), files_.end(), [f](const CheatFile& c) { return c.id.format == f; }));
}

size_t CheatDatabase::totalCheats() const {
    size_t n = 0;
    for (const auto& f : files_) n += f.cheats.size();
    return n;
}

std::vector<const CheatFile*> CheatDatabase::forTitle(const std::string& titleId) const {
    std::vector<const CheatFile*> out;
    for (const auto& f : files_)
        if (f.id.titleId == titleId) out.push_back(&f);
    return out;
}

Compatibility compatibilityFor(const CheatFile& f, const GameLibrary& games) {
    const GameInfo* g = games.find(f.id.titleId);
    if (!g) return Compatibility::GameNotInstalled;
    AppVersion v;
    if (!AppVersion::parse(g->version, v)) return Compatibility::VersionMismatch;   // unknown version: never "compatible"
    return checkCompatibility(f.id, g->titleId, v);
}

bool matchesQuery(const CheatFile& f, const GameInfo* installed, const std::string& query) {
    if (query.empty()) return true;
    const std::string q = lower(query);
    if (lower(f.gameName).find(q) != std::string::npos) return true;
    if (lower(f.id.titleId).find(q) != std::string::npos) return true;
    if (installed) {
        if (lower(installed->name).find(q) != std::string::npos) return true;
        if (lower(toString(installed->region)) == q) return true;
    }
    return false;
}

}  // namespace henu
