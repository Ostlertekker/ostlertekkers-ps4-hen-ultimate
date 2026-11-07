// OSTLERTEKKERS HEN - app/screens_online.cpp
// Online features: GoldHEN cheat repository download and the online updater.
// Internet is only used after the user allowed it once (settings.allowDownloads).
#include <algorithm>

#include "app/app.h"
#include "app/screens.h"
#include "app/screens_common.h"
#include "backup/backup.h"
#include "cheats/cheat_db.h"
#include "cheats/cheat_naming.h"
#include "core/sha256.h"
#include "files/fs_util.h"

namespace henu {

namespace {
void block(const UiContext& ctx, const std::string& text, float x, float& y, float w, Color c, float maxY) {
    for (const auto& line : wrapText(ctx.renderer, ctx.fonts.small, text, w)) {
        if (y > maxY) return;
        ctx.renderer.drawText(ctx.fonts.small, line, x, y, c);
        y += 30;
    }
}

void progressBar(IRenderer& r, const Theme& t, const Rect& rc, double frac) {
    r.fillRect(rc, withAlpha(t.text, 0.10f), rc.h / 2);
    if (frac > 0) r.fillRect({rc.x, rc.y, std::max(rc.h, static_cast<float>(rc.w * std::min(1.0, frac))), rc.h}, t.accent, rc.h / 2);
}

// Asks once for internet access; onYes runs after it was granted.
ScreenResult withInternet(App& app, std::function<void()> onYes) {
    if (!app.network()) {
        app.toast(app.t("online.noNetwork"));
        return ScreenResult::none();
    }
    app.network()->setAllowUnverifiedTls(app.settings().allowUnverifiedTls);
    if (app.settings().allowDownloads) {
        onYes();
        return ScreenResult::none();
    }
    return ScreenResult::push(std::unique_ptr<Screen>(new ConfirmScreen(
        app, app.t("online.consentTitle"), app.t("online.consentText"), app.t("online.consentYes"), [&app, onYes]() {
            Settings s = app.settings();
            s.allowDownloads = true;
            if (app.updateSettings(s)) onYes();
        })));
}

// The console could not verify the server certificate. Asked once; after a
// yes the connection is accepted and retry runs (downloads stay checked).
ScreenResult offerUnverifiedTls(App& app, const std::string& detail, std::function<void()> retry) {
    return ScreenResult::push(std::unique_ptr<Screen>(new ConfirmScreen(
        app, app.t("online.tlsTitle"), app.t("online.tlsText") + "\n\n" + detail, app.t("online.tlsYes"), [&app, retry]() {
            Settings s = app.settings();
            s.allowUnverifiedTls = true;
            if (!app.updateSettings(s)) return;
            if (app.network()) app.network()->setAllowUnverifiedTls(true);
            app.log().warn("user allowed connections without certificate verification");
            retry();
        })));
}
}  // namespace

// =================================================================== Online cheats

OnlineCheatsScreen::OnlineCheatsScreen(App& app, std::string titleFilter) : Screen(app), filter_(std::move(titleFilter)) {
    list_.rowHeight = 84;
    onlyInstalled_ = filter_.empty();
}

void OnlineCheatsScreen::onEnter() {
    app_.ensureScanned();
    if (entries_.empty() && phase_ == Phase::Idle && error_.empty()) {
        ScreenResult r = withInternet(app_, [this]() { startIndex(); });
        (void)r;   // consent dialog is shown by handle() on Confirm instead (onEnter cannot push)
    }
    rebuild();
}

void OnlineCheatsScreen::startIndex() {
    INetwork* net = app_.network();
    if (!net) return;
    job_.reset(new NetJob);
    phase_ = Phase::Index;
    error_.clear();
    errorCode_ = ErrorCode::None;
    job_->start([net](NetJob& job) -> Status {
        for (const char* fmt : {"json", "shn", "mc4"}) {
            std::string body;
            Status st = fetchInto(*net, job, std::string(kCheatRepoBase) + fmt + ".txt", 8u << 20, body);
            if (!st) return st;
            std::lock_guard<std::mutex> l(job.lock());
            job.extra.push_back(std::string(fmt) + "\n" + body);
        }
        return Status::ok();
    });
}

void OnlineCheatsScreen::update(const UiContext&) {
    if (!job_ || !job_->finished()) return;
    job_->join();
    Status st = job_->status();
    if (phase_ == Phase::Index) {
        if (st) {
            entries_.clear();
            std::lock_guard<std::mutex> l(job_->lock());
            for (const auto& block : job_->extra) {
                const size_t nl = block.find('\n');
                auto part = parseCheatIndex(block.substr(nl + 1), block.substr(0, nl));
                entries_.insert(entries_.end(), part.begin(), part.end());
            }
            app_.log().info("online cheat index: " + std::to_string(entries_.size()) + " files");
            // Remember the titles; if the game folders are not readable, they
            // are matched against the install service right away.
            std::vector<std::pair<std::string, std::string>> titles;
            titles.reserve(entries_.size());
            for (const auto& e : entries_) titles.emplace_back(e.titleId, e.name);
            app_.cacheCheatTitles(titles);
            bool onlyQueried = true;
            for (const auto& g : app_.games().all()) if (!g.fromQuery) { onlyQueried = false; break; }
            if (onlyQueried) app_.startInstalledQuery();
        } else {
            error_ = st.error().message;
            errorCode_ = st.error().code;
            app_.log().warn("online cheat index: " + error_);
        }
    } else if (phase_ == Phase::File) {
        if (st && pending_ < entries_.size()) {
            std::string body;
            {
                std::lock_guard<std::mutex> l(job_->lock());
                body = job_->text;
            }
            install(entries_[pending_], body);
        } else if (!st) {
            app_.toast(app_.t("common.failed") + ": " + st.error().message);
        }
    }
    phase_ = Phase::Idle;
    job_.reset();
    rebuild();
}

bool OnlineCheatsScreen::animating() const { return job_ != nullptr; }

void OnlineCheatsScreen::rebuild() {
    const int keep = list_.focus();
    list_.items.clear();
    visible_.clear();
    const Theme& t = app_.theme();
    for (size_t i = 0; i < entries_.size(); ++i) {
        const OnlineCheatEntry& e = entries_[i];
        const GameInfo* g = app_.games().find(e.titleId);
        if (!filter_.empty() && e.titleId != filter_) continue;
        if (filter_.empty() && onlyInstalled_ && !g) continue;
        ListItem it;
        it.key = e.file;
        it.title = e.name.empty() ? e.titleId : e.name;
        it.subtitle = e.file + " \xC2\xB7 " + e.format;
        const bool have = app_.sysFs().exists(app_.cheatRoot() + "/" + e.format + "/" + e.file);
        if (have) { it.badge = app_.t("online.have"); it.badgeColor = t.textMuted; it.dim = true; }
        else if (g && !g->version.empty() && g->version == e.version) { it.badge = app_.t("online.match"); it.badgeColor = t.ok; }
        else if (g) { it.badge = app_.t("online.otherVersion") + " " + (g->version.empty() ? "?" : g->version); it.badgeColor = t.warn; }
        else { it.badge = "v" + e.version; it.badgeColor = t.textMuted; }
        visible_.push_back(i);
        list_.items.push_back(it);
    }
    // Matching versions first.
    std::vector<size_t> order(visible_.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        auto rank = [&](size_t k) { const auto& it = list_.items[k]; return it.badgeColor.r == t.ok.r && it.badgeColor.g == t.ok.g && !it.dim ? 0 : (it.dim ? 2 : 1); };
        return rank(a) < rank(b);
    });
    std::vector<ListItem> items;
    std::vector<size_t> vis;
    for (size_t k : order) { items.push_back(list_.items[k]); vis.push_back(visible_[k]); }
    list_.items = std::move(items);
    visible_ = std::move(vis);
    list_.setFocus(std::max(0, std::min(keep, static_cast<int>(list_.items.size()) - 1)));
}

void OnlineCheatsScreen::install(const OnlineCheatEntry& e, const std::string& body) {
    if (e.format != "mc4") {
        auto parsed = parseCheatFile(e.file, body);
        if (!parsed || parsed.value().cheats.empty()) {
            app_.toast(app_.t("online.invalidFile"));
            return;
        }
    }
    IFileSystem& fs = app_.sysFs();
    const std::string dir = app_.cheatRoot() + "/" + e.format;
    const std::string target = dir + "/" + e.file;
    Status st = makeDirs(fs, app_.guard(), dir);
    if (st && fs.exists(target)) {
        const std::string keep = app_.dataRoot() + "/backups/replaced-cheats/" + timestampNow() + "_" + e.file;
        st = guardedCopy(fs, app_.guard(), target, keep);
    }
    if (st) {
        auto chk = app_.guard().check(target, AccessKind::Write);
        st = chk ? fs.writeFile(chk.value(), body) : Status::fail(chk.error().code, chk.error().message);
    }
    if (!st) {
        app_.log().error("online cheat install: " + st.error().message);
        app_.toast(app_.t("common.failed") + ": " + st.error().message);
        return;
    }
    app_.log().info("online cheat installed: " + target);
    app_.ensureScanned(true);
    app_.toast(app_.t("online.installed"));
}

ScreenResult OnlineCheatsScreen::handle(const InputEvent& ev) {
    if (ev.action == Action::Back) {
        if (job_) job_->cancel();
        return ScreenResult::pop();
    }
    if (job_) return ScreenResult::none();
    if (entries_.empty()) {
        if (ev.action == Action::Confirm) {
            if (errorCode_ == ErrorCode::CertificateError && !app_.settings().allowUnverifiedTls)
                return offerUnverifiedTls(app_, error_, [this]() { startIndex(); });
            return withInternet(app_, [this]() { startIndex(); });
        }
        return ScreenResult::none();
    }
    if (list_.handle(ev.action)) return ScreenResult::none();
    if (ev.action == Action::Search && filter_.empty()) {
        onlyInstalled_ = !onlyInstalled_;
        rebuild();
        app_.toast(app_.t(onlyInstalled_ ? "online.filterInstalled" : "online.filterAll"));
    } else if (ev.action == Action::Menu) {
        std::vector<MenuAction> a;
        a.push_back({app_.t("menu.search"), filter_, [this]() {
                         return ScreenResult::replace(std::unique_ptr<Screen>(new KeyboardScreen(
                             app_, app_.t("menu.search"), filter_, [this](const std::string& q) {
                                 std::string up = q;
                                 for (auto& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                                 filter_ = isValidTitleId(up) ? up : "";
                                 if (!filter_.empty() || q.empty()) rebuild();
                                 else app_.toast(app_.t("online.needTitleId"));
                             })));
                     }});
        a.push_back({app_.t("online.reload"), "", [this]() { entries_.clear(); startIndex(); return ScreenResult::pop(); }});
        return ScreenResult::push(std::unique_ptr<Screen>(new ActionMenuScreen(app_, app_.t("menu.title"), a)));
    } else if (ev.action == Action::Confirm && list_.focus() < static_cast<int>(visible_.size())) {
        const size_t idx = visible_[static_cast<size_t>(list_.focus())];
        const OnlineCheatEntry e = entries_[idx];
        const bool have = app_.sysFs().exists(app_.cheatRoot() + "/" + e.format + "/" + e.file);
        INetwork* net = app_.network();
        return ScreenResult::push(std::unique_ptr<Screen>(new ConfirmScreen(
            app_, app_.t("online.download"),
            app_.t(have ? "online.confirmReplace" : "online.confirm") + "\n" + app_.cheatRoot() + "/" + e.format + "/" + e.file,
            app_.t("online.downloadDo"), [this, idx, net, e]() {
                if (!net) return;
                pending_ = idx;
                phase_ = Phase::File;
                job_.reset(new NetJob);
                const std::string url = e.url();
                job_->start([net, url](NetJob& job) -> Status {
                    std::string body;
                    Status st = fetchInto(*net, job, url, 4u << 20, body);
                    std::lock_guard<std::mutex> l(job.lock());
                    job.text = std::move(body);
                    return st;
                });
            })));
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> OnlineCheatsScreen::hints() const {
    if (entries_.empty()) return {{"@confirm", "online.load"}};
    std::vector<std::pair<std::string, std::string>> h = {{"@confirm", "online.downloadDo"}};
    if (filter_.empty()) h.push_back({"\xE2\x96\xA1", "hint.filter"});
    h.push_back({"OPT", "hint.menu"});
    return h;
}

void OnlineCheatsScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    std::string sub = filter_.empty() ? app_.t(onlyInstalled_ ? "online.filterInstalled" : "online.filterAll") : filter_;
    if (!entries_.empty()) sub += " \xC2\xB7 " + std::to_string(list_.items.size()) + " / " + std::to_string(entries_.size());
    const float y = drawPageHeader(ctx, app_, app_.t("online.title"), sub);
    const Rect side{kMarginX + 1180, y, kScreenW - 2 * kMarginX - 1180, 1000 - y};
    if (job_) {
        const Rect box{kMarginX, y, 1150, 200};
        drawPanel(r, t, box, t.cornerRadius);
        r.drawText(f.title, app_.t(phase_ == Phase::Index ? "online.loadingIndex" : "online.downloading"), box.x + 30, box.y + 34, t.text);
        const uint64_t done = job_->done, total = job_->total;
        progressBar(r, t, {box.x + 30, box.y + 110, box.w - 60, 14}, total ? static_cast<double>(done) / total : std::fmod(ctx.time, 1.0));
        r.drawText(f.small, formatBytes(done) + (total ? " / " + formatBytes(total) : ""), box.x + 30, box.y + 142, t.textMuted);
    } else if (entries_.empty()) {
        const Rect box{kMarginX, y, 1150, 1000 - y};
        drawPanel(r, t, box, t.cornerRadius);
        float yy = box.y + 34;
        r.drawText(f.title, error_.empty() ? app_.t("online.start") : app_.t("common.failed"), box.x + 30, yy, error_.empty() ? t.text : t.error);
        yy += 64;
        block(ctx, error_.empty() ? app_.t("online.startHelp")
                       : error_ + "\n\n" + (errorCode_ == ErrorCode::CertificateError && !app_.settings().allowUnverifiedTls
                                                  ? app_.t("online.tlsHint") : app_.t("online.errorHelp")), box.x + 30, yy, box.w - 60,
              t.textMuted, box.y + box.h - 30);
    } else {
        list_.draw(r, t, f, {kMarginX, y, 1150, 1000 - y}, ctx.time, app_.t(filter_.empty() ? "online.noneInstalled" : "online.noneForGame"));
    }
    drawPanel(r, t, side, t.cornerRadius);
    float yy = side.y + 30;
    r.drawText(f.label, app_.t("online.source"), side.x + 28, yy, t.textMuted);
    yy += 34;
    block(ctx, "goldhen.github.io/GoldHEN_Cheat_Repository", side.x + 28, yy, side.w - 56, t.text, side.y + side.h);
    yy += 14;
    block(ctx, app_.t("online.note"), side.x + 28, yy, side.w - 56, t.textMuted, side.y + side.h - 30);
}

// =================================================================== Online update

UpdateScreen::UpdateScreen(App& app) : Screen(app) {}

void UpdateScreen::check() {
    INetwork* net = app_.network();
    if (!net) return;
    state_ = State::Checking;
    message_.clear();
    job_.reset(new NetJob);
    job_->start([net](NetJob& job) -> Status {
        std::string body;
        Status st = fetchInto(*net, job, std::string("https://api.github.com/repos/") + kUpdateRepo + "/releases?per_page=10",
                              4u << 20, body);
        std::lock_guard<std::mutex> l(job.lock());
        job.text = std::move(body);
        return st;
    });
}

void UpdateScreen::download() {
    INetwork* net = app_.network();
    if (!net) return;
    // Safety first: settings, favourites and own themes are saved before.
    BackupService b(app_.sysFs(), app_.guard(), app_.dataRoot(), app_.cheatRoot());
    auto bk = b.create(app_.dataRoot() + "/backups", false, timestampNow());
    if (!bk) app_.log().warn("pre-update backup failed: " + bk.error().message);
    state_ = State::Downloading;
    job_.reset(new NetJob);
    const std::string pkgUrl = rel_.pkgUrl, sumsUrl = rel_.sumsUrl;
    job_->start([net, pkgUrl, sumsUrl](NetJob& job) -> Status {
        std::string sums;
        if (!sumsUrl.empty()) {
            Status s = fetchInto(*net, job, sumsUrl, 64 * 1024, sums);
            if (!s) return s;
        }
        std::string pkg;
        Status st = fetchInto(*net, job, pkgUrl, 512ull << 20, pkg);
        std::lock_guard<std::mutex> l(job.lock());
        job.text = std::move(pkg);
        job.extra.push_back(sums);
        return st;
    });
}

void UpdateScreen::update(const UiContext&) {
    if (!job_ || !job_->finished()) return;
    job_->join();
    Status st = job_->status();
    if (!st) {
        state_ = State::Error;
        message_ = st.error().message;
        errorCode_ = st.error().code;
        app_.log().warn("update: " + message_);
        job_.reset();
        return;
    }
    errorCode_ = ErrorCode::None;
    if (state_ == State::Checking) {
        std::string text;
        {
            std::lock_guard<std::mutex> l(job_->lock());
            text = job_->text;
        }
        auto rel = pickNewestRelease(text);
        if (!rel) {
            state_ = State::Error;
            message_ = rel.error().message;
        } else {
            rel_ = rel.value();
            state_ = (kBuildNumber == 0 || rel_.build > kBuildNumber) ? State::Available : State::UpToDate;
            app_.log().info("update: newest build " + std::to_string(rel_.build) + ", installed " + std::to_string(kBuildNumber));
        }
    } else if (state_ == State::Downloading) {
        std::string pkg, sums;
        {
            std::lock_guard<std::mutex> l(job_->lock());
            pkg.swap(job_->text);
            if (!job_->extra.empty()) sums = job_->extra.front();
        }
        auto expected = sha256For(sums, rel_.pkgName);
        const std::string actual = sha256Hex(pkg);
        if (!expected) {
            state_ = State::Error;
            message_ = app_.t("update.noChecksum");
        } else if (expected.value() != actual) {
            state_ = State::Error;
            message_ = app_.t("update.badChecksum");
        } else {
            IFileSystem& fs = app_.sysFs();
            // /data/pkg is GoldHEN's internal Package Installer folder (source "hdd").
            makeDirsUnchecked(fs, "/data/pkg");
            const std::string path = "/data/pkg/OSTLERTEKKERS_HEN_" + rel_.tag + ".pkg";
            auto chk = app_.guard().check(path, AccessKind::Write);
            Status w = chk ? fs.writeFile(chk.value(), pkg) : Status::fail(chk.error().code, chk.error().message);
            if (w) {
                state_ = State::Ready;
                savedPath_ = path;
                app_.log().info("update saved: " + path + " sha256 " + actual);
            } else {
                state_ = State::Error;
                message_ = w.error().message;
            }
        }
    }
    job_.reset();
}

bool UpdateScreen::animating() const { return job_ != nullptr; }

ScreenResult UpdateScreen::handle(const InputEvent& ev) {
    if (ev.action == Action::Back) {
        if (job_) job_->cancel();
        return ScreenResult::none();   // section root: app returns to Home
    }
    if (job_) return ScreenResult::none();
    if (ev.action == Action::Confirm) {
        if (state_ == State::Available) {
            return ScreenResult::push(std::unique_ptr<Screen>(new ConfirmScreen(
                app_, app_.t("update.download"), app_.t("update.confirm") + "\n" + rel_.pkgName + " (" + formatBytes(rel_.pkgSize) + ")",
                app_.t("update.downloadDo"), [this]() { download(); })));
        }
        if (state_ == State::Error && errorCode_ == ErrorCode::CertificateError && !app_.settings().allowUnverifiedTls)
            return offerUnverifiedTls(app_, message_, [this]() { check(); });
        return withInternet(app_, [this]() { check(); });
    }
    if (ev.action == Action::Context) {
        BackupService b(app_.sysFs(), app_.guard(), app_.dataRoot(), app_.cheatRoot());
        auto res = b.create(app_.dataRoot() + "/backups", false, timestampNow());
        app_.toast(res ? app_.t("backup.created") : app_.t("common.failed") + ": " + res.error().message);
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> UpdateScreen::hints() const {
    if (job_) return {};
    return {{"@confirm", state_ == State::Available ? "update.downloadDo" : "update.check"}, {"\xE2\x96\xB3", "update.backupNow"}};
}

void UpdateScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    const float y = drawPageHeader(ctx, app_, app_.t("module.update"), app_.t("update.sub"));
    const Rect p{kMarginX, y, 900, 1000 - y};
    drawPanel(r, t, p, t.cornerRadius);
    const std::string installed = std::string("v") + kAppVersion + (kBuildNumber ? " \xC2\xB7 Build " + std::to_string(kBuildNumber) : " \xC2\xB7 " + app_.t("update.localBuild"));
    std::vector<std::pair<std::string, std::string>> rows = {
        {app_.t("update.installed"), installed},
        {app_.t("update.source"), std::string("github.com/") + kUpdateRepo},
        {app_.t("update.internet"), app_.network() ? (app_.settings().allowDownloads ? app_.t("update.allowed") : app_.t("update.askFirst"))
                                                   : app_.t("common.unavailable")},
    };
    if (rel_.build) {
        rows.push_back({app_.t("update.latest"), "Build " + std::to_string(rel_.build)});
        rows.push_back({app_.t("update.size"), formatBytes(rel_.pkgSize)});
        rows.push_back({app_.t("update.published"), rel_.published.substr(0, 10)});
    }
    float yy = drawInfoRows(r, t, f, p.x + 28, p.y + 30, p.w - 56, rows) + 20;
    std::string status;
    Color sc = t.text;
    switch (state_) {
        case State::Idle: status = app_.t("update.state.idle"); break;
        case State::Checking: status = app_.t("update.state.checking"); break;
        case State::Available: status = app_.t("update.state.available"); sc = t.ok; break;
        case State::UpToDate: status = app_.t("update.state.uptodate"); sc = t.ok; break;
        case State::Downloading: status = app_.t("update.state.downloading"); break;
        case State::Ready:
            status = app_.t("update.state.ready");
            if (app_.network() && app_.network()->usedUnverifiedTls()) status += "\n" + app_.t("online.tlsUsed");
            sc = t.ok;
            break;
        case State::Error:
            status = app_.t("common.failed") + ": " + message_;
            if (errorCode_ == ErrorCode::CertificateError && !app_.settings().allowUnverifiedTls) status += "\n\n" + app_.t("online.tlsHint");
            sc = t.error;
            break;
    }
    block(ctx, status, p.x + 28, yy, p.w - 56, sc, p.y + p.h - 60);
    if (job_) {
        const uint64_t done = job_->done, total = job_->total;
        progressBar(r, t, {p.x + 28, yy + 12, p.w - 56, 14}, total ? static_cast<double>(done) / total : std::fmod(ctx.time, 1.0));
        r.drawText(f.small, formatBytes(done) + (total ? " / " + formatBytes(total) : ""), p.x + 28, yy + 40, t.textMuted);
    }

    const Rect q{kMarginX + 924, y, kScreenW - 2 * kMarginX - 924, 1000 - y};
    drawPanel(r, t, q, t.cornerRadius);
    float qy = q.y + 30;
    if (state_ == State::Ready) {
        r.drawText(f.label, app_.t("update.howInstall"), q.x + 28, qy, t.textMuted);
        qy += 36;
        block(ctx, app_.t("update.installSteps") + "\n\n" + savedPath_, q.x + 28, qy, q.w - 56, t.text, q.y + q.h - 30);
    } else if (rel_.build) {
        r.drawText(f.label, app_.t("update.notes"), q.x + 28, qy, t.textMuted);
        qy += 36;
        block(ctx, rel_.title + "\n" + rel_.notes, q.x + 28, qy, q.w - 56, t.text, q.y + q.h - 30);
    } else {
        block(ctx, app_.t("update.explain"), q.x + 28, qy, q.w - 56, t.textMuted, q.y + q.h - 30);
        qy += 20;
        block(ctx, app_.t("update.goldhenNote"), q.x + 28, qy, q.w - 56, t.textMuted, q.y + q.h - 30);
    }
}

}  // namespace henu
