// PS4 HEN ULTIMATE - games/game_library.h
// READ-ONLY list of installed titles. Never launches, copies or modifies.
//
// Source of truth (docs/API_NACHWEIS.md G6/G7):
//   /user/app/<TITLEID>/                       one folder per installed title
//   /system_data/priv/appmeta/<TITLEID>/param.sfo   TITLE, CONTENT_ID, CATEGORY,
//                                                   APP_VER, VERSION
// Installed version = max(APP_VER, VERSION) - same rule the official GoldHEN
// Cheat Manager applies to app.db (source/cheats.c, check_game_appdb).
#pragma once

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
};

struct GamePaths {
    std::string appDir = "/user/app";
    std::string metaDir = "/system_data/priv/appmeta";
};

class GameLibrary {
public:
    Status scan(IFileSystem& fs, const GamePaths& paths = GamePaths());
    bool scanned() const { return scanned_; }
    const std::vector<GameInfo>& all() const { return games_; }
    const GameInfo* find(const std::string& titleId) const;
    size_t gameCount() const;
    size_t homebrewCount() const;

private:
    std::vector<GameInfo> games_;
    bool scanned_ = false;
};

// Higher of two "DD.DD" versions; invalid ones are ignored.
std::string higherVersion(const std::string& a, const std::string& b);

}  // namespace henu
