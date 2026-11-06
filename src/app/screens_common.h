// PS4 HEN ULTIMATE - app/screens_common.h
// Shared pieces for module screens: page header, confirm dialog, action
// menu (OPTIONS) and an on-screen keyboard for searching.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ui/list_view.h"
#include "ui/screen.h"

namespace henu {

class App;

// Title area used by all module pages; returns y where content starts.
float drawPageHeader(const UiContext& ctx, App& app, const std::string& title, const std::string& subtitle);

// "Really delete?" style dialog. onYes runs on confirm, then the dialog closes.
class ConfirmScreen : public Screen {
public:
    ConfirmScreen(App& app, std::string title, std::string text, std::string yesLabel, std::function<void()> onYes,
                  bool danger = false);
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    std::string title_, text_, yes_;
    std::function<void()> onYes_;
    bool danger_;
    int choice_ = 1;   // 0 = yes, 1 = cancel (safe default)
};

struct MenuAction {
    std::string label;
    std::string detail;
    std::function<ScreenResult()> run;   // result replaces the menu (Pop, Push, ...)
    bool enabled = true;
};

class ActionMenuScreen : public Screen {
public:
    ActionMenuScreen(App& app, std::string title, std::vector<MenuAction> actions);
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    std::string title_;
    std::vector<MenuAction> actions_;
    ListView list_;
};

// Grid keyboard (A-Z, 0-9, space, delete, OK). Calls onDone with the text.
class KeyboardScreen : public Screen {
public:
    KeyboardScreen(App& app, std::string title, std::string initial, std::function<void(const std::string&)> onDone);
    ScreenResult handle(const InputEvent& ev) override;
    void draw(const UiContext& ctx) override;
    std::vector<std::pair<std::string, std::string>> hints() const override;

private:
    std::string title_, text_;
    std::function<void(const std::string&)> onDone_;
    int row_ = 0, col_ = 0;
};

}  // namespace henu
