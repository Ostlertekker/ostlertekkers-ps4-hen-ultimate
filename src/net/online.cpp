// OSTLERTEKKERS HEN - net/online.cpp
#include "net/online.h"

#include <cctype>

#include "cheats/cheat_naming.h"
#include "core/json.h"

namespace henu {

std::vector<OnlineCheatEntry> parseCheatIndex(const std::string& text, const std::string& format) {
    std::vector<OnlineCheatEntry> out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) eol = text.size();
        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        OnlineCheatEntry e;
        e.file = line.substr(0, eq);
        e.name = line.substr(eq + 1);
        e.format = format;
        // Only plain file names: no paths, no URL tricks.
        if (e.file.find('/') != std::string::npos || e.file.find('\\') != std::string::npos ||
            e.file.find("..") != std::string::npos)
            continue;
        auto id = parseCheatFileName(e.file);
        if (!id || toString(id.value().format) != format) continue;
        e.titleId = id.value().titleId;
        e.version = id.value().version.toString();
        out.push_back(std::move(e));
    }
    return out;
}

int buildFromTag(const std::string& tag) {
    if (tag.compare(0, 6, "build-") != 0 || tag.size() == 6 || tag.size() > 15) return 0;
    int v = 0;
    for (size_t i = 6; i < tag.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(tag[i]))) return 0;
        v = v * 10 + (tag[i] - '0');
    }
    return v;
}

Result<ReleaseInfo> pickNewestRelease(const std::string& text) {
    using R = Result<ReleaseInfo>;
    auto j = parseJson(text);
    if (!j) return R::fail(ErrorCode::ParseError, "release list: " + j.error().message);
    if (!j.value().isArray()) {
        const std::string msg = j.value().get("message").asString();
        return R::fail(ErrorCode::NotFound, msg.empty() ? "release list: unexpected answer" : "GitHub: " + msg);
    }
    ReleaseInfo best;
    for (const auto& rel : j.value().asArray()) {
        if (rel.get("draft").asBool(false)) continue;
        ReleaseInfo info;
        info.tag = rel.get("tag_name").asString();
        info.build = buildFromTag(info.tag);
        if (info.build <= 0) continue;
        info.title = rel.get("name").asString();
        info.notes = rel.get("body").asString();
        info.published = rel.get("published_at").asString();
        for (const auto& a : rel.get("assets").asArray()) {
            const std::string name = a.get("name").asString();
            const std::string url = a.get("browser_download_url").asString();
            if (url.compare(0, 8, "https://") != 0) continue;
            // Releases can also hold the separate system theme PKG: take the
            // app (title HENU00001), or the first PKG when none says so.
            const bool isPkg = name.size() > 4 && name.compare(name.size() - 4, 4, ".pkg") == 0;
            const bool isApp = name.find("HENU00001") != std::string::npos;
            if (isPkg && (info.pkgUrl.empty() || (isApp && info.pkgName.find("HENU00001") == std::string::npos))) {
                info.pkgName = name;
                info.pkgUrl = url;
                info.pkgSize = static_cast<uint64_t>(a.get("size").asNumber(0));
            } else if (name == "SHA256SUMS.txt") {
                info.sumsUrl = url;
            }
        }
        if (info.pkgUrl.empty()) continue;
        if (info.build > best.build) best = info;
    }
    if (best.build == 0) return R::fail(ErrorCode::NotFound, "no release with a PKG found");
    return R::ok(best);
}

Result<std::string> sha256For(const std::string& sums, const std::string& fileName) {
    size_t pos = 0;
    while (pos < sums.size()) {
        size_t eol = sums.find('\n', pos);
        if (eol == std::string::npos) eol = sums.size();
        std::string line = sums.substr(pos, eol - pos);
        pos = eol + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 66) continue;
        std::string hash = line.substr(0, 64);
        size_t n = 64;
        while (n < line.size() && (line[n] == ' ' || line[n] == '*')) ++n;
        if (line.substr(n) != fileName) continue;
        bool hex = true;
        for (auto& c : hash) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (!std::isxdigit(static_cast<unsigned char>(c))) hex = false;
        }
        if (hex) return Result<std::string>::ok(hash);
    }
    return Result<std::string>::fail(ErrorCode::NotFound, "no checksum for " + fileName);
}

void NetJob::start(std::function<Status(NetJob&)> work) {
    cancel();
    join();
    cancel_ = false;
    done = 0;
    total = 0;
    started_ = true;
    running_ = true;
    {
        std::lock_guard<std::mutex> l(m_);
        status_ = Status::ok();
        text.clear();
        extra.clear();
    }
    thread_ = std::thread([this, work]() {
        Status st = work(*this);
        {
            std::lock_guard<std::mutex> l(m_);
            status_ = st;
        }
        running_ = false;
    });
}

Status fetchInto(INetwork& net, NetJob& job, const std::string& url, uint64_t maxBytes, std::string& out) {
    out.clear();
    return net.stream(url, maxBytes, [&](const char* d, size_t n, uint64_t done, uint64_t total) {
        out.append(d, n);
        job.done = done;
        job.total = total;
        return !job.cancelled();
    });
}

}  // namespace henu
