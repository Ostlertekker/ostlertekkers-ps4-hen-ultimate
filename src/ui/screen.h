// PS4 HEN ULTIMATE - ui/screen.h
// Each module provides one or more Screens. The app keeps a stack per
// section: Confirm pushes a sub-screen, Back pops, L1/R1 switch sections.
#pragma once

#include <memory>
#include <string>

#include "platform/platform.h"
#include "ui/draw.h"
#include "ui/input.h"

namespace henu {

class App;

struct UiContext {
    IRenderer& renderer;
    const Theme& theme;
    const Fonts& fonts;
    double time;      // seconds, monotonic
    float dt;         // seconds since last frame
    bool animate;     // false when animations are off
    float animSpeed;  // theme speed * settings factor
};

class Screen;

struct ScreenResult {
    enum class Kind { None, Push, Pop, GoToSection, Quit } kind = Kind::None;
    std::unique_ptr<Screen> next;   // Push
    std::string sectionId;          // GoToSection

    static ScreenResult none() { return {}; }
    static ScreenResult pop() { ScreenResult r; r.kind = Kind::Pop; return r; }
    static ScreenResult push(std::unique_ptr<Screen> s) {
        ScreenResult r; r.kind = Kind::Push; r.next = std::move(s); return r;
    }
    static ScreenResult goTo(const std::string& id) {
        ScreenResult r; r.kind = Kind::GoToSection; r.sectionId = id; return r;
    }
};

class Screen {
public:
    explicit Screen(App& app) : app_(app) {}
    virtual ~Screen() = default;
    virtual void onEnter() {}
    virtual ScreenResult handle(const InputEvent& ev) = 0;
    virtual void update(const UiContext& ctx) { (void)ctx; }
    virtual void draw(const UiContext& ctx) = 0;
    // True while an animation is running (the app then redraws every frame).
    virtual bool animating() const { return false; }
    // Hints shown in the bottom bar, e.g. {"X", "hint.open"}.
    virtual std::vector<std::pair<std::string, std::string>> hints() const { return {}; }

protected:
    App& app_;
};

}  // namespace henu
