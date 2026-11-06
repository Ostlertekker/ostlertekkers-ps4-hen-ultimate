// PS4 HEN ULTIMATE - files/path_guard.cpp
#include "files/path_guard.h"

#include <utility>

namespace henu {

namespace {
const char* const kCheatDirs[] = {
    "/data/GoldHEN/cheats/json",
    "/data/GoldHEN/cheats/shn",
    "/data/GoldHEN/cheats/mc4",
};
}  // namespace

PathGuard::PathGuard(std::string appDataRoot) {
    auto norm = normalize(appDataRoot);
    // An invalid or dangerous app root disables all writes (fail closed).
    if (norm.isOk() && norm.value() != "/" && norm.value() != "/data") {
        writable_.push_back(norm.value());
    }
    for (int i = 0; i < 8; ++i) writable_.push_back("/mnt/usb" + std::to_string(i));
    for (const char* d : kCheatDirs) writable_.push_back(d);

    // Deny list is checked before the allow list.
    protected_ = {
        "/system", "/system_ex", "/system_data", "/system_tmp",
        "/preinst", "/preinst2", "/update", "/dev", "/proc",
        "/user/app", "/user/patch", "/user/addcont", "/user/home",
        "/mnt/sandbox", "/mnt/disc",
    };
}

Result<std::string> PathGuard::normalize(const std::string& path) {
    if (path.empty()) return Result<std::string>::fail(ErrorCode::InvalidArgument, "empty path");
    if (path[0] != '/') return Result<std::string>::fail(ErrorCode::InvalidArgument, "relative path");
    if (path.find('\0') != std::string::npos)
        return Result<std::string>::fail(ErrorCode::InvalidArgument, "NUL in path");

    std::vector<std::string> parts;
    size_t i = 0;
    while (i < path.size()) {
        while (i < path.size() && path[i] == '/') ++i;
        size_t j = i;
        while (j < path.size() && path[j] != '/') ++j;
        if (j > i) {
            std::string seg = path.substr(i, j - i);
            if (seg == ".") {
                // skip
            } else if (seg == "..") {
                if (parts.empty())
                    return Result<std::string>::fail(ErrorCode::InvalidArgument, "'..' above root");
                parts.pop_back();
            } else {
                parts.push_back(std::move(seg));
            }
        }
        i = j;
    }

    // /user/data is the same storage as /data on PS4.
    if (parts.size() >= 2 && parts[0] == "user" && parts[1] == "data") {
        parts.erase(parts.begin());
    }

    std::string out;
    for (const auto& p : parts) { out += '/'; out += p; }
    if (out.empty()) out = "/";
    return Result<std::string>::ok(out);
}

bool PathGuard::isUnder(const std::string& path, const std::string& root) {
    if (path.size() < root.size()) return false;
    if (path.compare(0, root.size(), root) != 0) return false;
    return path.size() == root.size() || path[root.size()] == '/';
}

bool PathGuard::isGoldhenCheatPath(const std::string& normalizedPath) {
    for (const char* d : kCheatDirs) {
        if (isUnder(normalizedPath, d)) return true;
    }
    return false;
}

Result<std::string> PathGuard::check(const std::string& path, AccessKind kind) const {
    auto norm = normalize(path);
    if (!norm.isOk()) return norm;
    const std::string& p = norm.value();

    if (kind == AccessKind::Read) return norm;  // reading is allowed; OS enforces the rest

    for (const auto& prot : protected_) {
        if (isUnder(p, prot))
            return Result<std::string>::fail(ErrorCode::AccessDenied, "protected system path: " + p);
    }
    // Writable root folders themselves may not be deleted/replaced, only their contents.
    for (const auto& root : writable_) {
        if (p == root)
            return Result<std::string>::fail(ErrorCode::AccessDenied, "cannot modify root folder: " + p);
        if (isUnder(p, root)) return norm;
    }
    return Result<std::string>::fail(ErrorCode::AccessDenied, "path not writable: " + p);
}

}  // namespace henu
