// OSTLERTEKKERS HEN - games/game_library.cpp
#include "games/game_library.h"

#include <algorithm>
#include <cctype>

#include "cheats/cheat_naming.h"
#include "core/sfo.h"
#include "core/version.h"
#include "gfx/png_decode.h"

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
    notes_.clear();
    scanned_ = true;
    bool anyListed = false;
    Error firstError;
    // App folders first; then the metadata folders, which list every
    // installed title too (used when /user/app is not readable or empty).
    std::vector<std::string> sources = paths.appDirs;
    sources.insert(sources.end(), paths.metaDirs.begin(), paths.metaDirs.end());
    for (size_t si = 0; si < sources.size(); ++si) {
        const std::string& appDir = sources[si];
        const bool fromMeta = si >= paths.appDirs.size();
        auto entries = fs.list(appDir);
        if (!entries) {
            if (si == 0 || (fromMeta && si == paths.appDirs.size())) {
                if (si == 0) firstError = entries.error();
                notes_.push_back(appDir + ": " + entries.error().message);
            }
            continue;   // e.g. no extended storage connected
        }
        anyListed = true;
        const bool external = appDir.compare(0, 9, "/mnt/ext0") == 0;
        for (const auto& e : entries.value()) {
            if (!e.isDir || !isValidTitleId(e.name) || find(e.name)) continue;
            if (fromMeta && (e.name.compare(0, 4, "NPXS") == 0 || !fs.exists(appDir + "/" + e.name + "/param.sfo"))) continue;
            GameInfo g;
            g.titleId = e.name;
            g.appPath = fromMeta ? paths.appDirs.front() + "/" + e.name : appDir + "/" + e.name;
            g.external = external;
            for (const auto& metaDir : paths.metaDirs) {
                const std::string meta = metaDir + "/" + e.name;
                if (!g.metaFound) {
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
                }
                if (g.iconPath.empty() && fs.exists(meta + "/icon0.png")) g.iconPath = meta + "/icon0.png";
                if (g.metaFound && !g.iconPath.empty()) break;
            }
            if (!g.metaFound) {
                // Some homebrew keeps an unpacked sce_sys inside the app folder.
                auto bytes = fs.readFile(g.appPath + "/sce_sys/param.sfo", 256 * 1024);
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
                if (g.iconPath.empty() && fs.exists(g.appPath + "/sce_sys/icon0.png")) g.iconPath = g.appPath + "/sce_sys/icon0.png";
            }
            if (g.name.empty()) g.name = g.titleId;
            g.region = regionFromContentId(g.contentId);
            g.homebrew = g.contentId.compare(0, 2, "IV") == 0;
            if (paths.loadIcons && !g.iconPath.empty()) {
                auto png = fs.readFile(g.iconPath, 4 * 1024 * 1024);
                if (png) {
                    auto img = decodePng(png.value(), 2048);
                    if (img) {
                        Image small = scaleImage(img.value(), kIconSize, kIconSize);
                        g.icon = std::make_shared<const std::vector<uint32_t>>(std::move(small.px));
                    }
                }
            }
            games_.push_back(std::move(g));
        }
    }
    std::sort(games_.begin(), games_.end(), [](const GameInfo& a, const GameInfo& b) {
        auto lower = [](std::string s) {
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };
        return lower(a.name) < lower(b.name);
    });
    if (!anyListed) return Status::fail(firstError.code, firstError.message);
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
