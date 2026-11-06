// OSTLERTEKKERS HEN - files/fs_util.h
// Recursive helpers on top of IFileSystem. Writing helpers check PathGuard.
#pragma once

#include <string>
#include <vector>

#include "core/result.h"
#include "files/path_guard.h"
#include "platform/platform.h"

namespace henu {

std::string joinPath(const std::string& dir, const std::string& name);
std::string parentPath(const std::string& path);   // "/a/b" -> "/a", "/a" -> "/"
std::string baseName(const std::string& path);
std::string formatBytes(uint64_t bytes);           // "1.2 MB"

struct FileRef { std::string path; uint64_t size = 0; };

// All files below dir (depth-limited). Missing dir -> empty list.
std::vector<FileRef> listFilesRecursive(IFileSystem& fs, const std::string& dir, int maxDepth = 8);

// mkdir -p, each level checked by PathGuard.
Status makeDirs(IFileSystem& fs, const PathGuard& guard, const std::string& dir);

// Copy with guard check on the destination; creates parent folders.
Status guardedCopy(IFileSystem& fs, const PathGuard& guard, const std::string& from, const std::string& to);

// Deletes a file or a whole folder tree (guard-checked).
Status guardedRemoveTree(IFileSystem& fs, const PathGuard& guard, const std::string& path);

// mkdir -p without guard (only for the app's own data folder at startup).
Status makeDirsUnchecked(IFileSystem& fs, const std::string& dir);

// First mounted USB drive ("/mnt/usb0" ... "/mnt/usb7") or "".
std::string firstUsbRoot(IFileSystem& fs);

}  // namespace henu
