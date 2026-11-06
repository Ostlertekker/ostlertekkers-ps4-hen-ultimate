// PS4 HEN ULTIMATE - backup/backup.h
// Backups of dashboard data (settings, favourites, own themes) and optionally
// the GoldHEN cheat folders. Each backup is a folder
//   <target>/<stamp>/files/<console path without leading '/'>
//   <target>/<stamp>/manifest.json   {created, includesCheats, files:[{path,size,sha256}]}
// Restore verifies every checksum first and backs up the current state.
#pragma once

#include <string>
#include <vector>

#include "core/result.h"
#include "files/path_guard.h"
#include "platform/platform.h"

namespace henu {

struct BackupInfo {
    std::string dir;        // backup folder
    std::string stamp;      // folder name
    std::string created;
    size_t files = 0;
    uint64_t bytes = 0;
    bool includesCheats = false;
    bool manifestOk = false;
};

class BackupService {
public:
    BackupService(IFileSystem& fs, const PathGuard& guard, std::string dataRoot, std::string cheatRoot);

    // target: folder that holds backups, e.g. "/data/PS4HENULTIMATE/backups"
    Result<BackupInfo> create(const std::string& target, bool includeCheats, const std::string& stamp);
    std::vector<BackupInfo> list(const std::string& target);
    // Returns empty list if OK, otherwise the files that failed.
    std::vector<std::string> verify(const std::string& backupDir);
    Status restore(const std::string& backupDir, const std::string& safetyTarget, const std::string& stamp);
    Status remove(const std::string& backupDir);

private:
    std::vector<std::string> sources(bool includeCheats) const;
    IFileSystem& fs_;
    const PathGuard& guard_;
    std::string dataRoot_, cheatRoot_;
};

// "2026-11-06_05-12-30" from the wall clock.
std::string timestampNow();

}  // namespace henu
