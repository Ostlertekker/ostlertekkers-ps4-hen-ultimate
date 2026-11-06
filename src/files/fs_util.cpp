// OSTLERTEKKERS HEN - files/fs_util.cpp
#include "files/fs_util.h"

#include <cstdio>

namespace henu {

std::string joinPath(const std::string& dir, const std::string& name) {
    if (dir.empty() || dir == "/") return "/" + name;
    return dir.back() == '/' ? dir + name : dir + "/" + name;
}

std::string parentPath(const std::string& path) {
    const size_t p = path.find_last_of('/');
    if (p == std::string::npos || p == 0) return "/";
    return path.substr(0, p);
}

std::string baseName(const std::string& path) {
    const size_t p = path.find_last_of('/');
    return p == std::string::npos ? path : path.substr(p + 1);
}

std::string formatBytes(uint64_t b) {
    char buf[32];
    if (b < 1024) std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(b));
    else if (b < 1024ull * 1024) std::snprintf(buf, sizeof(buf), "%.1f KB", b / 1024.0);
    else if (b < 1024ull * 1024 * 1024) std::snprintf(buf, sizeof(buf), "%.1f MB", b / (1024.0 * 1024));
    else std::snprintf(buf, sizeof(buf), "%.2f GB", b / (1024.0 * 1024 * 1024));
    return buf;
}

namespace {
void walk(IFileSystem& fs, const std::string& dir, int depth, int maxDepth, std::vector<FileRef>& out) {
    if (depth > maxDepth || out.size() > 50000) return;
    auto entries = fs.list(dir);
    if (!entries) return;
    for (const auto& e : entries.value()) {
        const std::string p = joinPath(dir, e.name);
        if (e.isDir) walk(fs, p, depth + 1, maxDepth, out);
        else out.push_back({p, e.size});
    }
}
}  // namespace

std::vector<FileRef> listFilesRecursive(IFileSystem& fs, const std::string& dir, int maxDepth) {
    std::vector<FileRef> out;
    if (fs.exists(dir)) walk(fs, dir, 0, maxDepth, out);
    return out;
}

Status makeDirs(IFileSystem& fs, const PathGuard& guard, const std::string& dir) {
    auto norm = PathGuard::normalize(dir);
    if (!norm) return Status::fail(norm.error().code, norm.error().message);
    std::string cur;
    const std::string& p = norm.value();
    size_t i = 1;
    while (i <= p.size()) {
        size_t j = p.find('/', i);
        if (j == std::string::npos) j = p.size();
        cur = p.substr(0, j);
        if (!fs.exists(cur)) {
            // Creating a missing writable root itself (e.g. the cheat folder) is allowed;
            // modifying/deleting it is not (PathGuard::check).
            bool isRoot = false;
            for (const auto& r : guard.writableRoots()) if (r == cur) isRoot = true;
            auto chk = guard.check(cur, AccessKind::Write);
            if (!chk && !isRoot) return Status::fail(chk.error().code, chk.error().message);
            Status st = fs.makeDir(cur);
            if (!st) return st;
        }
        i = j + 1;
    }
    return Status::ok();
}

Status guardedCopy(IFileSystem& fs, const PathGuard& guard, const std::string& from, const std::string& to) {
    auto chk = guard.check(to, AccessKind::Write);
    if (!chk) return Status::fail(chk.error().code, chk.error().message);
    Status st = makeDirs(fs, guard, parentPath(chk.value()));
    if (!st) return st;
    return fs.copyFile(from, chk.value(), 4ull << 30);
}

Status guardedRemoveTree(IFileSystem& fs, const PathGuard& guard, const std::string& path) {
    auto chk = guard.check(path, AccessKind::Write);
    if (!chk) return Status::fail(chk.error().code, chk.error().message);
    auto entries = fs.list(chk.value());
    if (entries) {   // directory
        for (const auto& e : entries.value()) {
            Status st = guardedRemoveTree(fs, guard, joinPath(chk.value(), e.name));
            if (!st) return st;
        }
    }
    return fs.remove(chk.value());
}

Status makeDirsUnchecked(IFileSystem& fs, const std::string& dir) {
    if (dir.empty()) return Status::ok();
    size_t i = dir[0] == '/' ? 1 : 0;
    while (i <= dir.size()) {
        size_t j = dir.find('/', i);
        if (j == std::string::npos) j = dir.size();
        const std::string cur = dir.substr(0, j);
        if (!cur.empty() && !fs.exists(cur)) {
            Status st = fs.makeDir(cur);
            if (!st) return st;
        }
        i = j + 1;
    }
    return Status::ok();
}

std::string firstUsbRoot(IFileSystem& fs) {
    for (int i = 0; i < 8; ++i) {
        const std::string p = "/mnt/usb" + std::to_string(i);
        if (fs.exists(p) && fs.list(p)) return p;
    }
    return "";
}

}  // namespace henu
