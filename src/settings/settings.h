// OSTLERTEKKERS HEN - settings/settings.h
// Persistent dashboard settings (schema: schemas/settings.schema.json).
#pragma once

#include <string>
#include <vector>

#include "core/json.h"
#include "core/result.h"
#include "platform/platform.h"

namespace henu {

struct Settings {
    int schemaVersion = 1;
    std::string language = "de";             // de | en
    std::string themeId = "midnight";
    std::vector<std::string> favoriteThemes;
    std::string randomTheme = "off";         // off | on_start | daily
    std::string modeOverride = "theme";      // theme | dark | light
    std::string tileSize = "medium";         // small | medium | large
    std::string animations = "full";         // full | reduced | off
    bool soundEnabled = true;
    int volume = 70;
    std::string confirmButton = "cross";     // cross | circle
    float stickDeadzone = 0.25f;
    int repeatDelayMs = 350;
    int repeatRateMs = 90;
    bool allowDownloads = false;
    std::string cheatDbSource = "none";      // none | official_goldhen_repo | usb_zip
    bool updateCheckOnStart = false;
    std::string backupTarget = "internal";   // internal | usb
    int backupKeep = 5;
    std::string bootAnimation = "inferno";   // inferno | era1..era5 | random | off
    std::string bootSound = "builtin";       // builtin | custom | off
    std::string launchAnimation = "inferno"; // game start: inferno | era1..era5 | random | off
};

// Invalid/unknown values are replaced by defaults (never fails on content);
// `warnings` lists what was corrected so it can be logged.
Settings settingsFromJson(const Json& j, std::vector<std::string>* warnings = nullptr);
Json settingsToJson(const Settings& s);

class SettingsStore {
public:
    // dir: app data root, e.g. "/data/OSTLERTEKKERS_HEN"
    SettingsStore(IFileSystem& fs, std::string dir);

    // Order: settings.json -> settings.json.bak -> defaults.
    // `source` reports which one was used ("file", "backup", "defaults").
    Settings load(std::string* source = nullptr);

    // Atomic: write .tmp, read back + parse, current -> .bak, .tmp -> current.
    Status save(const Settings& s);

    Status resetToDefaults();   // keeps current as .bak

    std::string path() const { return dir_ + "/settings.json"; }

private:
    IFileSystem& fs_;
    std::string dir_;
};

}  // namespace henu
