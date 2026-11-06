// PS4 HEN ULTIMATE - files/path_guard.h
// Every write/delete/move in the dashboard must pass through PathGuard.
// Default is deny: only explicitly allowed roots are writable.
#pragma once

#include <string>
#include <vector>

#include "core/result.h"

namespace henu {

enum class AccessKind { Read, Write };

class PathGuard {
public:
    // appDataRoot: the dashboard's own folder, e.g. "/data/PS4HENULTIMATE".
    explicit PathGuard(std::string appDataRoot);

    // Normalizes an absolute POSIX path: collapses "//", resolves "." and
    // "..", strips trailing "/". Maps "/user/data/..." to "/data/...".
    // Fails for relative paths, empty paths, NUL bytes, or ".." above root.
    static Result<std::string> normalize(const std::string& path);

    // Checks whether an operation is allowed. Returns the normalized path.
    Result<std::string> check(const std::string& path, AccessKind kind) const;

    bool canWrite(const std::string& path) const { return check(path, AccessKind::Write).isOk(); }

    // True if path is one of the GoldHEN cheat folders (or inside one).
    // Writes there additionally require user confirmation + backup (enforced
    // by the cheats module, not here).
    static bool isGoldhenCheatPath(const std::string& normalizedPath);

    const std::vector<std::string>& writableRoots() const { return writable_; }

private:
    static bool isUnder(const std::string& path, const std::string& root);

    std::vector<std::string> writable_;
    std::vector<std::string> protected_;  // checked first; deny wins
};

}  // namespace henu
