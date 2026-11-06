// PS4 HEN ULTIMATE - games/game_library.cpp
#include "games/game_library.h"

#include <algorithm>
#include <cctype>

#include "cheats/cheat_naming.h"
#include "core/sfo.h"
#include "core/version.h"

namespace henu {

std::string higherVersion(const std::string& a, const std::string& b) {
    AppVersion va, vb;
    const bool oa = AppVersion::parse(a, va), ob = AppVersion::parse(b, vb);
    if (oa && ob) return (va < vb) ? b : a;
    if (oa) return a;
    if (ob) return b;
    return "";
}

Status GameLibrary::scan(IFileSystem& fs, const GamePaths& paths) {
    games_.clear();
    scanned_ = true;
    auto entries = fs.list(paths.appDir);
    if (!entries) return Status::fail(entries.error().code, entries.error().message);
    for (const auto& e : entries.value()) {
        if (!e.isDir || !isValidTitleId(e.name)) continue;
        GameInfo g;
        g.titleId = e.name;
        const std::string meta = paths.metaDir + "/" + e.name;
        auto bytes = fs.readFile(meta + "/param.sfo", 256 * 1024);
        if (bytes) {
            auto sfo = parseSfo(bytes.value());
            if (sfo) {
                g.metaFound = true;
                g.name = sfo.value().str("TITLE");
                g.contentId = sfo.value().str("CONTENT_ID");
                g.category = sfo.value().str("CATEGORY");
                g.version = higherVersion(sfo.value().str("APP_VER"), sfo.value().str("VERSION"));
            }
        }
        if (g.name.empty()) g.name = g.titleId;
        g.region = regionFromContentId(g.contentId);
        g.homebrew = g.contentId.compare(0, 2, "IV") == 0;
        if (fs.exists(meta + "/icon0.png")) g.iconPath = meta + "/icon0.png";
        games_.push_back(g);
    }
    std::sort(games_.begin(), games_.end(), [](const GameInfo& a, const GameInfo& b) {
        auto lower = [](std::string s) {
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        return lower(a.name) < lower(b.name);
    });
    return Status::ok();
}

const GameInfo* GameLibrary::find(const std::string& titleId) const {
    for (const auto& g : games_)
        if (g.titleId == titleId) return &g;
    return nullptr;
}

size_t GameLibrary::gameCount() const {
    return static_cast<size_t>(std::count_if(games_.begin(), games_.end(), [](const GameInfo& g) { return !g.homebrew; }));
}

size_t GameLibrary::homebrewCount() const { return games_.size() - gameCount(); }

}  // namespace henu
