// PS4 HEN ULTIMATE - app/services.h
// Interfaces of the data/logic modules. Implementations follow in Phases C-F.
// Each service is constructed with the Platform and reports its capabilities
// into the CapabilityRegistry during init().
#pragma once

#include <string>
#include <vector>

#include "cheats/cheat_types.h"
#include "core/capability.h"
#include "core/result.h"
#include "platform/platform.h"
#include "settings/settings.h"
#include "themes/theme.h"

namespace henu {

// ------------------------------------------------------------ Themes (§5)
class IThemeManager {
public:
    virtual ~IThemeManager() = default;
    virtual Status scan() = 0;                         // bundled + user themes
    virtual const std::vector<Theme>& themes() const = 0;
    virtual Result<Theme> get(const std::string& id) const = 0;
    virtual Status importFrom(const std::string& sourcePath) = 0;   // validated copy
    virtual Status exportTo(const std::string& id, const std::string& destDir) = 0;
    virtual std::string pickRandom(const std::string& excludeId) const = 0;
};

// ------------------------------------------------------------ Games (§7)
struct GameInfo {
    std::string titleId;
    std::string name;
    std::string contentId;
    Maybe<AppVersion> version;   // base + patch, if readable
    std::string coverPath;       // empty if none
    bool isHomebrew = false;
};

class IGameLibrary {
public:
    virtual ~IGameLibrary() = default;
    virtual Status scan() = 0;   // READ-ONLY
    virtual const std::vector<GameInfo>& games() const = 0;
};

// ------------------------------------------------------------ Cheats (§4)
class ICheatDatabase {
public:
    virtual ~ICheatDatabase() = default;
    virtual Status rebuildIndex() = 0;   // scan GoldHEN cheat folders
    virtual std::vector<CheatFile> search(const std::string& query) const = 0;  // name / title id
    virtual std::vector<CheatFile> forTitle(const std::string& titleId) const = 0;
    virtual Status importFile(const std::string& sourcePath) = 0;   // staging -> validate -> confirm
    virtual Status exportFile(const std::string& cheatPath, const std::string& destDir) = 0;
    virtual Status importOfflinePack(const std::string& zipPath) = 0;
    virtual size_t fileCount() const = 0;   // shown in UI: real data size, no promises
};

// ------------------------------------------------------------ Backup (§8)
struct BackupInfo {
    std::string path;
    std::string createdAt;   // ISO 8601
    uint64_t bytes = 0;
    bool includesCheats = false;
};

class IBackupService {
public:
    virtual ~IBackupService() = default;
    virtual Result<BackupInfo> create(bool includeCheats) = 0;   // with SHA-256 manifest
    virtual Result<std::vector<BackupInfo>> list() = 0;
    virtual Status verify(const std::string& backupPath) = 0;
    virtual Status restore(const std::string& backupPath) = 0;  // verify first, backup current
};

// ------------------------------------------------------------ Update (§11)
struct UpdateManifest {
    std::string version;       // semver
    std::string changelogDe, changelogEn;
    std::string pkgUrl;        // https only
    std::string sha256;        // hex
    uint64_t sizeBytes = 0;
};

class IUpdateService {
public:
    virtual ~IUpdateService() = default;
    virtual Result<UpdateManifest> check() = 0;
    virtual Status download(const UpdateManifest& m, const std::string& destPath) = 0;  // verifies SHA-256
};

}  // namespace henu
