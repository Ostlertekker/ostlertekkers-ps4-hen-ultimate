// OSTLERTEKKERS HEN - games/game_library.h
// READ-ONLY list of installed titles. Never launches, copies or modifies.
//
// Source of truth (docs/API_NACHWEIS.md G6/G7):
//   /user/app/<TITLEID>/                       one folder per installed title
//   /system_data/priv/appmeta/<TITLEID>/param.sfo   TITLE, CONTENT_ID, CATEGORY,
//                                                   APP_VER, VERSION
// Installed version = max(APP_VER, VERSION) - same rule the official GoldHEN
// Cheat Manager applies to app.db (source/cheats.c, check_game_appdb).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "cheats/cheat_types.h"
#include "core/result.h"
#include "platform/platform.h"

namespace henu {

struct GameInfo {
    std::string titleId;
    std::string name;
    std::string contentId;
    std::string category;      // gd = application, gp = patch ...
    std::string version;       // "01.05" or "" if unknown
    Region region = Region::Unknown;
    bool homebrew = false;     // content id "IV..." (fake-signed / homebrew)
    bool metaFound = false;    // param.sfo readable
    std::string iconPath;      // appmeta/icon0.png if present
    std::string appPath;       // e.g. /user/app/CUSA00001 or /mnt/ext0/user/app/...
    bool external = false;     // installed on the extended storage
    // Decoded + downscaled icon (A8R8G8B8, kIconSize^2); empty if not loaded.
    std::shared_ptr<const std::vector<uint32_t>> icon;
};

constexpr int kIconSize = 192;

struct GamePaths {
    // Installed applications (internal, then extended storage).
    std::vector<std::string> appDirs{"/user/app", "/mnt/ext0/user/app"};
    // Metadata folders tried in order for <ID>/param.sfo and <ID>/icon0.png.
    std::vector<std::string> metaDirs{"/system_data/priv/appmeta", "/user/appmeta",
                                      "/system_data/priv/appmeta/external"};
    bool loadIcons = true;
};

class GameLibrary {
public:
    // Fails only if no application folder could be listed at all.
    Status scan(IFileSystem& fs, const GamePaths& paths = GamePaths());
    // Human-readable notes from the last scan (unreadable folders ...).
    const std::vector<std::string>& notes() const { return notes_; }
    bool scanned() const { return scanned_; }
    const std::vector<GameInfo>& all() const { return games_; }
    const GameInfo* find(const std::string& titleId) const;
    size_t gameCount() const;
    size_t homebrewCount() const;

private:
    std::vector<GameInfo> games_;
    std::vector<std::string> notes_;
    bool scanned_ = false;
};

// Higher of two "DD.DD" versions; invalid ones are ignored.
std::string higherVersion(const std::string& a, const std::string& b);

}  // namespace henu
