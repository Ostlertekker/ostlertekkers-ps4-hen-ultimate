// OSTLERTEKKERS HEN - app/screens_games_cheats.cpp
// Games, Homebrew and the Cheat Center.
#include <algorithm>
#include <cctype>
#include <functional>

#include "app/app.h"
#include "app/screens.h"
#include "app/screens_common.h"
#include "backup/backup.h"
#include "cheats/cheat_naming.h"
#include "files/fs_util.h"

namespace henu {

namespace {
std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

const char* formatLabel(CheatFormat f) {
    switch (f) {
        case CheatFormat::Json: return "JSON";
        case CheatFormat::Shn: return "SHN";
        case CheatFormat::Mc4: return "MC4";
    }
    return "?";
}

void compatBadge(App& app, const Theme& t, Compatibility c, ListItem& it) {
    switch (c) {
        case Compatibility::Compatible: it.badge = app.t("cheats.badge.ok"); it.badgeColor = t.ok; break;
        case Compatibility::VersionMismatch: it.badge = app.t("cheats.badge.version"); it.badgeColor = t.warn; break;
        case Compatibility::GameNotInstalled: it.badge = app.t("cheats.badge.notInstalled"); it.badgeColor = t.textMuted; break;
        case Compatibility::WrongTitle: it.badge = "?"; break;
    }
}

const CheatFile* findCheat(App& app, const std::string& fileName) {
    for (const auto& f : app.cheats().files())
        if (f.fileName == fileName) return &f;
    return nullptr;
}
}  // namespace

// =================================================================== Games

namespace {
constexpr int kGridCols = 5;
constexpr int kGridRows = 2;
constexpr float kCellW = 214, kCellH = 318, kCellGap = 20;

// Icon or a generated placeholder tile (initials on a theme gradient).
void drawGameIcon(App& app, IRenderer& r, const Theme& t, const Fonts& f, const GameInfo& g, const Rect& rc, float radius) {
    const TextureId tex = app.iconTexture(g);
    if (tex) {
        r.drawTexture(tex, rc, 1.0f, radius);
    } else {
        const size_t k = std::hash<std::string>{}(g.titleId) % std::max<size_t>(1, t.tileGradients.size());
        const auto grad = t.tileGradients.empty() ? std::make_pair(t.accent, t.accent2) : t.tileGradients[k];
        r.gradientRect(rc, grad.first, grad.second, radius);
        std::string ini;
        for (char c : g.name) {
            if (std::isalnum(static_cast<unsigned char>(c)) && (ini.empty() || ini.size() < 2)) ini += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (ini.size() == 2) break;
        }
        r.drawText(f.h2, ini.empty() ? "?" : ini, rc.x + rc.w / 2, rc.y + rc.h / 2 - 28, withAlpha({255, 255, 255, 255}, 0.92f), TextAlign::Center);
    }
    r.strokeRect(rc, withAlpha(t.text, 0.10f), 1.5f, radius);
}
}  // namespace

GamesScreen::GamesScreen(App& app, bool homebrew) : Screen(app), homebrew_(homebrew) {}

void GamesScreen::onEnter() {
    app_.ensureScanned();
    rebuild();
}

void GamesScreen::rebuild() {
    const std::string key = focus_ < static_cast<int>(ids_.size()) ? ids_[static_cast<size_t>(focus_)] : "";
    std::vector<const GameInfo*> v;
    for (const auto& g : app_.games().all()) {
        if (g.homebrew != homebrew_) continue;
        if (filter_ == 1 && !app_.favorites().isGame(g.titleId)) continue;
        if (filter_ == 2 && app_.cheats().forTitle(g.titleId).empty()) continue;
        if (!query_.empty()) {
            const std::string q = lower(query_);
            if (lower(g.name).find(q) == std::string::npos && lower(g.titleId).find(q) == std::string::npos &&
                lower(toString(g.region)) != q)
                continue;
        }
        v.push_back(&g);
    }
    if (sortById_) std::sort(v.begin(), v.end(), [](const GameInfo* a, const GameInfo* b) { return a->titleId < b->titleId; });
    // Favourites first.
    std::stable_sort(v.begin(), v.end(), [this](const GameInfo* a, const GameInfo* b) {
        return app_.favorites().isGame(a->titleId) && !app_.favorites().isGame(b->titleId);
    });
    ids_.clear();
    focus_ = 0;
    for (const GameInfo* g : v) {
        if (g->titleId == key) focus_ = static_cast<int>(ids_.size());
        ids_.push_back(g->titleId);
    }
}

ScreenResult GamesScreen::handle(const InputEvent& ev) {
    const int n = static_cast<int>(ids_.size());
    const std::string cur = focus_ < n ? ids_[static_cast<size_t>(focus_)] : "";
    switch (ev.action) {
        case Action::Left: if (focus_ % kGridCols > 0) --focus_; break;
        case Action::Right: if (focus_ % kGridCols < kGridCols - 1 && focus_ + 1 < n) ++focus_; break;
        case Action::Up: if (focus_ >= kGridCols) focus_ -= kGridCols; break;
        case Action::Down:
            if (focus_ + kGridCols < n) focus_ += kGridCols;
            else if (focus_ / kGridCols < (n - 1) / kGridCols) focus_ = n - 1;
            break;
        case Action::Context:
            if (!cur.empty()) { app_.favorites().toggleGame(cur); rebuild(); }
            break;
        case Action::Search:
            filter_ = (filter_ + 1) % (homebrew_ ? 2 : 3);
            rebuild();
            app_.toast(app_.t("games.filter." + std::to_string(filter_)));
            break;
        case Action::Confirm:
            if (!cur.empty()) return ScreenResult::push(std::unique_ptr<Screen>(new GameDetailScreen(app_, cur)));
            break;
        case Action::Menu: {
            std::vector<MenuAction> a;
            a.push_back({app_.t("menu.search"), query_.empty() ? "" : app_.t("menu.current") + " " + query_, [this]() {
                             return ScreenResult::replace(std::unique_ptr<Screen>(new KeyboardScreen(
                                 app_, app_.t("menu.search"), query_, [this](const std::string& q) { query_ = q; rebuild(); })));
                         }});
            if (!query_.empty())
                a.push_back({app_.t("menu.clearSearch"), "", [this]() { query_.clear(); rebuild(); return ScreenResult::pop(); }});
            a.push_back({sortById_ ? app_.t("games.sortName") : app_.t("games.sortId"), "", [this]() {
                             sortById_ = !sortById_; rebuild(); return ScreenResult::pop();
                         }});
            if (!homebrew_)
                a.push_back({app_.t("online.title"), app_.t("online.menuDetail"), [this]() {
                                 return ScreenResult::replace(std::unique_ptr<Screen>(new OnlineCheatsScreen(app_, "")));
                             }});
            a.push_back({app_.t("menu.rescan"), "", [this]() { app_.ensureScanned(true); rebuild(); return ScreenResult::pop(); }});
            return ScreenResult::push(std::unique_ptr<Screen>(new ActionMenuScreen(app_, app_.t("menu.title"), a)));
        }
        default: break;
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> GamesScreen::hints() const {
    return {{"@confirm", "games.open"}, {"\xE2\x96\xB3", "hint.favorite"}, {"\xE2\x96\xA1", "hint.filter"}, {"OPT", "hint.menu"}};
}

void GamesScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    const size_t total = homebrew_ ? app_.games().homebrewCount() : app_.games().gameCount();
    std::string sub = std::to_string(total) + " " + app_.t(homebrew_ ? "homebrew.count" : "games.count") + " \xC2\xB7 " +
                      app_.t("games.filter." + std::to_string(filter_));
    if (!query_.empty()) sub += " \xC2\xB7 \"" + query_ + "\"";
    const float y = drawPageHeader(ctx, app_, app_.t(homebrew_ ? "module.homebrew" : "module.games"), sub);
    const int n = static_cast<int>(ids_.size());
    const Rect p{kMarginX + 1190, y, kScreenW - 2 * kMarginX - 1190, 1000 - y};

    if (n == 0) {
        // Explain why the list is empty (sandbox, no titles, filter).
        const Rect box{kMarginX, y, 1160, 1000 - y};
        drawPanel(r, t, box, t.cornerRadius);
        float yy = box.y + 34;
        r.drawText(f.title, app_.t(total ? "games.noMatch" : "games.none"), box.x + 30, yy, t.text);
        yy += 64;
        std::string why;
        if (!total) {
            const std::string& note = app_.gameScanNote();
            if (!app_.games().notes().empty()) why = app_.t("games.scanError") + " " + app_.games().notes().front();
            if (!note.empty()) why += (why.empty() ? "" : "\n") + app_.t("games.noSandboxEscape") + " " + note;
            if (why.empty()) why = app_.t("games.noneHelp");
        }
        for (const auto& line : wrapText(r, f.body, why, box.w - 60)) {
            r.drawText(f.body, line, box.x + 30, yy, t.textMuted);
            yy += 36;
        }
        drawPanel(r, t, p, t.cornerRadius);
        float py = p.y + 30;
        for (const auto& line : wrapText(r, f.small, app_.t("games.sourceNote"), p.w - 56)) {
            r.drawText(f.small, line, p.x + 28, py, t.textMuted);
            py += 30;
        }
        return;
    }

    focus_ = std::max(0, std::min(focus_, n - 1));
    const int row = focus_ / kGridCols;
    if (row < top_) top_ = row;
    if (row >= top_ + kGridRows) top_ = row - kGridRows + 1;
    for (int i = top_ * kGridCols; i < n && i < (top_ + kGridRows) * kGridCols; ++i) {
        const GameInfo* g = app_.games().find(ids_[static_cast<size_t>(i)]);
        if (!g) continue;
        const int rr = i / kGridCols - top_, cc = i % kGridCols;
        const Rect cell{kMarginX + cc * (kCellW + kCellGap), y + rr * (kCellH + kCellGap), kCellW, kCellH};
        const bool foc = i == focus_;
        if (foc) r.fillRect(inflate(cell, 6), withAlpha(t.surfaceAlt, 0.95f), 22);
        const Rect icon{cell.x + 7, cell.y + 7, cell.w - 14, cell.w - 14};
        drawGameIcon(app_, r, t, f, *g, icon, 18);
        if (app_.favorites().isGame(g->titleId)) {
            r.fillRect({icon.x + 10, icon.y + 10, 40, 40}, withAlpha({0, 0, 0, 255}, 0.55f), 20);
            r.drawText(f.bodyBold, "\xE2\x98\x85", icon.x + 30, icon.y + 16, t.warn, TextAlign::Center);
        }
        const size_t cheats = homebrew_ ? 0 : app_.cheats().forTitle(g->titleId).size();
        if (cheats) {
            const std::string b = std::to_string(cheats) + " " + app_.t(cheats == 1 ? "games.cheatFile1" : "games.cheatFiles");
            const float bw = pillWidth(r, f, b);
            drawPill(r, f, b, icon.x + icon.w - bw - 8, icon.y + icon.h - 44, withAlpha({0, 0, 0, 255}, 0.7f), t.accent2);
        }
        r.drawText(f.bodyBold, ellipsize(r, f.bodyBold, g->name, cell.w - 14), cell.x + 9, icon.y + icon.h + 14, t.text);
        r.drawText(f.small, g->titleId + (g->version.empty() ? "" : "  v" + g->version), cell.x + 9, icon.y + icon.h + 52, t.textMuted);
        if (foc) drawFocusRing(r, t, inflate(cell, 6), 22, ctx.time);
    }
    // scroll indicator
    const int rows = (n + kGridCols - 1) / kGridCols;
    if (rows > kGridRows) {
        const float trackX = kMarginX + kGridCols * (kCellW + kCellGap) - 6, trackH = 2 * kCellH + kCellGap;
        const float th = std::max(40.f, trackH * kGridRows / rows);
        const float ty = y + (trackH - th) * top_ / std::max(1, rows - kGridRows);
        r.fillRect({trackX, y, 6, trackH}, withAlpha(t.text, 0.08f), 3);
        r.fillRect({trackX, ty, 6, th}, withAlpha(t.text, 0.45f), 3);
    }

    // ---- side panel: focused title
    drawPanel(r, t, p, t.cornerRadius);
    const GameInfo* g = app_.games().find(ids_[static_cast<size_t>(focus_)]);
    if (!g) return;
    r.drawText(f.bodyBold, ellipsize(r, f.bodyBold, g->name, p.w - 56), p.x + 28, p.y + 28, t.text);
    std::vector<std::pair<std::string, std::string>> rows2 = {
        {"Title ID", g->titleId},
        {app_.t("games.version"), g->version.empty() ? app_.t("common.unknown") : g->version},
        {app_.t("games.region"), toString(g->region)},
        {app_.t("games.storage"), app_.t(g->external ? "games.storage.ext" : "games.storage.int")},
    };
    if (!homebrew_) {
        size_t ok = 0;
        auto files = app_.cheats().forTitle(g->titleId);
        for (const auto* c : files) if (compatibilityFor(*c, app_.games()) == Compatibility::Compatible) ++ok;
        rows2.push_back({app_.t("games.cheatFilesLong"), std::to_string(files.size()) + " (" + std::to_string(ok) + " " + app_.t("games.matching") + ")"});
    }
    float yy = drawInfoRows(r, t, f, p.x + 28, p.y + 84, p.w - 56, rows2) + 16;
    for (const auto& line : wrapText(r, f.small, app_.t("games.openHelp"), p.w - 56)) {
        r.drawText(f.small, line, p.x + 28, yy, t.textMuted);
        yy += 28;
    }
}

// =================================================================== Game detail

GameDetailScreen::GameDetailScreen(App& app, std::string titleId) : Screen(app), id_(std::move(titleId)) {
    actions_.rowHeight = 84;
}

void GameDetailScreen::onEnter() {
    const int keep = actions_.focus();
    actions_.items.clear();
    const GameInfo* g = app_.games().find(id_);
    if (!g) return;
    const Theme& t = app_.theme();
    auto files = app_.cheats().forTitle(id_);
    size_t ok = 0;
    for (const auto* c : files) if (compatibilityFor(*c, app_.games()) == Compatibility::Compatible) ++ok;
    auto add = [&](const std::string& key, const std::string& title, const std::string& sub, const std::string& badge, Color bc) {
        ListItem it;
        it.key = key;
        it.title = title;
        it.subtitle = sub;
        it.badge = badge;
        it.badgeColor = bc;
        actions_.items.push_back(it);
    };
    add("launch", app_.t("detail.launch"), app_.t("detail.launchSub"),
        app_.canLaunch() ? "" : app_.t("common.unavailable"), app_.canLaunch() ? t.ok : t.warn);
    if (!g->homebrew) {
        add("cheats", app_.t("detail.cheats"), app_.t("detail.cheatsSub"),
            std::to_string(files.size()) + " / " + std::to_string(ok) + " " + app_.t("games.matching"), ok ? t.ok : t.textMuted);
        add("online", app_.t("detail.online"), app_.t("detail.onlineSub"), app_.network() ? "" : app_.t("common.unavailable"),
            t.warn);
        add("import", app_.t("detail.import"), app_.t("detail.importSub"), "", t.accent);
    }
    add("fav", app_.favorites().isGame(id_) ? app_.t("detail.unfav") : app_.t("detail.fav"), "", "", t.accent);
    add("folder", app_.t("detail.folder"), app_.cheatRoot(), "", t.accent);
    add("export", app_.t("detail.export"), app_.t("detail.exportSub"), "", t.accent);
    actions_.setFocus(std::min(keep, static_cast<int>(actions_.items.size()) - 1));
}

ScreenResult GameDetailScreen::handle(const InputEvent& ev) {
    if (ev.action == Action::Back) return ScreenResult::pop();
    if (actions_.handle(ev.action)) return ScreenResult::none();
    const ListItem* cur = actions_.current();
    const GameInfo* g = app_.games().find(id_);
    if (!g || !cur) return ScreenResult::none();
    if (ev.action == Action::Context) { app_.favorites().toggleGame(id_); onEnter(); return ScreenResult::none(); }
    if (ev.action != Action::Confirm) return ScreenResult::none();
    const std::string& k = cur->key;
    if (k == "launch") { app_.startLaunch(*g); return ScreenResult::none(); }
    if (k == "cheats") { app_.setCheatQuery(id_); return ScreenResult::goTo("cheats"); }
    if (k == "online") return ScreenResult::push(std::unique_ptr<Screen>(new OnlineCheatsScreen(app_, id_)));
    if (k == "import") return ScreenResult::push(std::unique_ptr<Screen>(new CheatImportScreen(app_)));
    if (k == "fav") { app_.favorites().toggleGame(id_); onEnter(); app_.toast(app_.t("toast.saved")); }
    if (k == "folder") return ScreenResult::goTo("files");
    if (k == "export") {
        const std::string usb = firstUsbRoot(app_.sysFs());
        if (usb.empty()) { app_.toast(app_.t("common.noUsb")); return ScreenResult::none(); }
        std::string info = "Title: " + g->name + "\nTitle ID: " + g->titleId + "\nVersion: " + g->version +
                           "\nRegion: " + toString(g->region) + "\nContent ID: " + g->contentId + "\nPath: " + g->appPath + "\n\nCheat files:\n";
        for (const auto* c : app_.cheats().forTitle(id_))
            info += "  " + c->fileName + "  (" + std::to_string(c->cheats.size()) + " cheats)\n";
        const std::string dir = usb + "/OSTLERTEKKERS_HEN/export";
        Status st = makeDirs(app_.sysFs(), app_.guard(), dir);
        if (st) {
            auto chk = app_.guard().check(dir + "/" + id_ + ".txt", AccessKind::Write);
            st = chk ? app_.sysFs().writeFile(chk.value(), info) : Status::fail(chk.error().code, chk.error().message);
        }
        for (const auto* c : app_.cheats().forTitle(id_)) {
            if (!st) break;
            st = guardedCopy(app_.sysFs(), app_.guard(), c->path, dir + "/" + c->fileName);
        }
        app_.toast(st ? app_.t("detail.exported") + " " + dir : app_.t("common.failed") + ": " + st.error().message);
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> GameDetailScreen::hints() const {
    return {{"@confirm", "hint.select"}, {"\xE2\x96\xB3", "hint.favorite"}};
}

void GameDetailScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    const GameInfo* g = app_.games().find(id_);
    if (!g) { drawPageHeader(ctx, app_, id_, app_.t("common.unknown")); return; }
    const float y = drawPageHeader(ctx, app_, ellipsize(r, f.h2, g->name, kScreenW - 2 * kMarginX),
                                   g->titleId + " \xC2\xB7 " + (g->homebrew ? "Homebrew" : app_.t("module.games")));
    const Rect icon{kMarginX, y, 340, 340};
    drawGameIcon(app_, r, t, f, *g, icon, 26);
    const Rect info{kMarginX, y + 360, 340, 1000 - y - 360};
    drawPanel(r, t, info, t.cornerRadius);
    drawInfoRows(r, t, f, info.x + 22, info.y + 22, info.w - 44,
                 {{app_.t("games.version"), g->version.empty() ? "?" : g->version},
                  {app_.t("games.region"), toString(g->region)},
                  {app_.t("games.category"), g->category.empty() ? "-" : (g->category == "gd" || g->category == "gde" || g->category == "gp" || g->category == "ac") ? app_.t("games.cat." + g->category) : g->category},
                  {app_.t("games.storage"), app_.t(g->external ? "games.storage.ext" : "games.storage.int")}});
    actions_.draw(r, t, f, {kMarginX + 370, y, kScreenW - 2 * kMarginX - 370, 1000 - y}, ctx.time, "");
}

// =================================================================== Cheat Center

CheatsScreen::CheatsScreen(App& app) : Screen(app) {}

void CheatsScreen::onEnter() {
    app_.ensureScanned();
    const std::string q = app_.takeCheatQuery();
    if (!q.empty()) { query_ = q; filter_ = 0; }
    rebuild();
}

void CheatsScreen::rebuild() {
    const std::string key = list_.current() ? list_.current()->key : "";
    list_.items.clear();
    const Theme& t = app_.theme();
    std::vector<const CheatFile*> v;
    if (filter_ == 3) {
        for (const auto& name : app_.favorites().recent()) {
            const CheatFile* f = findCheat(app_, name);
            if (f) v.push_back(f);
        }
    } else {
        for (const auto& f : app_.cheats().files()) {
            if (filter_ == 1 && compatibilityFor(f, app_.games()) != Compatibility::Compatible) continue;
            if (filter_ == 2 && !app_.favorites().isCheat(f.fileName)) continue;
            v.push_back(&f);
        }
    }
    int restore = 0;
    for (const CheatFile* f : v) {
        const GameInfo* g = app_.games().find(f->id.titleId);
        if (!matchesQuery(*f, g, query_)) continue;
        ListItem it;
        it.key = f->fileName;
        it.title = !f->gameName.empty() ? f->gameName : (g ? g->name : f->id.titleId);
        it.subtitle = f->id.titleId + " \xC2\xB7 v" + f->id.version.toString() + " \xC2\xB7 " + formatLabel(f->id.format) +
                      " \xC2\xB7 " + (f->readable ? std::to_string(f->cheats.size()) + " Cheats" : app_.t("cheats.encrypted")) +
                      (f->id.variant.empty() ? "" : " \xC2\xB7 " + f->id.variant);
        it.star = app_.favorites().isCheat(f->fileName);
        compatBadge(app_, t, compatibilityFor(*f, app_.games()), it);
        if (it.key == key) restore = static_cast<int>(list_.items.size());
        list_.items.push_back(it);
    }
    list_.setFocus(restore);
}

ScreenResult CheatsScreen::handle(const InputEvent& ev) {
    if (list_.handle(ev.action)) return ScreenResult::none();
    const ListItem* cur = list_.current();
    switch (ev.action) {
        case Action::Confirm:
            if (cur) return ScreenResult::push(std::unique_ptr<Screen>(new CheatDetailScreen(app_, cur->key)));
            break;
        case Action::Context:
            if (cur) { app_.favorites().toggleCheat(cur->key); rebuild(); }
            break;
        case Action::Search:
            filter_ = (filter_ + 1) % 4;
            rebuild();
            app_.toast(app_.t("cheats.filter." + std::to_string(filter_)));
            break;
        case Action::Menu: {
            std::vector<MenuAction> a;
            a.push_back({app_.t("menu.search"), app_.t("cheats.searchHelp"), [this]() {
                             return ScreenResult::replace(std::unique_ptr<Screen>(new KeyboardScreen(
                                 app_, app_.t("menu.search"), query_, [this](const std::string& q) { query_ = q; rebuild(); })));
                         }});
            if (!query_.empty())
                a.push_back({app_.t("menu.clearSearch"), query_, [this]() { query_.clear(); rebuild(); return ScreenResult::pop(); }});
            a.push_back({app_.t("cheats.import"), app_.t("cheats.importHelp"), [this]() {
                             return ScreenResult::replace(std::unique_ptr<Screen>(new CheatImportScreen(app_)));
                         }});
            const ListItem* sel = list_.current();
            const std::string usb = firstUsbRoot(app_.sysFs());
            MenuAction exp{app_.t("cheats.export"), usb.empty() ? app_.t("common.noUsb") : usb + "/OSTLERTEKKERS_HEN/export/cheats", nullptr,
                           sel != nullptr && !usb.empty()};
            if (sel) {
                const std::string file = sel->key;
                exp.run = [this, file, usb]() {
                    const CheatFile* f = findCheat(app_, file);
                    if (!f) return ScreenResult::pop();
                    Status st = guardedCopy(app_.sysFs(), app_.guard(), f->path, usb + "/OSTLERTEKKERS_HEN/export/cheats/" + f->fileName);
                    app_.toast(st ? app_.t("common.exported") : app_.t("common.failed") + ": " + st.error().message);
                    if (!st) app_.log().error("cheat export: " + st.error().message);
                    return ScreenResult::pop();
                };
            }
            a.push_back(exp);
            a.push_back({app_.t("menu.rescan"), "", [this]() { app_.ensureScanned(true); rebuild(); return ScreenResult::pop(); }});
            a.push_back({app_.t("cheats.download"), app_.t("cheats.downloadHelp"), nullptr, false});
            return ScreenResult::push(std::unique_ptr<Screen>(new ActionMenuScreen(app_, app_.t("menu.title"), a)));
        }
        default: break;
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> CheatsScreen::hints() const {
    return {{"@confirm", "hint.open"}, {"\xE2\x96\xB3", "hint.favorite"}, {"\xE2\x96\xA1", "hint.filter"}, {"OPT", "hint.menu"}};
}

void CheatsScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    const CheatDatabase& db = app_.cheats();
    std::string sub = std::to_string(db.files().size()) + " " + app_.t("cheats.files") + " \xC2\xB7 " +
                      std::to_string(db.totalCheats()) + " Cheats \xC2\xB7 " + app_.t("cheats.filter." + std::to_string(filter_));
    if (!query_.empty()) sub += " \xC2\xB7 \"" + query_ + "\"";
    const float y = drawPageHeader(ctx, app_, app_.t("module.cheats"), sub);
    const bool any = !db.files().empty();
    list_.draw(r, t, f, {kMarginX, y, 1150, 1000 - y}, ctx.time, app_.t(any ? "cheats.noMatch" : "cheats.none"));

    const Rect p{kMarginX + 1180, y, kScreenW - 2 * kMarginX - 1180, 1000 - y};
    drawPanel(r, t, p, t.cornerRadius);
    const ListItem* cur = list_.current();
    const CheatFile* c = cur ? findCheat(app_, cur->key) : nullptr;
    float yy;
    if (!c) {
        yy = drawInfoRows(r, t, f, p.x + 28, p.y + 30, p.w - 56,
                          {{"JSON", std::to_string(db.count(CheatFormat::Json))},
                           {"SHN", std::to_string(db.count(CheatFormat::Shn))},
                           {"MC4", std::to_string(db.count(CheatFormat::Mc4))},
                           {app_.t("cheats.problems"), std::to_string(db.errors().size())}}) + 16;
    } else {
        const GameInfo* g = app_.games().find(c->id.titleId);
        std::vector<std::pair<std::string, std::string>> rows = {
            {app_.t("cheats.fileVersion"), c->id.version.toString()},
            {app_.t("cheats.installed"), g ? (g->version.empty() ? app_.t("common.unknown") : g->version) : app_.t("common.no")},
            {app_.t("games.region"), g ? toString(g->region) : "-"},
            {"Process", c->process.empty() ? "-" : c->process},
            {app_.t("cheats.credits"), c->credits.empty() ? "-" : c->credits.front()},
        };
        yy = drawInfoRows(r, t, f, p.x + 28, p.y + 30, p.w - 56, rows) + 16;
        const Compatibility comp = compatibilityFor(*c, app_.games());
        std::string msg = app_.t(comp == Compatibility::Compatible        ? "cheats.msg.ok"
                                 : comp == Compatibility::VersionMismatch ? "cheats.msg.version"
                                                                          : "cheats.msg.notInstalled");
        // GoldHEN uses only one format per title+version.
        int sameKey = 0;
        for (const auto* o : db.forTitle(c->id.titleId))
            if (o->id.version == c->id.version && o->id.format != c->id.format) ++sameKey;
        if (sameKey) msg += " " + app_.t("cheats.msg.multiFormat");
        if (c->note == "version-mismatch" || c->note == "id-mismatch") msg += " " + app_.t("cheats.msg.contentMismatch");
        for (const auto& line : wrapText(r, f.small, msg, p.w - 56)) {
            r.drawText(f.small, line, p.x + 28, yy, comp == Compatibility::Compatible ? t.ok : t.warn);
            yy += 28;
        }
        yy += 10;
    }
    for (const auto& line : wrapText(r, f.small, app_.t("cheats.howTo"), p.w - 56)) {
        if (yy > p.y + p.h - 30) break;
        r.drawText(f.small, line, p.x + 28, yy, t.textMuted);
        yy += 28;
    }
}

// =================================================================== Cheat detail

CheatDetailScreen::CheatDetailScreen(App& app, std::string fileName) : Screen(app), file_(std::move(fileName)) {}

void CheatDetailScreen::onEnter() {
    app_.favorites().addRecent(file_);
    rebuild();
}

void CheatDetailScreen::rebuild() {
    list_.items.clear();
    indices_.clear();
    const CheatFile* c = findCheat(app_, file_);
    if (!c) return;
    for (size_t i = 0; i < c->cheats.size(); ++i) {
        const CheatEntry& e = c->cheats[i];
        if (category_ >= 0 && static_cast<int>(e.category) != category_) continue;
        indices_.push_back(i);
        ListItem it;
        it.title = e.name;
        it.subtitle = e.description.empty() ? (e.type.empty() ? "" : e.type) : e.description;
        it.badge = app_.t(std::string("cat.") + toString(e.category));
        it.badgeColor = app_.theme().accent2;
        list_.items.push_back(it);
    }
    list_.rowHeight = 80;
    list_.setFocus(list_.focus());
}

ScreenResult CheatDetailScreen::handle(const InputEvent& ev) {
    if (ev.action == Action::Back) return ScreenResult::pop();
    if (list_.handle(ev.action)) return ScreenResult::none();
    if (ev.action == Action::Search) {
        // cycle through categories present in this file
        const CheatFile* c = findCheat(app_, file_);
        if (!c) return ScreenResult::none();
        std::vector<int> present;
        for (const auto& e : c->cheats) {
            int k = static_cast<int>(e.category);
            if (std::find(present.begin(), present.end(), k) == present.end()) present.push_back(k);
        }
        std::sort(present.begin(), present.end());
        auto it = std::find(present.begin(), present.end(), category_);
        category_ = (category_ < 0) ? (present.empty() ? -1 : present.front())
                    : (it == present.end() || it + 1 == present.end()) ? -1 : *(it + 1);
        rebuild();
        app_.toast(category_ < 0 ? app_.t("cat.all") : app_.t(std::string("cat.") + toString(static_cast<CheatCategory>(category_))));
    } else if (ev.action == Action::Context) {
        app_.favorites().toggleCheat(file_);
        app_.toast(app_.favorites().isCheat(file_) ? app_.t("common.favAdded") : app_.t("common.favRemoved"));
    } else if (ev.action == Action::Menu) {
        return openMenu();
    }
    return ScreenResult::none();
}

bool CheatDetailScreen::editable() const {
    auto id = parseCheatFileName(file_);
    return id && id.value().format != CheatFormat::Mc4;
}

std::string CheatDetailScreen::filePath() const {
    auto id = parseCheatFileName(file_);
    return app_.cheatRoot() + "/" + (id ? toString(id.value().format) : "json") + "/" + file_;
}

std::string CheatDetailScreen::originalBackup() const { return app_.dataRoot() + "/backups/edited-cheats/" + file_; }

bool CheatDetailScreen::writeEdited(const std::string& content, const std::string& doneKey) {
    IFileSystem& fs = app_.sysFs();
    const std::string path = filePath();
    if (!fs.exists(originalBackup())) {
        Status b = guardedCopy(fs, app_.guard(), path, originalBackup());
        if (!b) { app_.toast(app_.t("common.failed") + ": " + b.error().message); return false; }
    }
    auto chk = app_.guard().check(path, AccessKind::Write);
    Status w = chk ? fs.writeFile(chk.value(), content) : Status::fail(chk.error().code, chk.error().message);
    if (!w) {
        app_.log().error("cheat edit: " + w.error().message);
        app_.toast(app_.t("common.failed") + ": " + w.error().message);
        return false;
    }
    app_.log().info("cheat file edited: " + path);
    app_.ensureScanned(true);
    rebuild();
    app_.toast(app_.t(doneKey));
    return true;
}

void CheatDetailScreen::applyEdit(const CheatEdit& edit, const std::string& doneKey) {
    auto bytes = app_.sysFs().readFile(filePath(), 8u << 20);
    if (!bytes) { app_.toast(app_.t("common.failed") + ": " + bytes.error().message); return; }
    auto out = editCheatFile(file_, bytes.value(), edit);
    if (!out) { app_.toast(app_.t("common.failed") + ": " + out.error().message); return; }
    writeEdited(out.value(), doneKey);
}

ScreenResult CheatDetailScreen::openMenu() {
    const CheatFile* c = findCheat(app_, file_);
    if (!c) return ScreenResult::none();
    const bool canEdit = editable() && c->readable;
    const int row = list_.focus();
    const bool hasRow = row >= 0 && static_cast<size_t>(row) < indices_.size();
    const size_t idx = hasRow ? indices_[static_cast<size_t>(row)] : 0;
    const std::string cheatName = hasRow ? c->cheats[idx].name : "";
    std::vector<MenuAction> a;
    a.push_back({app_.t("edit.remove"), cheatName, [this, idx, cheatName]() {
                     return ScreenResult::replace(std::unique_ptr<Screen>(new ConfirmScreen(
                         app_, app_.t("edit.remove"), app_.t("edit.removeConfirm") + "\n" + cheatName + "\n\n" + app_.t("edit.backupNote"),
                         app_.t("edit.removeDo"), [this, idx]() {
                             CheatEdit e;
                             e.op = CheatEditOp::Remove;
                             e.index = idx;
                             applyEdit(e, "edit.removed");
                         }, true)));
                 }, canEdit && hasRow});
    a.push_back({app_.t("edit.rename"), cheatName, [this, idx, cheatName]() {
                     return ScreenResult::replace(std::unique_ptr<Screen>(new KeyboardScreen(
                         app_, app_.t("edit.rename"), cheatName, [this, idx](const std::string& name) {
                             if (name.empty()) return;
                             CheatEdit e;
                             e.op = CheatEditOp::Rename;
                             e.index = idx;
                             e.newName = name;
                             applyEdit(e, "edit.renamed");
                         })));
                 }, canEdit && hasRow});
    const bool hasBackup = app_.sysFs().exists(originalBackup());
    a.push_back({app_.t("edit.restore"), originalBackup(), [this]() {
                     auto bytes = app_.sysFs().readFile(originalBackup(), 8u << 20);
                     if (!bytes) { app_.toast(app_.t("common.failed") + ": " + bytes.error().message); return ScreenResult::pop(); }
                     if (writeEdited(bytes.value(), "edit.restored")) {
                         auto chk = app_.guard().check(originalBackup(), AccessKind::Write);
                         if (chk) app_.sysFs().remove(chk.value());
                     }
                     return ScreenResult::pop();
                 }, hasBackup});
    a.push_back({app_.t("edit.deleteFile"), filePath(), [this]() {
                     return ScreenResult::replace(std::unique_ptr<Screen>(new ConfirmScreen(
                         app_, app_.t("edit.deleteFile"), app_.t("edit.deleteConfirm") + "\n" + filePath() + "\n\n" + app_.t("edit.backupNote"),
                         app_.t("edit.deleteDo"), [this]() {
                             IFileSystem& fs = app_.sysFs();
                             const std::string keep = app_.dataRoot() + "/backups/replaced-cheats/" + timestampNow() + "_" + file_;
                             Status b = guardedCopy(fs, app_.guard(), filePath(), keep);
                             auto chk = app_.guard().check(filePath(), AccessKind::Write);
                             Status d = !b ? b : chk ? fs.remove(chk.value()) : Status::fail(chk.error().code, chk.error().message);
                             if (!d) { app_.toast(app_.t("common.failed") + ": " + d.error().message); return; }
                             app_.log().info("cheat file deleted (backup " + keep + "): " + filePath());
                             app_.ensureScanned(true);
                             app_.toast(app_.t("edit.deleted"));
                         }, true)));
                 }, true});
    if (!canEdit) a.front().detail = app_.t("edit.notEditable");
    return ScreenResult::push(std::unique_ptr<Screen>(new ActionMenuScreen(app_, file_, std::move(a))));
}

std::vector<std::pair<std::string, std::string>> CheatDetailScreen::hints() const {
    return {{"\xE2\x96\xA1", "hint.category"}, {"\xE2\x96\xB3", "hint.favorite"}, {"OPT", "hint.edit"}};
}

void CheatDetailScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    const CheatFile* c = findCheat(app_, file_);
    if (!c) { drawPageHeader(ctx, app_, file_, app_.t("cheats.gone")); return; }
    const GameInfo* g = app_.games().find(c->id.titleId);
    const std::string title = !c->gameName.empty() ? c->gameName : (g ? g->name : c->id.titleId);
    const std::string sub = c->fileName + " \xC2\xB7 " +
                            (category_ < 0 ? app_.t("cat.all") : app_.t(std::string("cat.") + toString(static_cast<CheatCategory>(category_))));
    const float y = drawPageHeader(ctx, app_, ellipsize(r, f.h2, title, kScreenW - 2 * kMarginX), sub);
    const std::string empty = c->readable ? app_.t("cheats.noneInCategory") : app_.t("cheats.mc4Note");
    list_.draw(r, t, f, {kMarginX, y, 1150, 1000 - y}, ctx.time, empty);

    const Rect p{kMarginX + 1180, y, kScreenW - 2 * kMarginX - 1180, 1000 - y};
    drawPanel(r, t, p, t.cornerRadius);
    const Compatibility comp = compatibilityFor(*c, app_.games());
    ListItem tmp;
    compatBadge(app_, t, comp, tmp);
    drawPill(r, f, tmp.badge, p.x + 28, p.y + 28, withAlpha(tmp.badgeColor, 0.22f), tmp.badgeColor);
    float yy = drawInfoRows(r, t, f, p.x + 28, p.y + 84, p.w - 56,
                            {{"Cheats", std::to_string(c->cheats.size())},
                             {app_.t("cheats.fileVersion"), c->id.version.toString()},
                             {app_.t("cheats.installed"), g ? (g->version.empty() ? app_.t("common.unknown") : g->version) : app_.t("common.no")},
                             {"Process", c->process.empty() ? "-" : c->process}}) + 16;
    for (const auto& line : wrapText(r, f.small, app_.t("cheats.howTo"), p.w - 56)) {
        if (yy > p.y + p.h - 30) break;
        r.drawText(f.small, line, p.x + 28, yy, t.textMuted);
        yy += 28;
    }
}

// =================================================================== Cheat import (USB)

CheatImportScreen::CheatImportScreen(App& app) : Screen(app) {}

void CheatImportScreen::onEnter() {
    cands_.clear();
    list_.items.clear();
    usb_ = firstUsbRoot(app_.sysFs());
    if (usb_.empty()) return;
    std::vector<std::string> dirs = {usb_, usb_ + "/GoldHEN/cheats/json", usb_ + "/GoldHEN/cheats/shn",
                                     usb_ + "/GoldHEN/cheats/mc4", usb_ + "/OSTLERTEKKERS_HEN/import"};
    for (const auto& d : dirs) {
        auto entries = app_.sysFs().list(d);
        if (!entries) continue;
        for (const auto& e : entries.value()) {
            if (e.isDir) continue;
            auto id = parseCheatFileName(e.name);
            if (!id) continue;
            Candidate c{joinPath(d, e.name), e.name, "", false};
            const std::string fmt = toString(id.value().format);
            if (fmt != "mc4") {
                auto bytes = app_.sysFs().readFile(c.path, 4u << 20);
                if (!bytes) c.problem = bytes.error().message;
                else {
                    auto parsed = parseCheatFile(e.name, bytes.value());
                    if (!parsed) c.problem = parsed.error().message;
                    else if (parsed.value().cheats.empty()) c.problem = app_.t("import.noCheats");
                }
            }
            c.replaces = app_.sysFs().exists(app_.cheatRoot() + "/" + fmt + "/" + e.name);
            cands_.push_back(c);
            ListItem it;
            it.title = e.name;
            it.subtitle = d;
            it.badge = !c.problem.empty() ? app_.t("import.invalid") : c.replaces ? app_.t("import.replaces") : app_.t("import.new");
            it.badgeColor = !c.problem.empty() ? app_.theme().error : c.replaces ? app_.theme().warn : app_.theme().ok;
            it.dim = !c.problem.empty();
            list_.items.push_back(it);
        }
    }
}

ScreenResult CheatImportScreen::handle(const InputEvent& ev) {
    if (ev.action == Action::Back) return ScreenResult::pop();
    if (list_.handle(ev.action)) return ScreenResult::none();
    if (ev.action == Action::Confirm && !cands_.empty()) {
        const Candidate c = cands_[static_cast<size_t>(list_.focus())];
        if (!c.problem.empty()) { app_.toast(c.problem); return ScreenResult::none(); }
        auto id = parseCheatFileName(c.name);
        const std::string target = app_.cheatRoot() + "/" + toString(id.value().format) + "/" + c.name;
        const std::string text = app_.t(c.replaces ? "import.confirmReplace" : "import.confirm") + "\n" + target;
        return ScreenResult::push(std::unique_ptr<Screen>(new ConfirmScreen(
            app_, app_.t("cheats.import"), text, app_.t("import.do"), [this, c, target]() {
                IFileSystem& fs = app_.sysFs();
                if (c.replaces) {   // keep the old file
                    const std::string keep = app_.dataRoot() + "/backups/replaced-cheats/" + timestampNow() + "_" + c.name;
                    Status b = guardedCopy(fs, app_.guard(), target, keep);
                    if (!b) { app_.toast(app_.t("common.failed") + ": " + b.error().message); return; }
                }
                Status st = guardedCopy(fs, app_.guard(), c.path, target);
                if (!st) {
                    app_.log().error("cheat import: " + st.error().message);
                    app_.toast(app_.t("common.failed") + ": " + st.error().message);
                    return;
                }
                app_.log().info("cheat imported: " + target);
                app_.ensureScanned(true);
                app_.toast(app_.t("import.done"));
            })));
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> CheatImportScreen::hints() const { return {{"@confirm", "import.do"}}; }

void CheatImportScreen::draw(const UiContext& ctx) {
    const float y = drawPageHeader(ctx, app_, app_.t("cheats.import"),
                                   usb_.empty() ? app_.t("common.noUsb") : app_.t("import.sub") + " " + usb_);
    list_.draw(ctx.renderer, ctx.theme, ctx.fonts, {kMarginX, y, kScreenW - 2 * kMarginX, 1000 - y}, ctx.time,
               app_.t(usb_.empty() ? "import.insertUsb" : "import.nothing"));
}

}  // namespace henu
