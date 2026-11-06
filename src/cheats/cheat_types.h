// PS4 HEN ULTIMATE - cheats/cheat_types.h
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
    std::string process;      // empty = eboot.bin
    CheatFormat format = CheatFormat::Json;
};

struct CheatEntry {
    std::string name;
    std::string description;
    CheatCategory category = CheatCategory::Other;
};

struct CheatFile {
    CheatFileId id;
    std::string path;          // absolute path on console
    std::string gameName;      // from file, if present
    std::vector<std::string> credits;
    std::vector<CheatEntry> cheats;
};

enum class Compatibility {
    Compatible,        // title id AND version match exactly
    VersionMismatch,   // same title id, different version -> warn
    WrongTitle,        // different title id
    GameNotInstalled,  // cannot check
};

}  // namespace henu
