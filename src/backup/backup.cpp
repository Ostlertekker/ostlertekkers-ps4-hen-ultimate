// OSTLERTEKKERS HEN - backup/backup.cpp
#include "backup/backup.h"

#include <algorithm>
#include <ctime>

#include "core/json.h"
#include "core/sha256.h"
#include "files/fs_util.h"

namespace henu {

std::string timestampNow() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm);
    return buf;
}

BackupService::BackupService(IFileSystem& fs, const PathGuard& guard, std::string dataRoot, std::string cheatRoot)
    : fs_(fs), guard_(guard), dataRoot_(std::move(dataRoot)), cheatRoot_(std::move(cheatRoot)) {}

std::vector<std::string> BackupService::sources(bool includeCheats) const {
    std::vector<std::string> out;
    for (const char* f : {"/settings.json", "/favorites.json"})
        if (fs_.exists(dataRoot_ + f)) out.push_back(dataRoot_ + f);
    for (const auto& r : listFilesRecursive(fs_, dataRoot_ + "/themes", 2)) out.push_back(r.path);
    if (includeCheats)
        for (const char* sub : {"/json", "/shn", "/mc4"})
            for (const auto& r : listFilesRecursive(fs_, cheatRoot_ + sub, 1)) out.push_back(r.path);
    return out;
}

Result<BackupInfo> BackupService::create(const std::string& target, bool includeCheats, const std::string& stamp) {
    using R = Result<BackupInfo>;
    const std::string dir = joinPath(target, stamp);
    if (fs_.exists(dir)) return R::fail(ErrorCode::InvalidArgument, "backup already exists: " + dir);
    Status st = makeDirs(fs_, guard_, dir + "/files");
    if (!st) return R::fail(st.error().code, st.error().message);

    BackupInfo info;
    info.dir = dir;
    info.stamp = stamp;
    info.created = stamp;
    info.includesCheats = includeCheats;
    Json files = Json::array();
    for (const auto& src : sources(includeCheats)) {
        auto data = fs_.readFile(src, 64u << 20);
        if (!data) return R::fail(data.error().code, src + ": " + data.error().message);
        const std::string rel = src.substr(1);
        const std::string dst = dir + "/files/" + rel;
        st = makeDirs(fs_, guard_, parentPath(dst));
        if (!st) return R::fail(st.error().code, st.error().message);
        st = fs_.writeFile(dst, data.value());
        if (!st) return R::fail(st.error().code, st.error().message);
        Json e = Json::object();
        e.set("path", Json::string(src));
        e.set("size", Json::number(static_cast<double>(data.value().size())));
        e.set("sha256", Json::string(sha256Hex(data.value())));
        files.push(e);
        info.files++;
        info.bytes += data.value().size();
    }
    Json m = Json::object();
    m.set("schemaVersion", Json::number(1));
    m.set("created", Json::string(stamp));
    m.set("includesCheats", Json::boolean(includeCheats));
    m.set("files", files);
    st = fs_.writeFile(dir + "/manifest.json", m.dump(2));
    if (!st) return R::fail(st.error().code, st.error().message);
    info.manifestOk = true;
    return R::ok(info);
}

std::vector<BackupInfo> BackupService::list(const std::string& target) {
    std::vector<BackupInfo> out;
    auto entries = fs_.list(target);
    if (!entries) return out;
    for (const auto& e : entries.value()) {
        if (!e.isDir) continue;
        BackupInfo b;
        b.dir = joinPath(target, e.name);
        b.stamp = e.name;
        auto text = fs_.readFile(b.dir + "/manifest.json", 8u << 20);
        if (text) {
            auto j = parseJson(text.value());
            if (j) {
                b.manifestOk = true;
                b.created = j.value().get("created").asString(e.name);
                b.includesCheats = j.value().get("includesCheats").asBool();
                for (const auto& f : j.value().get("files").asArray()) {
                    b.files++;
                    b.bytes += static_cast<uint64_t>(f.get("size").asNumber());
                }
            }
        }
        out.push_back(b);
    }
    std::sort(out.begin(), out.end(), [](const BackupInfo& a, const BackupInfo& b) { return a.stamp > b.stamp; });
    return out;
}

std::vector<std::string> BackupService::verify(const std::string& dir) {
    std::vector<std::string> bad;
    auto text = fs_.readFile(dir + "/manifest.json", 8u << 20);
    if (!text) return {"manifest.json"};
    auto j = parseJson(text.value());
    if (!j) return {"manifest.json"};
    for (const auto& f : j.value().get("files").asArray()) {
        const std::string path = f.get("path").asString();
        auto data = fs_.readFile(dir + "/files/" + path.substr(path.empty() ? 0 : 1), 64u << 20);
        if (!data || sha256Hex(data.value()) != f.get("sha256").asString()) bad.push_back(path);
    }
    return bad;
}

Status BackupService::restore(const std::string& dir, const std::string& safetyTarget, const std::string& stamp) {
    auto bad = verify(dir);
    if (!bad.empty()) return Status::fail(ErrorCode::IntegrityError, "checksum mismatch: " + bad.front());
    auto text = fs_.readFile(dir + "/manifest.json", 8u << 20);
    auto j = parseJson(text.value());
    bool cheats = j.value().get("includesCheats").asBool();
    // Safety copy of the current state first.
    auto safety = create(safetyTarget, cheats, stamp);
    if (!safety) return Status::fail(safety.error().code, "safety backup failed: " + safety.error().message);
    for (const auto& f : j.value().get("files").asArray()) {
        const std::string path = f.get("path").asString();
        // Only dashboard data and GoldHEN cheat folders may be restored.
        const bool allowed = path.compare(0, dataRoot_.size() + 1, dataRoot_ + "/") == 0 ||
                             PathGuard::isGoldhenCheatPath(PathGuard::normalize(path).isOk() ? PathGuard::normalize(path).value() : "");
        if (!allowed) return Status::fail(ErrorCode::AccessDenied, "not restorable: " + path);
        Status st = guardedCopy(fs_, guard_, dir + "/files/" + path.substr(1), path);
        if (!st) return st;
    }
    return Status::ok();
}

Status BackupService::remove(const std::string& dir) { return guardedRemoveTree(fs_, guard_, dir); }

}  // namespace henu
