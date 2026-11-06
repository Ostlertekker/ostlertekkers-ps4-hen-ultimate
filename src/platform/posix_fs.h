// OSTLERTEKKERS HEN - platform/posix_fs.h
// IFileSystem over POSIX calls (open/read/write/rename/mkdir/opendir/statvfs).
// Used on host and on PS4 (OpenOrbis ships a musl libc). Does NOT check
// permissions - callers must go through PathGuard for writes.
#pragma once

#include "platform/platform.h"

namespace henu {

class PosixFileSystem : public IFileSystem {
public:
    bool exists(const std::string& path) override;
    Result<std::vector<DirEntry>> list(const std::string& dir) override;
    Result<std::string> readFile(const std::string& path, size_t maxBytes) override;
    Status writeFile(const std::string& path, const std::string& data) override;
    Status appendFile(const std::string& path, const std::string& data) override;
    Status makeDir(const std::string& path) override;
    Status rename(const std::string& from, const std::string& to) override;
    Status remove(const std::string& path) override;
    Result<SpaceInfo> space(const std::string& mountPath) override;
    Status copyFile(const std::string& from, const std::string& to, uint64_t maxBytes) override;
};

}  // namespace henu
