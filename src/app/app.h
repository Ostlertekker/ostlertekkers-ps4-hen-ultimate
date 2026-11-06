// PS4 HEN ULTIMATE - app/app.h
// Owns all services, the section list (from config/features.json), one
// screen stack per section and the frame loop.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app/favorites.h"
#include "cheats/cheat_db.h"
#include "core/capability.h"
#include "files/path_guard.h"
#include "games/game_library.h"
#include "core/log.h"
#include "i18n/i18n.h"
#include "platform/platform.h"
#include "settings/settings.h"
#include "themes/theme_catalog.h"
#include "ui/draw.h"
#include "ui/input.h"
#include "ui/screen.h"

namespace henu {

struct ModuleDef {
    std::string id;
    int order = 0;
    CapStatus status = CapStatus::NotImplemented;
    std::string note;
};

struct AppOptions {
    bool onConsole = false;   // true on PS4: implemented features count as "unverified" until tested
};

constexpr const char* kAppVersion = "0.4.0";

class App {
public:
    App(Platform& platform, AppOptions opts);
    ~App();

    // Loads config, languages, themes, settings and fonts. Problems are logged
    // and shown in the UI; only a missing renderer makes init fail.
    Status init();

    // One frame: poll input, handle events, update, draw. false = quit.
    bool frame(float dt);

    // Feeds an action directly (tests, preview tool).
    void inject(const InputEvent& ev);

    // ---- services for screens
    const Settings& settings() const { return settings_; }
    // Applies + saves; returns false and shows a toast on failure.
    bool updateSettings(const Settings& s);
    const I18n& i18n() const { return i18n_; }
    std::string t(const std::string& key) const { return i18n_.t(key); }
    const ThemeCatalog& themes() const { return themes_; }
    const Theme& theme() const { return *theme_; }
    const CapabilityRegistry& caps() const { return caps_; }
    const std::vector<ModuleDef>& modules() const { return modules_; }
    const ModuleDef* module(const std::string& id) const;
    const Fonts& fonts() const { return fonts_; }
    IRenderer& renderer() { return *platform_.renderer; }
    Logger& log() { return log_; }
    const AppOptions& options() const { return opts_; }
    const std::string& dataRoot() const { return platform_.dataRoot; }
    void toast(const std::string& text);

    // ---- console data (paths are console-absolute, e.g. "/user/app")
    IFileSystem& sysFs() { return sysFs_ ? *sysFs_ : *platform_.fs; }
    const PathGuard& guard() const { return *guard_; }
    GameLibrary& games() { return games_; }
    CheatDatabase& cheats() { return cheats_; }
    Favorites& favorites() { return favs_; }
    ThemeCatalog& themeCatalog() { return themes_; }
    SystemSnapshot systemInfo();
    std::string cheatRoot() const { return "/data/GoldHEN/cheats"; }
    // Scans installed titles and cheat files (once, or again with force).
    void ensureScanned(bool force = false);
    void rescanThemes();
    void reloadUserData();   // after a restore: settings + favourites from disk
    // Hand-over between sections (e.g. "show cheats for CUSA00001").
    void setCheatQuery(const std::string& q) { cheatQuery_ = q; }
    std::string takeCheatQuery() { std::string q = cheatQuery_; cheatQuery_.clear(); return q; }
    bool goToSectionById(const std::string& id);

    // True if the last frame() actually rendered (false = idle, nothing changed).
    bool lastFrameDrawn() const { return drawn_; }

    // ---- navigation state (for tests)
    const std::string& currentSection() const;
    size_t stackDepth() const;

private:
    void loadModules();
    void loadLanguages();
    void loadFonts();
    void applyTheme();
    void applyInput();
    void handle(const InputEvent& ev);
    void goToSection(size_t index);
    Screen& top();
    std::unique_ptr<Screen> makeRoot(const std::string& moduleId);
    void drawChrome(const UiContext& ctx);

    Platform& platform_;
    AppOptions opts_;
    std::unique_ptr<IFileSystem> sysFs_;
    std::unique_ptr<PathGuard> guard_;
    GameLibrary games_;
    CheatDatabase cheats_;
    Favorites favs_;
    std::string cheatQuery_;
    Logger log_;
    I18n i18n_;
    ThemeCatalog themes_;
    const Theme* theme_ = nullptr;
    CapabilityRegistry caps_;
    std::unique_ptr<SettingsStore> store_;
    Settings settings_;
    Fonts fonts_;
    InputMapper input_;
    std::vector<ModuleDef> modules_;
    std::vector<std::vector<std::unique_ptr<Screen>>> stacks_;
    size_t section_ = 0;
    double time_ = 0;
    std::string toastText_;
    double toastUntil_ = 0;
    double nextFlush_ = 5;
    bool quit_ = false;
    bool dirty_ = true;
    bool drawn_ = false;
    bool toastShown_ = false;
};

}  // namespace henu
