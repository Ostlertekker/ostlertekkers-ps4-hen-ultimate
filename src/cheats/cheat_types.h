// OSTLERTEKKERS HEN - cheats/cheat_types.h
// Data model for the Cheat Center. Matches the GoldHEN cheat file layout:
//   /data/GoldHEN/cheats/{json,shn,mc4}/{TITLEID}_{VERSION}[_{process}].{format}
// (documented in github.com/GoldHEN/GoldHEN_Cheat_Repository README)
#pragma once

#include <string>
#include <vector>

#include "core/version.h"

namespace henu {

enum class CheatFormat { Json, Shn, Mc4 };
const char* toString(CheatFormat f);  // "json" | "shn" | "mc4"

enum class Region { Unknown, US, EU, JP, Asia };
const char* toString(Region r);       // "unknown" | "US" | "EU" | "JP" | "ASIA"

// Categories are derived from cheat names (the file formats have none).
enum class CheatCategory {
    Health, Money, Ammo, Stamina, Experience, Unlocks, Resources, Stats, Gameplay, Other,
};
const char* toString(CheatCategory c);

struct CheatFileId {
    std::string titleId;      // e.g. "CUSA01234"
    AppVersion version;       // e.g. 01.01
    std::string variant;      // text after "_" following the version, e.g. "2" or "default_mp.elf"
    CheatFormat format = CheatFormat::Json;
};

struct CheatEntry {
    std::string name;
    std::string description;
    std::string type;          // json: "checkbox" | "button"
    CheatCategory category = CheatCategory::Other;
};

struct CheatFile {
    CheatFileId id;
    std::string fileName;
    std::string path;          // absolute console path
    std::string gameName;      // from file content
    std::string process;       // from file content (json "process", shn Process)
    std::string contentTitleId, contentVersion;   // as written inside the file
    std::vector<std::string> credits;
    std::vector<CheatEntry> cheats;
    bool readable = false;     // false for encrypted MC4 or broken files
    std::string note;          // why not readable / warnings
};

enum class Compatibility {
    Compatible,        // title id AND version match exactly
    VersionMismatch,   // same title id, different version -> warn
    WrongTitle,        // different title id
    GameNotInstalled,  // cannot check
};

}  // namespace henu
