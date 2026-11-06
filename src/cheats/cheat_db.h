// OSTLERTEKKERS HEN - cheats/cheat_db.h
// Scans the GoldHEN cheat folders and reads cheat names from the files.
// Formats (verified against github.com/GoldHEN/GoldHEN_Cheat_Repository):
//   json: {"name","id","version","process","mods":[{"name","type",...}],"credits"}
//   shn : XML <Trainer Game= Cusa= Version= Process= Moder=> <Cheat Text="..."> ...
//   mc4 : encrypted - only the file name is evaluated
#pragma once

#include <string>
#include <vector>

#include "cheats/cheat_types.h"
#include "core/result.h"
#include "platform/platform.h"

namespace henu {

class GameLibrary;
struct GameInfo;

// Parses content of one cheat file (file name gives title/version/format).
Result<CheatFile> parseCheatFile(const std::string& fileName, const std::string& bytes);
// SHN text with UTF-16 LE/BE BOM -> UTF-8; anything else unchanged.
std::string toUtf8(const std::string& bytes);

class CheatDatabase {
public:
    // root: "/data/GoldHEN/cheats" (subfolders json, shn, mc4)
    void scan(IFileSystem& fs, const std::string& root);
    bool scanned() const { return scanned_; }
    const std::vector<CheatFile>& files() const { return files_; }
    size_t count(CheatFormat f) const;
    size_t totalCheats() const;
    const std::vector<std::string>& errors() const { return errors_; }
    // All files for a title, any version.
    std::vector<const CheatFile*> forTitle(const std::string& titleId) const;

private:
    std::vector<CheatFile> files_;
    std::vector<std::string> errors_;
    bool scanned_ = false;
};

Compatibility compatibilityFor(const CheatFile& f, const GameLibrary& games);

// Case-insensitive match on game name, title id, region code.
bool matchesQuery(const CheatFile& f, const GameInfo* installed, const std::string& query);

}  // namespace henu
