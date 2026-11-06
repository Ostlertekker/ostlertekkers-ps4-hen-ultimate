// PS4 HEN ULTIMATE - app/screens.h
#pragma once

#include <memory>
#include <string>
#include <vector>

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
    enum Row { Language, ThemeRow, Animations, ConfirmButton, Reset, RowCount };
    void change(int delta);
    int focus_ = 0;
    bool resetArmed_ = false;
    float highlightY_ = -1;
    float highlightTarget_ = -1;
};

}  // namespace henu
