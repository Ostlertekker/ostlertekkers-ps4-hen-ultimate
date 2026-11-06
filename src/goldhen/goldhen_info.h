// PS4 HEN ULTIMATE - goldhen/goldhen_info.h
// READ-ONLY view of the externally running GoldHEN. This module never writes,
// patches, updates or replaces anything belonging to GoldHEN.
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
    std::string path;      // as listed in plugins.ini
    std::string section;   // "[default]" or title id
    bool fileExists = false;
};

struct GoldhenInfo {
    Maybe<bool> present;                // GoldHEN data folder found
    Maybe<std::string> version;         // e.g. "2.4b18.12" - only if reliably readable
    Maybe<std::string> firmware;        // from ISystemInfo, never derived from GoldHEN
    FirmwareSupport support = FirmwareSupport::Unknown;
    Maybe<int> cheatFileCount;          // files in cheats/{json,shn,mc4}
    Maybe<std::vector<GoldhenPlugin>> plugins;
    std::vector<std::string> notes;     // warnings shown to the user
};

class GoldhenDetector {
public:
    GoldhenDetector(IFileSystem& fs, ISystemInfo& sys) : fs_(fs), sys_(sys) {}
    // Phase F: detection sources are documented in docs/API_NACHWEIS.md
    // before use. Until then, unknown values stay unknown.
    GoldhenInfo detect();

private:
    IFileSystem& fs_;
    ISystemInfo& sys_;
};

// Known-support table (docs/KOMPATIBILITAET.md). Pure function, testable.
FirmwareSupport lookupSupport(const std::string& goldhenVersion, const std::string& firmware);

}  // namespace henu
