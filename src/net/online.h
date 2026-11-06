// OSTLERTEKKERS HEN - net/online.h
// Online sources (only used after the user allowed internet access):
//  * GoldHEN cheat repository index, the same files the official GoldHEN
//    Cheat Manager reads (ONLINE_URL in its include/cheats.h):
//      https://goldhen.github.io/GoldHEN_Cheat_Repository/{json,shn,mc4}.txt
//      lines "CUSA12345_01.02.json=Game name", file at <base>/<fmt>/<file>
//  * GitHub Releases of this project for the online updater.
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "cheats/cheat_types.h"
#include "core/result.h"
#include "platform/platform.h"

namespace henu {

constexpr const char* kCheatRepoBase = "https://goldhen.github.io/GoldHEN_Cheat_Repository/";
constexpr const char* kUpdateRepo = "Ostlertekker/ostlertekkers-ps4-hen-ultimate";

struct OnlineCheatEntry {
    std::string file;       // CUSA12345_01.02.json
    std::string titleId;    // CUSA12345
    std::string version;    // 01.02
    std::string format;     // json | shn | mc4
    std::string name;       // game name from the index
    std::string url() const { return std::string(kCheatRepoBase) + format + "/" + file; }
};

// Invalid lines are skipped. format = "json" | "shn" | "mc4".
std::vector<OnlineCheatEntry> parseCheatIndex(const std::string& text, const std::string& format);

struct ReleaseInfo {
    int build = 0;              // from tag "build-N"
    std::string tag, title, notes, published;
    std::string pkgName, pkgUrl;
    uint64_t pkgSize = 0;
    std::string sumsUrl;        // SHA256SUMS.txt
};

// GitHub API ".../releases" array: newest release that has a .pkg asset
// and a tag build-N. Drafts are ignored.
Result<ReleaseInfo> pickNewestRelease(const std::string& githubJson);
int buildFromTag(const std::string& tag);   // "build-12" -> 12, otherwise 0
// "<hex>  <name>" lines -> hex for name (lower case), or error.
Result<std::string> sha256For(const std::string& sums, const std::string& fileName);

// A network task on a worker thread. The worker only fills memory; files are
// written by the UI thread afterwards (PathGuard, no concurrent file access).
class NetJob {
public:
    ~NetJob() { cancel(); join(); }
    void start(std::function<Status(NetJob&)> work);
    bool running() const { return running_; }
    bool finished() const { return started_ && !running_; }
    void cancel() { cancel_ = true; }
    bool cancelled() const { return cancel_; }
    void join() { if (thread_.joinable()) thread_.join(); }
    Status status() const { std::lock_guard<std::mutex> l(m_); return status_; }

    std::atomic<uint64_t> done{0}, total{0};
    // Results (guarded by lock()).
    std::mutex& lock() { return m_; }
    std::string text;            // main body (index, release JSON, file)
    std::vector<std::string> extra;

private:
    std::thread thread_;
    std::atomic<bool> running_{false}, cancel_{false};
    bool started_ = false;
    mutable std::mutex m_;
    Status status_ = Status::ok();
};

// Helper for workers: GET into a string with progress + cancel.
Status fetchInto(INetwork& net, NetJob& job, const std::string& url, uint64_t maxBytes, std::string& out);

}  // namespace henu
