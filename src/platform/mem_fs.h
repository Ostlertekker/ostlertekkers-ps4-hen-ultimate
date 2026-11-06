// PS4 HEN ULTIMATE - platform/mem_fs.h
// In-memory IFileSystem for unit tests. Can simulate write failures.
#pragma once

#include <map>
#include <set>
#include <string>

#include "platform/platform.h"

namespace henu {

class MemFileSystem : public IFileSystem {
public:
    std::map<std::string, std::string> files;
    std::set<std::string> dirs{"/"};
    bool failWrites = false;
    bool corruptWrites = false;   // write succeeds but stores truncated data

    bool exists(const std::string& p) override { return files.count(p) || dirs.count(p); }

    Result<std::vector<DirEntry>> list(const std::string& dir) override {
        if (!dirs.count(dir)) return Result<std::vector<DirEntry>>::fail(ErrorCode::NotFound, dir);
        std::vector<DirEntry> out;
        const std::string pre = dir == "/" ? "/" : dir + "/";
        for (const auto& f : files)
            if (f.first.compare(0, pre.size(), pre) == 0 && f.first.find('/', pre.size()) == std::string::npos)
                out.push_back({f.first.substr(pre.size()), false, f.second.size(), 0});
        for (const auto& d : dirs)
            if (d != dir && d.compare(0, pre.size(), pre) == 0 && d.find('/', pre.size()) == std::string::npos)
                out.push_back({d.substr(pre.size()), true, 0, 0});
        return Result<std::vector<DirEntry>>::ok(out);
    }

    Result<std::string> readFile(const std::string& p, size_t maxBytes) override {
        auto it = files.find(p);
        if (it == files.end()) return Result<std::string>::fail(ErrorCode::NotFound, p);
        if (it->second.size() > maxBytes) return Result<std::string>::fail(ErrorCode::IoError, "too large");
        return Result<std::string>::ok(it->second);
    }

    Status writeFile(const std::string& p, const std::string& data) override {
        if (failWrites) return Status::fail(ErrorCode::IoError, "simulated write failure");
        files[p] = corruptWrites ? data.substr(0, data.size() / 2) : data;
        return Status::ok();
    }

    Status makeDir(const std::string& p) override { dirs.insert(p); return Status::ok(); }

    Status rename(const std::string& from, const std::string& to) override {
        auto it = files.find(from);
        if (it == files.end()) return Status::fail(ErrorCode::NotFound, from);
        files[to] = it->second;
        files.erase(from);
        return Status::ok();
    }

    Status remove(const std::string& p) override {
        if (files.erase(p) || dirs.erase(p)) return Status::ok();
        return Status::fail(ErrorCode::NotFound, p);
    }

    Result<SpaceInfo> space(const std::string&) override {
        return Result<SpaceInfo>::fail(ErrorCode::Unsupported, "memfs");
    }
};

}  // namespace henu
