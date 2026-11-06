// PS4 HEN ULTIMATE - app/screens_tools.cpp
// Theme Center, GoldHEN, System, Network, Update, Files, Backup, Diagnostics.
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <time.h>

#include "app/app.h"
#include "app/screens.h"
#include "app/screens_common.h"
#include "backup/backup.h"
#include "files/fs_util.h"
#include "goldhen/goldhen_info.h"

namespace henu {

namespace {
std::string orUnknown(App& app, const Maybe<std::string>& m) { return m.known ? m.value : app.t("common.notAvailable"); }

std::string uptimeText(App& app) {
    timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return app.t("common.notAvailable");
    const long s = static_cast<long>(ts.tv_sec);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%ld h %02ld min", s / 3600, (s / 60) % 60);
    return buf;
}

std::string spaceText(App& app, const std::string& mount) {
    auto sp = app.sysFs().space(mount);
    if (!sp || sp.value().total == 0) return app.t("common.notAvailable");
    return formatBytes(sp.value().free) + " " + app.t("system.freeOf") + " " + formatBytes(sp.value().total);
}

void textBlock(const UiContext& ctx, const std::string& text, float x, float& y, float w, Color c, float maxY = 1000) {
    for (const auto& line : wrapText(ctx.renderer, ctx.fonts.small, text, w)) {
        if (y > maxY) return;
        ctx.renderer.drawText(ctx.fonts.small, line, x, y, c);
        y += 30;
    }
}
}  // namespace

// =================================================================== Themes

ThemesScreen::ThemesScreen(App& app) : Screen(app) {
    focus_ = std::max(0, app.themes().indexOf(app.settings().themeId));
}

void ThemesScreen::applyTheme(const std::string& id) {
    Settings s = app_.settings();
    s.themeId = id;
    if (app_.updateSettings(s)) app_.toast(app_.t("themes.applied") + " " + app_.theme().name(s.language));
}

ScreenResult ThemesScreen::handle(const InputEvent& ev) {
    const auto& list = app_.themes().themes();
    const int n = static_cast<int>(list.size());
    const int cols = 4;
    if (n == 0) return ScreenResult::none();
    switch (ev.action) {
        case Action::Left: focus_ = std::max(0, focus_ - 1); break;
        case Action::Right: focus_ = std::min(n - 1, focus_ + 1); break;
        case Action::Up: if (focus_ >= cols) focus_ -= cols; break;
        case Action::Down: focus_ = std::min(n - 1, focus_ + cols); break;
        case Action::Confirm: applyTheme(list[static_cast<size_t>(focus_)].id); break;
        case Action::Context: {
            Settings s = app_.settings();
            const std::string id = list[static_cast<size_t>(focus_)].id;
            auto it = std::find(s.favoriteThemes.begin(), s.favoriteThemes.end(), id);
            if (it == s.favoriteThemes.end()) s.favoriteThemes.push_back(id); else s.favoriteThemes.erase(it);
            app_.updateSettings(s);
            break;
        }
        case Action::Search: {   // random theme (favourites first, if any)
            std::vector<std::string> pool = app_.settings().favoriteThemes;
            if (pool.size() < 2) { pool.clear(); for (const auto& t : list) pool.push_back(t.id); }
            pool.erase(std::remove(pool.begin(), pool.end(), app_.settings().themeId), pool.end());
            if (!pool.empty()) {
                const std::string id = pool[static_cast<size_t>(std::rand()) % pool.size()];
                focus_ = std::max(0, app_.themes().indexOf(id));
                applyTheme(id);
            }
            break;
        }
        case Action::Menu: {
            std::vector<MenuAction> a;
            const std::string usb = firstUsbRoot(app_.sysFs());
            const std::string usbDir = usb + "/PS4HENULTIMATE/themes";
            a.push_back({app_.t("themes.import"), usb.empty() ? app_.t("common.noUsb") : usbDir, [this, usbDir]() {
                             int ok = 0, bad = 0;
                             auto entries = app_.sysFs().list(usbDir);
                             if (entries) {
                                 for (const auto& e : entries.value()) {
                                     if (e.isDir || e.name.size() < 6 || e.name.compare(e.name.size() - 5, 5, ".json") != 0) continue;
                                     auto text = app_.sysFs().readFile(joinPath(usbDir, e.name), 64 * 1024);
                                     auto j = text ? parseJson(text.value()) : Result<Json>::fail(ErrorCode::IoError, "read");
                                     auto th = j ? parseTheme(j.value()) : Result<Theme>::fail(ErrorCode::ParseError, "json");
                                     if (!th) { ++bad; app_.log().warn("theme import " + e.name + ": " + th.error().message); continue; }
                                     Status st = guardedCopy(app_.sysFs(), app_.guard(), joinPath(usbDir, e.name),
                                                             app_.dataRoot() + "/themes/" + th.value().id + ".json");
                                     if (st) ++ok; else ++bad;
                                 }
                             }
                             app_.rescanThemes();
                             app_.toast(std::to_string(ok) + " " + app_.t("themes.imported") + (bad ? ", " + std::to_string(bad) + " " + app_.t("common.errors") : ""));
                             return ScreenResult::pop();
                         }, !usb.empty()});
            a.push_back({app_.t("themes.export"), usb.empty() ? app_.t("common.noUsb") : usbDir, [this, usbDir]() {
                             Status st = makeDirs(app_.sysFs(), app_.guard(), usbDir);
                             if (st) st = app_.sysFs().writeFile(usbDir + "/" + app_.theme().id + ".json", themeToJson(app_.theme()).dump(2));
                             app_.toast(st ? app_.t("common.exported") : app_.t("common.failed") + ": " + st.error().message);
                             return ScreenResult::pop();
                         }, !usb.empty()});
            return ScreenResult::push(std::unique_ptr<Screen>(new ActionMenuScreen(app_, app_.t("menu.title"), a)));
        }
        default: break;
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> ThemesScreen::hints() const {
    return {{"@confirm", "themes.apply"}, {"\xE2\x96\xB3", "hint.favorite"}, {"\xE2\x96\xA1", "themes.random"}, {"OPT", "hint.menu"}};
}

void ThemesScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Fonts& f = ctx.fonts;
    const Theme& cur = ctx.theme;
    const auto& list = app_.themes().themes();
    const std::string lang = app_.settings().language;
    const float y0 = drawPageHeader(ctx, app_, app_.t("module.themes"),
                                    std::to_string(list.size()) + " Themes \xC2\xB7 " + app_.t("themes.sub"));
    const int cols = 4;
    const float gap = 22, w = (kScreenW - 2 * kMarginX - gap * (cols - 1)) / cols, h = 200;
    const int visRows = 3;
    const int row = focus_ / cols;
    if (row < top_) top_ = row;
    if (row >= top_ + visRows) top_ = row - visRows + 1;
    for (int i = top_ * cols; i < static_cast<int>(list.size()) && i < (top_ + visRows) * cols; ++i) {
        const Theme& t = list[static_cast<size_t>(i)];
        const int rr = i / cols - top_, cc = i % cols;
        const Rect card{kMarginX + cc * (w + gap), y0 + rr * (h + gap), w, h};
        // mini preview in the theme's own colours
        r.fillRect(card, t.background, 20);
        r.gradientRect({card.x, card.y, card.w, card.h}, t.background, mix(t.background, t.accent, 0.25f), 20);
        for (int k = 0; k < 4 && k < static_cast<int>(t.tileGradients.size()); ++k)
            r.gradientRect({card.x + 20 + k * 58.f, card.y + 20, 48, 48}, t.tileGradients[static_cast<size_t>(k)].first,
                           t.tileGradients[static_cast<size_t>(k)].second, 10);
        r.fillRect({card.x + card.w - 44, card.y + 28, 22, 22}, t.accent, 11);
        r.drawText(f.bodyBold, t.name(lang), card.x + 20, card.y + 96, t.text);
        r.drawText(f.small, app_.t("themes.cat." + t.category) + (t.builtIn ? "" : " \xC2\xB7 " + app_.t("themes.own")),
                   card.x + 20, card.y + 136, t.textMuted);
        const auto& fav = app_.settings().favoriteThemes;
        if (std::find(fav.begin(), fav.end(), t.id) != fav.end()) r.drawText(f.bodyBold, "\xE2\x98\x85", card.x + card.w - 44, card.y + 136, t.warn);
        if (t.id == app_.settings().themeId)
            drawPill(r, f, app_.t("themes.active"), card.x + card.w - 20 - pillWidth(r, f, app_.t("themes.active")),
                     card.y + card.h - 50, withAlpha(t.accent, 0.3f), t.text);
        r.strokeRect(card, withAlpha(cur.text, 0.12f), 1.5f, 20);
        if (i == focus_) drawFocusRing(r, cur, card, 20, ctx.time);
    }
}

// =================================================================== GoldHEN (read-only)

void GoldhenScreen::onEnter() {
    plugins_.items.clear();
    GoldhenStatus s = detectGoldhen(app_.sysFs());
    for (const auto& p : s.plugins) {
        ListItem it;
        it.title = baseName(p.path);
        it.subtitle = "[" + p.section + "] " + p.path;
        it.badge = p.fileExists ? app_.t("goldhen.present") : app_.t("goldhen.missing");
        it.badgeColor = p.fileExists ? app_.theme().ok : app_.theme().warn;
        plugins_.items.push_back(it);
    }
    plugins_.rowHeight = 76;
}

ScreenResult GoldhenScreen::handle(const InputEvent& ev) {
    plugins_.handle(ev.action);
    return ScreenResult::none();
}

void GoldhenScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    const float y = drawPageHeader(ctx, app_, "GoldHEN", app_.t("goldhen.sub"));
    GoldhenStatus s = detectGoldhen(app_.sysFs());
    SystemSnapshot sys = app_.systemInfo();
    const std::string fw = sys.firmware.known ? sys.firmware.value : "";
    const std::string minBuild = minimumGoldhenFor(fw);
    const Rect p{kMarginX, y, 860, 1000 - y};
    drawPanel(r, t, p, t.cornerRadius);
    float yy = drawInfoRows(r, t, f, p.x + 28, p.y + 30, p.w - 56,
                            {{app_.t("goldhen.folder"), s.folderFound ? app_.t("goldhen.found") : app_.t("goldhen.notFound")},
                             {app_.t("common.firmware"), fw.empty() ? app_.t("common.notAvailable") : fw},
                             {app_.t("goldhen.minBuild"), minBuild.empty() ? app_.t("goldhen.notListed") : minBuild},
                             {app_.t("goldhen.build"), app_.t("goldhen.buildUnknown")},
                             {"Cheats JSON / SHN / MC4", std::to_string(s.cheatsJson) + " / " + std::to_string(s.cheatsShn) + " / " + std::to_string(s.cheatsMc4)},
                             {app_.t("goldhen.patches"), std::to_string(s.patchFiles)},
                             {"config.ini", s.configFound ? app_.t("goldhen.found") : app_.t("goldhen.notFound")},
                             {"plugins.ini", s.pluginsIniFound ? std::to_string(s.plugins.size()) + " " + app_.t("goldhen.entries") : app_.t("goldhen.notFound")}}) + 16;
    textBlock(ctx, app_.t("goldhen.readOnly"), p.x + 28, yy, p.w - 56, t.textMuted, p.y + p.h - 30);
    const Rect q{kMarginX + 884, y, kScreenW - 2 * kMarginX - 884, 1000 - y};
    r.drawText(f.label, app_.t("goldhen.plugins"), q.x, q.y, t.textMuted);
    plugins_.draw(r, t, f, {q.x, q.y + 36, q.w, q.h - 36}, ctx.time, app_.t("goldhen.noPlugins"));
}

// =================================================================== System

ScreenResult SystemScreen::handle(const InputEvent&) { return ScreenResult::none(); }

void SystemScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    app_.ensureScanned();
    SystemSnapshot s = app_.systemInfo();
    const float y = drawPageHeader(ctx, app_, app_.t("module.system"), app_.t("system.sub"));
    const std::string fw = s.firmware.known ? s.firmware.value : "";
    const std::string min = minimumGoldhenFor(fw);
    const Rect p{kMarginX, y, (kScreenW - 2 * kMarginX - 24) / 2, 1000 - y};
    drawPanel(r, t, p, t.cornerRadius);
    r.drawText(f.label, app_.t("system.console"), p.x + 28, p.y + 26, t.textMuted);
    drawInfoRows(r, t, f, p.x + 28, p.y + 70, p.w - 56,
                 {{app_.t("common.firmware"), orUnknown(app_, s.firmware)},
                  {app_.t("system.goldhenSupport"), fw.empty() ? app_.t("common.notAvailable") : (min.empty() ? app_.t("goldhen.notListed") : app_.t("system.since") + " " + min)},
                  {app_.t("system.language"), orUnknown(app_, s.language)},
                  {app_.t("system.confirm"), s.circleIsConfirm.known ? (s.circleIsConfirm.value ? "\xE2\x97\x8B" : "\xE2\x9C\x95") : app_.t("common.notAvailable")},
                  {app_.t("system.uptime"), uptimeText(app_)},
                  {app_.t("system.runningApp"), app_.t("system.thisApp")}});
    const Rect q{p.x + p.w + 24, y, p.w, 1000 - y};
    drawPanel(r, t, q, t.cornerRadius);
    r.drawText(f.label, app_.t("system.storage"), q.x + 28, q.y + 26, t.textMuted);
    const std::string usb = firstUsbRoot(app_.sysFs());
    drawInfoRows(r, t, f, q.x + 28, q.y + 70, q.w - 56,
                 {{app_.t("system.internal"), spaceText(app_, "/user")},
                  {"USB", usb.empty() ? app_.t("common.noUsb") : usb + ": " + spaceText(app_, usb)},
                  {app_.t("module.games"), std::to_string(app_.games().gameCount())},
                  {app_.t("module.homebrew"), std::to_string(app_.games().homebrewCount())},
                  {app_.t("cheats.files"), std::to_string(app_.cheats().files().size())},
                  {app_.t("system.network"), s.ipAddress.known ? s.ipAddress.value : app_.t("system.offline")}});
}

// =================================================================== Network

ScreenResult NetworkScreen::handle(const InputEvent&) { return ScreenResult::none(); }

void NetworkScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    SystemSnapshot s = app_.systemInfo();
    const float y = drawPageHeader(ctx, app_, app_.t("module.network"), app_.t("network.sub"));
    const Rect p{kMarginX, y, 900, 1000 - y};
    drawPanel(r, t, p, t.cornerRadius);
    const bool up = s.ipAddress.known && s.ipAddress.value != "0.0.0.0";
    float yy = drawInfoRows(r, t, f, p.x + 28, p.y + 30, p.w - 56,
                            {{app_.t("network.status"), up ? app_.t("network.connected") : app_.t("system.offline")},
                             {app_.t("network.ip"), s.ipAddress.known ? s.ipAddress.value : app_.t("common.notAvailable")},
                             {"GoldHEN FTP", up ? s.ipAddress.value + ":2121" : "-"},
                             {app_.t("network.downloads"), app_.t("network.downloadsOff")}}) + 16;
    textBlock(ctx, app_.t("network.ftpHelp"), p.x + 28, yy, p.w - 56, t.textMuted);
    const Rect q{kMarginX + 924, y, kScreenW - 2 * kMarginX - 924, 1000 - y};
    drawPanel(r, t, q, t.cornerRadius);
    float qy = q.y + 30;
    r.drawText(f.label, app_.t("module.statusLabel"), q.x + 28, qy, t.textMuted);
    qy += 40;
    textBlock(ctx, app_.t("network.limits"), q.x + 28, qy, q.w - 56, t.text);
}

// =================================================================== Update

ScreenResult UpdateScreen::handle(const InputEvent& ev) {
    if (ev.action == Action::Confirm) {
        BackupService b(app_.sysFs(), app_.guard(), app_.dataRoot(), app_.cheatRoot());
        auto res = b.create(app_.dataRoot() + "/backups", false, timestampNow());
        app_.toast(res ? app_.t("backup.created") : app_.t("common.failed") + ": " + res.error().message);
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> UpdateScreen::hints() const { return {{"@confirm", "update.backupNow"}}; }

void UpdateScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    const float y = drawPageHeader(ctx, app_, app_.t("module.update"), app_.t("update.sub"));
    const Rect p{kMarginX, y, 900, 1000 - y};
    drawPanel(r, t, p, t.cornerRadius);
    float yy = drawInfoRows(r, t, f, p.x + 28, p.y + 30, p.w - 56,
                            {{app_.t("update.installed"), std::string("v") + kAppVersion},
                             {"Title ID", "HENU00001"},
                             {app_.t("update.autoCheck"), app_.t("update.autoCheckOff")},
                             {app_.t("update.source"), "GitHub Releases"}}) + 16;
    textBlock(ctx, app_.t("update.manual"), p.x + 28, yy, p.w - 56, t.text);
    const Rect q{kMarginX + 924, y, kScreenW - 2 * kMarginX - 924, 1000 - y};
    drawPanel(r, t, q, t.cornerRadius);
    float qy = q.y + 30;
    textBlock(ctx, app_.t("update.goldhenNote"), q.x + 28, qy, q.w - 56, t.textMuted);
}

// =================================================================== Files

void FilesScreen::onEnter() { open(dir_); }

void FilesScreen::open(const std::string& dir) {
    dir_ = dir;
    list_.items.clear();
    IFileSystem& fs = app_.sysFs();
    if (dir_.empty()) {
        std::vector<std::pair<std::string, std::string>> roots = {
            {app_.dataRoot(), "files.root.app"}, {"/data/GoldHEN", "files.root.goldhen"}, {"/data", "files.root.data"},
            {"/user/app", "files.root.games"}};
        for (int i = 0; i < 8; ++i) roots.push_back({"/mnt/usb" + std::to_string(i), "files.root.usb"});
        for (const auto& rt : roots) {
            if (!fs.exists(rt.first)) continue;
            ListItem it;
            it.key = rt.first;
            it.title = rt.first;
            it.subtitle = app_.t(rt.second);
            it.badge = app_.guard().canWrite(rt.first + "/x") ? app_.t("files.writable") : app_.t("files.readOnly");
            it.badgeColor = app_.guard().canWrite(rt.first + "/x") ? app_.theme().ok : app_.theme().textMuted;
            list_.items.push_back(it);
        }
        list_.setFocus(0);
        return;
    }
    auto entries = fs.list(dir_);
    if (!entries) { app_.toast(entries.error().message); return; }
    auto v = entries.value();
    std::sort(v.begin(), v.end(), [](const DirEntry& a, const DirEntry& b) {
        if (a.isDir != b.isDir) return a.isDir;
        return a.name < b.name;
    });
    for (const auto& e : v) {
        if (list_.items.size() >= 2000) break;
        ListItem it;
        it.key = joinPath(dir_, e.name);
        it.title = e.isDir ? e.name + "/" : e.name;
        it.right = e.isDir ? "" : formatBytes(e.size);
        it.dim = false;
        list_.items.push_back(it);
    }
    list_.rowHeight = 64;
    list_.setFocus(0);
}

ScreenResult FilesScreen::handle(const InputEvent& ev) {
    if (list_.handle(ev.action)) return ScreenResult::none();
    const ListItem* cur = list_.current();
    switch (ev.action) {
        case Action::Back:
            if (dir_.empty()) return ScreenResult::pop();
            {
                // leave to the root list when at one of the start folders
                const bool usbRoot = dir_.size() == 9 && dir_.compare(0, 8, "/mnt/usb") == 0;
                const bool atRoot = usbRoot || dir_ == app_.dataRoot() || dir_ == "/data/GoldHEN" || dir_ == "/data" ||
                                    dir_ == "/user/app";
                open(atRoot ? "" : parentPath(dir_));
            }
            return ScreenResult::none();
        case Action::Confirm:
            if (!cur) break;
            if (dir_.empty() || (cur->title.size() && cur->title.back() == '/')) { open(cur->key); break; }
            [[fallthrough]];
        case Action::Menu: {
            if (!cur || dir_.empty()) break;
            const std::string path = cur->key;
            const bool isDir = cur->title.size() && cur->title.back() == '/';
            const std::string usb = firstUsbRoot(app_.sysFs());
            auto canDelete = app_.guard().check(path, AccessKind::Write);
            std::vector<MenuAction> a;
            if (!isDir) {
                const std::string dest = usb + "/PS4HENULTIMATE/export/" + baseName(path);
                a.push_back({app_.t("files.copyUsb"), usb.empty() ? app_.t("common.noUsb") : dest, [this, path, dest]() {
                                 Status st = guardedCopy(app_.sysFs(), app_.guard(), path, dest);
                                 app_.toast(st ? app_.t("common.exported") : app_.t("common.failed") + ": " + st.error().message);
                                 return ScreenResult::pop();
                             }, !usb.empty() && dir_.compare(0, 8, "/mnt/usb") != 0});
            }
            a.push_back({app_.t("files.delete"), canDelete ? path : app_.t("files.protected"), [this, path]() {
                             return ScreenResult::replace(std::unique_ptr<Screen>(new ConfirmScreen(
                                 app_, app_.t("files.delete"), app_.t("files.deleteConfirm") + "\n" + path, app_.t("files.delete"),
                                 [this, path]() {
                                     Status st = guardedRemoveTree(app_.sysFs(), app_.guard(), path);
                                     app_.log().info("delete " + path + ": " + (st ? "ok" : st.error().message));
                                     app_.toast(st ? app_.t("files.deleted") : app_.t("common.failed") + ": " + st.error().message);
                                     open(dir_);
                                 }, true)));
                         }, canDelete.isOk()});
            return ScreenResult::push(std::unique_ptr<Screen>(new ActionMenuScreen(app_, baseName(path), a)));
        }
        default: break;
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> FilesScreen::hints() const {
    return {{"@confirm", "hint.open"}, {"OPT", "hint.menu"}};
}

void FilesScreen::draw(const UiContext& ctx) {
    std::string sub = dir_.empty() ? app_.t("files.sub") : dir_;
    if (!dir_.empty()) sub += " \xC2\xB7 " + std::to_string(list_.items.size()) + " " + app_.t("files.entries");
    const float y = drawPageHeader(ctx, app_, app_.t("module.files"), sub);
    list_.draw(ctx.renderer, ctx.theme, ctx.fonts, {kMarginX, y, kScreenW - 2 * kMarginX, 1000 - y}, ctx.time, app_.t("files.empty"));
}

// =================================================================== Backup

std::string BackupScreen::target() {
    if (app_.settings().backupTarget == "usb") {
        const std::string usb = firstUsbRoot(app_.sysFs());
        if (!usb.empty()) return usb + "/PS4HENULTIMATE/backups";
    }
    return app_.dataRoot() + "/backups";
}

void BackupScreen::onEnter() { rebuild(); }

void BackupScreen::rebuild() {
    list_.items.clear();
    dirs_.clear();
    const Theme& t = app_.theme();
    ListItem a1;
    a1.title = app_.t("backup.createData");
    a1.subtitle = app_.t("backup.createDataSub");
    a1.badge = "+";
    a1.badgeColor = t.ok;
    ListItem a2 = a1;
    a2.title = app_.t("backup.createAll");
    a2.subtitle = app_.t("backup.createAllSub");
    list_.items.push_back(a1);
    list_.items.push_back(a2);
    BackupService b(app_.sysFs(), app_.guard(), app_.dataRoot(), app_.cheatRoot());
    for (const auto& bi : b.list(target())) {
        if (bi.stamp == "safety") continue;
        ListItem it;
        it.title = bi.stamp;
        it.subtitle = std::to_string(bi.files) + " " + app_.t("backup.files") + (bi.includesCheats ? " \xC2\xB7 " + app_.t("backup.withCheats") : "");
        it.right = formatBytes(bi.bytes);
        if (!bi.manifestOk) { it.badge = app_.t("backup.noManifest"); it.badgeColor = t.error; }
        list_.items.push_back(it);
        dirs_.push_back(bi.dir);
    }
    list_.setFocus(list_.focus());
}

ScreenResult BackupScreen::handle(const InputEvent& ev) {
    if (list_.handle(ev.action)) return ScreenResult::none();
    if (ev.action == Action::Search) {
        Settings s = app_.settings();
        s.backupTarget = s.backupTarget == "usb" ? "internal" : "usb";
        if (s.backupTarget == "usb" && firstUsbRoot(app_.sysFs()).empty()) { app_.toast(app_.t("common.noUsb")); return ScreenResult::none(); }
        app_.updateSettings(s);
        rebuild();
        return ScreenResult::none();
    }
    if (ev.action != Action::Confirm) return ScreenResult::none();
    const int i = list_.focus();
    if (i < 2) {
        BackupService b(app_.sysFs(), app_.guard(), app_.dataRoot(), app_.cheatRoot());
        auto res = b.create(target(), i == 1, timestampNow());
        if (res) {
            app_.log().info("backup created: " + res.value().dir);
            app_.toast(app_.t("backup.created") + " (" + std::to_string(res.value().files) + ")");
        } else {
            app_.log().error("backup: " + res.error().message);
            app_.toast(app_.t("common.failed") + ": " + res.error().message);
        }
        rebuild();
        return ScreenResult::none();
    }
    const std::string dir = dirs_[static_cast<size_t>(i - 2)];
    std::vector<MenuAction> a;
    a.push_back({app_.t("backup.verify"), "", [this, dir]() {
                     BackupService b(app_.sysFs(), app_.guard(), app_.dataRoot(), app_.cheatRoot());
                     auto bad = b.verify(dir);
                     app_.toast(bad.empty() ? app_.t("backup.verifyOk") : app_.t("backup.verifyBad") + " " + std::to_string(bad.size()));
                     return ScreenResult::pop();
                 }});
    a.push_back({app_.t("backup.restore"), app_.t("backup.restoreSub"), [this, dir]() {
                     return ScreenResult::replace(std::unique_ptr<Screen>(new ConfirmScreen(
                         app_, app_.t("backup.restore"), app_.t("backup.restoreConfirm"), app_.t("backup.restore"), [this, dir]() {
                             BackupService b(app_.sysFs(), app_.guard(), app_.dataRoot(), app_.cheatRoot());
                             Status st = b.restore(dir, app_.dataRoot() + "/backups/safety", timestampNow());
                             app_.log().info("restore " + dir + ": " + (st ? "ok" : st.error().message));
                             if (st) { app_.ensureScanned(true); app_.reloadUserData(); }
                             app_.toast(st ? app_.t("backup.restored") : app_.t("common.failed") + ": " + st.error().message);
                         })));
                 }});
    a.push_back({app_.t("backup.delete"), dir, [this, dir]() {
                     return ScreenResult::replace(std::unique_ptr<Screen>(new ConfirmScreen(
                         app_, app_.t("backup.delete"), dir, app_.t("backup.delete"), [this, dir]() {
                             BackupService b(app_.sysFs(), app_.guard(), app_.dataRoot(), app_.cheatRoot());
                             Status st = b.remove(dir);
                             app_.toast(st ? app_.t("files.deleted") : app_.t("common.failed") + ": " + st.error().message);
                         }, true)));
                 }});
    return ScreenResult::push(std::unique_ptr<Screen>(new ActionMenuScreen(app_, baseName(dir), a)));
}

std::vector<std::pair<std::string, std::string>> BackupScreen::hints() const {
    return {{"@confirm", "hint.select"}, {"\xE2\x96\xA1", "backup.toggleTarget"}};
}

void BackupScreen::draw(const UiContext& ctx) {
    const float y = drawPageHeader(ctx, app_, app_.t("module.backup"), app_.t("backup.target") + " " + target());
    list_.draw(ctx.renderer, ctx.theme, ctx.fonts, {kMarginX, y, kScreenW - 2 * kMarginX, 1000 - y}, ctx.time, "");
}

// =================================================================== Diagnostics

void DiagScreen::onEnter() {
    list_.items.clear();
    const Theme& t = app_.theme();
    for (const auto& m : app_.modules()) {
        ListItem it;
        it.title = app_.t("module." + m.id);
        it.subtitle = m.note;
        it.badge = app_.t(std::string("status.") + toString(m.status));
        it.badgeColor = m.status == CapStatus::Available ? t.ok : m.status == CapStatus::Limited ? t.warn
                        : m.status == CapStatus::Unverified ? t.accent2 : t.textMuted;
        list_.items.push_back(it);
    }
    list_.rowHeight = 70;
}

ScreenResult DiagScreen::handle(const InputEvent& ev) {
    if (list_.handle(ev.action)) return ScreenResult::none();
    if (ev.action == Action::Menu) {
        const std::string usb = firstUsbRoot(app_.sysFs());
        std::vector<MenuAction> a;
        a.push_back({app_.t("diag.export"), usb.empty() ? app_.t("common.noUsb") : usb + "/PS4HENULTIMATE/diag", [this, usb]() {
                         app_.log().flush();
                         const std::string dir = usb + "/PS4HENULTIMATE/diag/" + timestampNow();
                         Status st = guardedCopy(app_.sysFs(), app_.guard(), app_.dataRoot() + "/logs/app.log", dir + "/app.log");
                         if (st) {
                             std::string caps;
                             for (const auto& m : app_.modules()) caps += m.id + ": " + toString(m.status) + "\n";
                             SystemSnapshot s = app_.systemInfo();
                             caps += std::string("firmware: ") + (s.firmware.known ? s.firmware.value : "?") + "\nversion: " + kAppVersion + "\n";
                             st = app_.sysFs().writeFile(dir + "/status.txt", caps);
                         }
                         app_.toast(st ? app_.t("common.exported") : app_.t("common.failed") + ": " + st.error().message);
                         return ScreenResult::pop();
                     }, !usb.empty()});
        return ScreenResult::push(std::unique_ptr<Screen>(new ActionMenuScreen(app_, app_.t("menu.title"), a)));
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> DiagScreen::hints() const { return {{"OPT", "hint.menu"}}; }

void DiagScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    const float y = drawPageHeader(ctx, app_, app_.t("module.diag"), app_.t("diag.sub"));
    list_.draw(r, t, f, {kMarginX, y, 760, 1000 - y}, ctx.time, "");
    const Rect p{kMarginX + 790, y, kScreenW - 2 * kMarginX - 790, 1000 - y};
    drawPanel(r, t, p, t.cornerRadius);
    float yy = p.y + 26;
    r.drawText(f.label, app_.t("diag.controls"), p.x + 28, yy, t.textMuted);
    yy += 36;
    textBlock(ctx, app_.t("diag.controlsText"), p.x + 28, yy, p.w - 56, t.text);
    yy += 10;
    r.drawText(f.label, app_.t("diag.log"), p.x + 28, yy, t.textMuted);
    yy += 34;
    const auto& lines = app_.log().recent();
    const size_t maxLines = static_cast<size_t>(std::max(0.f, (p.y + p.h - yy - 20) / 28));
    const size_t start = lines.size() > maxLines ? lines.size() - maxLines : 0;
    for (size_t i = start; i < lines.size(); ++i) {
        const std::string& l = lines[i];
        const Color c = l.compare(0, 7, "[ERROR]") == 0 ? t.error : l.compare(0, 6, "[WARN]") == 0 ? t.warn : t.textMuted;
        r.drawText(f.small, ellipsize(r, f.small, l, p.w - 56), p.x + 28, yy, c);
        yy += 28;
    }
}

}  // namespace henu
