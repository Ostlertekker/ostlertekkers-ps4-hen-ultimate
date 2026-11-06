// OSTLERTEKKERS HEN - app/screens.h
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "net/online.h"
#include "cheats/cheat_edit.h"
#include "ui/list_view.h"
#include "ui/screen.h"

namespace henu {

// Start page: hero, status panel, one tile per module.
class HomeScreen : public Screen {
public:
    explicit HomeScreen(App& app);
    ScreenResult handle(const InputEvent& ev) override;
    void update(const UiContext& ctx) override;
    void draw(const UiContext& ctx) override;
    bool animating() const override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

    int focusIndex() const { return focus_; }
    const std::vector<Rect>& focusRects() const { return rects_; }

private:
    bool statusRead_ = false;
    bool goldhenFolder_ = false;
    std::string firmware_;
    void layout();
    std::vector<std::string> tileIds_;   // module ids; index 0 = hero button
    std::vector<Rect> rects_;
    std::vector<float> scale_;
    std::vector<float> target_;
    int focus_ = 1;
};

// Generic page for a module: title, status, reason and planned features.
// Used for every module that has no own implementation yet.
class ModuleScreen : public Screen {
public:
    ModuleScreen(App& app, std::string moduleId);
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    std::string id_;
};

// Working settings page (language, theme, animations, confirm button, reset).
class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(App& app);
    ScreenResult handle(const InputEvent& ev) override;
    void update(const UiContext& ctx) override;
    void draw(const UiContext& ctx) override;
    bool animating() const override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

    int focusIndex() const { return focus_; }

private:
    enum Row { Language, ThemeRow, Animations, ConfirmButton, BootAnim, BootSound, LaunchAnim, Volume, BootPreview,
               ImportWav, Reset, RowCount };
    void importWav();
    void change(int delta);
    int focus_ = 0;
    bool resetArmed_ = false;
    float highlightY_ = -1;
    float highlightTarget_ = -1;
};

// ---------------------------------------------------------------- Games / Homebrew
class GamesScreen : public Screen {
public:
    GamesScreen(App& app, bool homebrew);
    void onEnter() override;
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    void rebuild();
    bool homebrew_;
    int filter_ = 0;          // 0 all, 1 favourites, 2 with cheats
    bool sortById_ = false;
    std::string query_;
    std::vector<std::string> ids_;   // visible title ids (grid order)
    int focus_ = 0, top_ = 0;
};

// One installed title: details and everything that can be done for it.
class GameDetailScreen : public Screen {
public:
    GameDetailScreen(App& app, std::string titleId);
    void onEnter() override;
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    std::string id_;
    ListView actions_;
};

// GoldHEN cheat repository (online): browse + download into /data/GoldHEN/cheats.
class OnlineCheatsScreen : public Screen {
public:
    OnlineCheatsScreen(App& app, std::string titleFilter);
    void onEnter() override;
    ScreenResult handle(const InputEvent& ev) override;
    void update(const UiContext& ctx) override;
    void draw(const UiContext& ctx) override;
    bool animating() const override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    void startIndex();
    void rebuild();
    void install(const OnlineCheatEntry& e, const std::string& body);
    std::string filter_;
    bool onlyInstalled_ = true;
    std::unique_ptr<NetJob> job_;
    enum class Phase { Idle, Index, File } phase_ = Phase::Idle;
    std::vector<OnlineCheatEntry> entries_;
    std::vector<size_t> visible_;
    size_t pending_ = 0;
    ListView list_;
    std::string error_;
};

// ---------------------------------------------------------------- Cheat Center
class CheatsScreen : public Screen {
public:
    explicit CheatsScreen(App& app);
    void onEnter() override;
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    void rebuild();
    int filter_ = 0;          // 0 all, 1 installed+matching, 2 favourites, 3 recent
    std::string query_;
    ListView list_;
};

class CheatDetailScreen : public Screen {
public:
    CheatDetailScreen(App& app, std::string fileName);
    void onEnter() override;
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    void rebuild();
    ScreenResult openMenu();
    bool editable() const;
    std::string filePath() const;
    std::string originalBackup() const;
    // Backs up the untouched original once, then writes content (PathGuard).
    bool writeEdited(const std::string& content, const std::string& doneKey);
    void applyEdit(const CheatEdit& edit, const std::string& doneKey);
    std::string file_;
    int category_ = -1;       // -1 = all
    std::vector<size_t> indices_;   // list row -> index in CheatFile::cheats
    ListView list_;
};

class CheatImportScreen : public Screen {
public:
    explicit CheatImportScreen(App& app);
    void onEnter() override;
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    struct Candidate { std::string path, name, problem; bool replaces = false; };
    std::vector<Candidate> cands_;
    std::string usb_;
    ListView list_;
};

// ---------------------------------------------------------------- Themes
class ThemesScreen : public Screen {
public:
    explicit ThemesScreen(App& app);
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    void applyTheme(const std::string& id);
    int focus_ = 0, top_ = 0;
};

// ---------------------------------------------------------------- Info pages
class GoldhenScreen : public Screen {
public:
    explicit GoldhenScreen(App& app) : Screen(app) {}
    void onEnter() override;
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;

private:
    ListView plugins_;
};

class SystemScreen : public Screen {
public:
    explicit SystemScreen(App& app) : Screen(app) {}
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
};

class NetworkScreen : public Screen {
public:
    explicit NetworkScreen(App& app) : Screen(app) {}
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
};

class UpdateScreen : public Screen {
public:
    explicit UpdateScreen(App& app);
    ScreenResult handle(const InputEvent& ev) override;
    void update(const UiContext& ctx) override;
    void draw(const UiContext& ctx) override;
    bool animating() const override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    void check();
    void download();
    enum class State { Idle, Checking, Available, UpToDate, Downloading, Ready, Error } state_ = State::Idle;
    std::unique_ptr<NetJob> job_;
    ReleaseInfo rel_;
    std::string message_, savedPath_;
};

// ---------------------------------------------------------------- Files
class FilesScreen : public Screen {
public:
    explicit FilesScreen(App& app) : Screen(app) {}
    void onEnter() override;
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    void open(const std::string& dir);
    std::string dir_;          // "" = list of roots
    ListView list_;
};

// ---------------------------------------------------------------- Backup
class BackupScreen : public Screen {
public:
    explicit BackupScreen(App& app) : Screen(app) {}
    void onEnter() override;
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    std::string target() ;
    void rebuild();
    ListView list_;
    std::vector<std::string> dirs_;   // backup dir per list row (after the 2 action rows)
};

// ---------------------------------------------------------------- Diagnostics
class DiagScreen : public Screen {
public:
    explicit DiagScreen(App& app) : Screen(app) {}
    void onEnter() override;
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    ListView list_;
};

}  // namespace henu
