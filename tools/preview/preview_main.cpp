// PS4 HEN ULTIMATE - tools/preview/preview_main.cpp
// Runs the real App with the same software renderer the PS4 uses and writes
// one PNG per scenario step (exact pixels, no SDL needed).
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "app/app.h"
#include "platform/posix_fs.h"
#include "gfx/soft_renderer.h"

using namespace henu;

namespace {
class NoInput : public IInput {
public:
    PadState poll() override { PadState p; p.connected = true; return p; }
};
class NoSystem : public ISystemInfo {
public:
    SystemSnapshot snapshot() override { return {}; }
};
}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: preview <bundleRoot> <dataRoot> <outDir>\n");
        return 2;
    }
    const std::string bundle = argv[1], data = argv[2], out = argv[3];
    PosixFileSystem fs;
    Platform p;
    p.fs.reset(new PosixFileSystem);
    auto* rec = new SoftwareRenderer(*p.fs);
    p.renderer.reset(rec);
    p.input.reset(new NoInput);
    p.system.reset(new NoSystem);
    p.bundleRoot = bundle;
    p.dataRoot = data;

    App app(p, AppOptions{true});
    Status st = app.init();
    if (!st) { std::fprintf(stderr, "init: %s\n", st.error().message.c_str()); return 1; }

    int shot = 0;
    auto settle = [&](int frames) { for (int i = 0; i < frames; ++i) app.frame(1.0f / 60.0f); };
    auto capture = [&](const std::string& name) {
        settle(45);
        char file[512];
        std::snprintf(file, sizeof(file), "%s/%02d_%s.png", out.c_str(), ++shot, name.c_str());
        if (!fs.writeFile(file, encodePng(rec->canvas()))) { std::fprintf(stderr, "write failed: %s\n", file); return; }
        std::printf("%s  (section=%s, %zu texts)\n", file, app.currentSection().c_str(), rec->texts().size());
    };
    auto press = [&](Action a, int times = 1) {
        for (int i = 0; i < times; ++i) { app.inject({a, false}); settle(2); }
    };

    // Rough timing of one full frame with this renderer (host CPU).
    {
        std::clock_t c0 = std::clock();
        int drawn = 0;
        for (int i = 0; i < 60; ++i) {
            app.inject({i % 2 ? Action::Left : Action::Right, false});   // force a redraw every frame
            app.frame(1.0f / 60.0f);
            drawn += app.lastFrameDrawn() ? 1 : 0;
        }
        const double ms = 1000.0 * (std::clock() - c0) / CLOCKS_PER_SEC / 60.0;
        std::printf("full redraw: %.1f ms per frame (%d/60 drawn)\n", ms, drawn);
        c0 = std::clock();
        for (int i = 0; i < 60; ++i) app.frame(1.0f / 60.0f);
        settle(60);
        c0 = std::clock();
        drawn = 0;
        for (int i = 0; i < 60; ++i) { app.frame(1.0f / 60.0f); drawn += app.lastFrameDrawn() ? 1 : 0; }
        std::printf("idle: %.2f ms per frame (%d/60 drawn)\n",
                    1000.0 * (std::clock() - c0) / CLOCKS_PER_SEC / 60.0, drawn);
    }
    capture("home_de");
    press(Action::Right, 2);
    press(Action::Down);
    capture("home_focus_moved");
    press(Action::Up, 2);
    capture("home_hero_focus");
    press(Action::Down);
    press(Action::Left, 6);
    press(Action::Right);   // -> cheats tile
    press(Action::Confirm);
    capture("cheats_page");
    press(Action::Back);
    press(Action::PrevSection, 4);   // home -> diag -> update -> backup -> settings
    capture("settings_de");
    press(Action::Right);            // language -> en
    capture("settings_en");
    press(Action::Down);
    press(Action::Right);            // theme -> next
    capture("settings_theme_changed");
    press(Action::Back);
    capture("home_en_new_theme");
    press(Action::NextSection, 4);   // home -> goldhen
    capture("goldhen_page");
    return 0;
}
