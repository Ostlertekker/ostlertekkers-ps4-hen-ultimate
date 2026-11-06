// PS4 HEN ULTIMATE - app/favorites.h
// Favourite games, favourite cheat files and recently opened cheat files.
// Stored in <dataRoot>/favorites.json (dashboard data only).
#pragma once

#include <set>
#include <string>
#include <vector>

#include "core/result.h"
#include "platform/platform.h"

namespace henu {

class Favorites {
public:
    void load(IFileSystem& fs, const std::string& dataRoot);
    Status save();

    bool isGame(const std::string& titleId) const { return games_.count(titleId) != 0; }
    bool isCheat(const std::string& fileName) const { return cheats_.count(fileName) != 0; }
    void toggleGame(const std::string& titleId);
    void toggleCheat(const std::string& fileName);
    void addRecent(const std::string& fileName);   // most recent first, max 20
    const std::vector<std::string>& recent() const { return recent_; }

private:
    IFileSystem* fs_ = nullptr;
    std::string path_;
    std::set<std::string> games_, cheats_;
    std::vector<std::string> recent_;
};

}  // namespace henu
