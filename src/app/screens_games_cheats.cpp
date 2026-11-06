// PS4 HEN ULTIMATE - app/screens_games_cheats.cpp
// Games, Homebrew and the Cheat Center.
#include <algorithm>
#include <cctype>

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

GamesScreen::GamesScreen(App& app, bool homebrew) : Screen(app), homebrew_(homebrew) {}

void GamesScreen::onEnter() {
    app_.ensureScanned();
    rebuild();
}

void GamesScreen::rebuild() {
    const std::string key = list_.current() ? list_.current()->key : "";
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
    list_.items.clear();
    int restore = 0;
    for (const GameInfo* g : v) {
        ListItem it;
        it.key = g->titleId;
        it.title = g->name;
        it.subtitle = g->titleId + " \xC2\xB7 " + (g->version.empty() ? app_.t("common.unknown") : "v" + g->version) +
                      " \xC2\xB7 " + toString(g->region);
        it.star = app_.favorites().isGame(g->titleId);
        if (!homebrew_) {
            const size_t n = app_.cheats().forTitle(g->titleId).size();
            if (n) {
                it.badge = std::to_string(n) + " " + app_.t("games.cheatFiles");
                it.badgeColor = app_.theme().accent2;
            }
        }
        if (it.key == key) restore = static_cast<int>(list_.items.size());
        list_.items.push_back(it);
    }
    list_.setFocus(restore);
}

ScreenResult GamesScreen::handle(const InputEvent& ev) {
    if (list_.handle(ev.action)) return ScreenResult::none();
    const ListItem* cur = list_.current();
    switch (ev.action) {
        case Action::Context:
            if (cur) { app_.favorites().toggleGame(cur->key); rebuild(); }
            break;
        case Action::Search:
            filter_ = (filter_ + 1) % (homebrew_ ? 2 : 3);
            rebuild();
            app_.toast(app_.t("games.filter." + std::to_string(filter_)));
            break;
        case Action::Confirm:
            if (cur && !homebrew_) {
                app_.setCheatQuery(cur->key);
                return ScreenResult::goTo("cheats");
            }
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
            a.push_back({app_.t("menu.rescan"), "", [this]() { app_.ensureScanned(true); rebuild(); return ScreenResult::pop(); }});
            return ScreenResult::push(std::unique_ptr<Screen>(new ActionMenuScreen(app_, app_.t("menu.title"), a)));
        }
        default: break;
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> GamesScreen::hints() const {
    std::vector<std::pair<std::string, std::string>> h;
    if (!homebrew_) h.push_back({"@confirm", "games.showCheats"});
    h.push_back({"\xE2\x96\xB3", "hint.favorite"});
    h.push_back({"\xE2\x96\xA1", "hint.filter"});
    h.push_back({"OPT", "hint.menu"});
    return h;
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
    list_.draw(r, t, f, {kMarginX, y, 1150, 1000 - y}, ctx.time, app_.t(total ? "games.noMatch" : "games.none"));

    const Rect p{kMarginX + 1180, y, kScreenW - 2 * kMarginX - 1180, 1000 - y};
    drawPanel(r, t, p, t.cornerRadius);
    const ListItem* cur = list_.current();
    const GameInfo* g = cur ? app_.games().find(cur->key) : nullptr;
    if (!g) {
        float yy = p.y + 30;
        for (const auto& line : wrapText(r, f.small, app_.t("games.sourceNote"), p.w - 56)) {
            r.drawText(f.small, line, p.x + 28, yy, t.textMuted);
            yy += 30;
        }
        return;
    }
    r.drawText(f.bodyBold, ellipsize(r, f.bodyBold, g->name, p.w - 56), p.x + 28, p.y + 28, t.text);
    std::vector<std::pair<std::string, std::string>> rows = {
        {"Title ID", g->titleId},
        {app_.t("games.version"), g->version.empty() ? app_.t("common.unknown") : g->version},
        {app_.t("games.region"), toString(g->region)},
        {"Content ID", g->contentId.empty() ? app_.t("common.unknown") : g->contentId},
        {app_.t("games.category"), g->category.empty() ? "-" : g->category},
    };
    if (!homebrew_) {
        size_t ok = 0;
        auto files = app_.cheats().forTitle(g->titleId);
        for (const auto* c : files) if (compatibilityFor(*c, app_.games()) == Compatibility::Compatible) ++ok;
        rows.push_back({app_.t("games.cheatFilesLong"), std::to_string(files.size()) + " (" + std::to_string(ok) + " " + app_.t("games.matching") + ")"});
    }
    float yy = drawInfoRows(r, t, f, p.x + 28, p.y + 84, p.w - 56, rows) + 20;
    for (const auto& line : wrapText(r, f.small, app_.t("games.readOnly"), p.w - 56)) {
        r.drawText(f.small, line, p.x + 28, yy, t.textMuted);
        yy += 28;
    }
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
            MenuAction exp{app_.t("cheats.export"), usb.empty() ? app_.t("common.noUsb") : usb + "/PS4HENULTIMATE/export/cheats", nullptr,
                           sel != nullptr && !usb.empty()};
            if (sel) {
                const std::string file = sel->key;
                exp.run = [this, file, usb]() {
                    const CheatFile* f = findCheat(app_, file);
                    if (!f) return ScreenResult::pop();
                    Status st = guardedCopy(app_.sysFs(), app_.guard(), f->path, usb + "/PS4HENULTIMATE/export/cheats/" + f->fileName);
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
    const CheatFile* c = findCheat(app_, file_);
    if (!c) return;
    for (const auto& e : c->cheats) {
        if (category_ >= 0 && static_cast<int>(e.category) != category_) continue;
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
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> CheatDetailScreen::hints() const {
    return {{"\xE2\x96\xA1", "hint.category"}, {"\xE2\x96\xB3", "hint.favorite"}};
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
                                     usb_ + "/GoldHEN/cheats/mc4", usb_ + "/PS4HENULTIMATE/import"};
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
