// PS4 HEN ULTIMATE - goldhen/goldhen_info.h
// READ-ONLY view of GoldHEN's data folder. This module never writes,
// patches, updates or replaces anything belonging to GoldHEN.
// Known layout (GoldHEN docs / GoldHEN_Cheat_Manager include/cheats.h):
//   /data/GoldHEN/cheats/{json,shn,mc4}   cheat files
//   /data/GoldHEN/patches/                game patches
//   /data/GoldHEN/plugins/  + /data/GoldHEN/plugins.ini
//   /data/GoldHEN/config.ini
#pragma once

#include <string>
#include <vector>

#include "platform/platform.h"

namespace henu {

enum class FirmwareSupport {
    SupportedBeta,   // listed in developer changelog (e.g. 13.52 since 2.4b18.11)
    NotListed,       // firmware not in our known-support table
    Unknown,         // firmware or GoldHEN version could not be determined
};

struct GoldhenPlugin {
    std::string section;   // "default" or a title id
    std::string path;
    bool fileExists = false;
};

struct GoldhenStatus {
    bool folderFound = false;
    bool configFound = false;
    bool pluginsIniFound = false;
    size_t cheatsJson = 0, cheatsShn = 0, cheatsMc4 = 0;
    size_t patchFiles = 0;
    std::vector<GoldhenPlugin> plugins;
};

GoldhenStatus detectGoldhen(IFileSystem& fs, const std::string& root = "/data/GoldHEN");

// plugins.ini: "[default]" / "[CUSA00001]" sections, one .prx path per line, ';' comments.
std::vector<GoldhenPlugin> parsePluginsIni(const std::string& text);

// Known-support table (docs/KOMPATIBILITAET.md). Pure function, testable.
FirmwareSupport lookupSupport(const std::string& goldhenVersion, const std::string& firmware);

// Lowest GoldHEN build documented to support this firmware, "" if none known.
std::string minimumGoldhenFor(const std::string& firmware);

}  // namespace henu
