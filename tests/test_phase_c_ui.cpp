// OSTLERTEKKERS HEN - tests for input, focus navigation and the App.
// Uses the real assets from the project root (run tests from there).
#include <sys/stat.h>

#include <cstdio>
#include <string>

#include "app/app.h"
#include "check.h"
#include "platform/posix_fs.h"
#include "themes/theme_catalog.h"
#include "gfx/soft_renderer.h"
#include "ui/focus.h"
#include "ui/input.h"

using namespace henu;

namespace {
PadState pad(uint32_t buttons, float lx = 0, float ly = 0) {
    PadState p;
    p.connected = true;
    p.buttons = buttons;
    p.lx = lx;
    p.ly = ly;
    return p;
}
uint32_t B(Button b) { return static_cast<uint32_t>(b); }
int count(const std::vector<InputEvent>& ev, Action a) {
    int n = 0;
    for (const auto& e : ev) if (e.action == a) ++n;
    return n;
}

class NoInput : public IInput {
public:
    PadState poll() override { PadState p; p.connected = true; return p; }
};
class NoSystem : public ISystemInfo {
public:
    SystemSnapshot snapshot() override { return {}; }
};

struct TestApp {
    Platform p;
    SoftwareRenderer* rec = nullptr;
    std::unique_ptr<App> app;
    explicit TestApp(const std::string& dataRoot) {
        p.fs.reset(new PosixFileSystem);
        rec = new SoftwareRenderer(*p.fs);
        p.renderer.reset(rec);
        p.input.reset(new NoInput);
        p.system.reset(new NoSystem);
        p.bundleRoot = ".";
        p.dataRoot = dataRoot;
        app.reset(new App(p, AppOptions{true}));
    }
    void press(Action a) { app->inject({a, false}); app->frame(1.0f / 60); }
};

std::string freshDataDir(const char* name) {
    ::mkdir("build", 0777);
    ::mkdir("build/test-data", 0777);
    std::string d = std::string("build/test-data/") + name;
    PosixFileSystem fs;
    for (const char* f : {"/settings.json", "/settings.json.bak", "/settings.json.tmp", "/logs/app.log"})
        if (fs.exists(d + f)) fs.remove(d + f);
    return d;
}
}  // namespace

static void testInputMapper() {
    InputMapper m;
    InputConfig c;
    c.repeatDelay = 0.3f;
    c.repeatRate = 0.1f;
    m.configure(c);

    auto e = m.update(pad(B(Button::Right)), 0.016f);
    CHECK(count(e, Action::Right) == 1);
    e = m.update(pad(B(Button::Right)), 0.1f);
    CHECK(e.empty());                                   // held, before repeat delay
    e = m.update(pad(B(Button::Right)), 0.25f);         // t=0.35 -> 1 repeat
    CHECK(count(e, Action::Right) == 1 && e[0].repeat);
    e = m.update(pad(B(Button::Right)), 0.1f);          // t=0.45 -> 1 repeat
    CHECK(count(e, Action::Right) == 1);
    e = m.update(pad(B(Button::Right)), 5.0f);          // stall: capped catch-up
    CHECK(count(e, Action::Right) <= 4);
    e = m.update(pad(0), 0.016f);
    CHECK(e.empty());

    // buttons: edge only
    e = m.update(pad(B(Button::Cross)), 0.016f);
    CHECK(count(e, Action::Confirm) == 1);
    e = m.update(pad(B(Button::Cross)), 0.5f);
    CHECK(e.empty());
    m.update(pad(0), 0.016f);
    e = m.update(pad(B(Button::Circle) | B(Button::L1)), 0.016f);
    CHECK(count(e, Action::Back) == 1 && count(e, Action::PrevSection) == 1);
    m.update(pad(0), 0.016f);

    // swapped confirm (JP layout)
    c.circleConfirms = true;
    m.configure(c);
    e = m.update(pad(B(Button::Circle)), 0.016f);
    CHECK(count(e, Action::Confirm) == 1 && count(e, Action::Back) == 0);
    m.update(pad(0), 0.016f);
    e = m.update(pad(B(Button::Cross)), 0.016f);
    CHECK(count(e, Action::Back) == 1);
    m.update(pad(0), 0.016f);

    // stick with deadzone; stick down = positive y
    e = m.update(pad(0, 0.1f, 0.0f), 0.016f);
    CHECK(e.empty());
    e = m.update(pad(0, 0.0f, 0.9f), 0.016f);
    CHECK(count(e, Action::Down) == 1);
    m.update(pad(0), 0.016f);
    e = m.update(pad(0, -0.8f, 0.3f), 0.016f);
    CHECK(count(e, Action::Left) == 1 && count(e, Action::Down) == 0);
    m.update(pad(0), 0.016f);
    // D-pad wins over stick; only one direction per frame
    e = m.update(pad(B(Button::Up) | B(Button::Left), 0.9f, 0), 0.016f);
    CHECK(e.size() == 1);
    m.update(pad(0), 0.016f);
    // disconnected pad produces nothing
    PadState off = pad(B(Button::Cross));
    off.connected = false;
    CHECK(m.update(off, 0.016f).empty());
}

static void testFocus() {
    // 3x2 grid + one wide item on top
    std::vector<Rect> r = {
        {0, 0, 620, 100},                                          // 0 wide top
        {0, 200, 200, 100}, {210, 200, 200, 100}, {420, 200, 200, 100},   // 1 2 3
        {0, 310, 200, 100}, {210, 310, 200, 100}, {420, 310, 200, 100},   // 4 5 6
    };
    CHECK(moveFocus(r, 1, Action::Right) == 2);
    CHECK(moveFocus(r, 2, Action::Right) == 3);
    CHECK(moveFocus(r, 3, Action::Right) == 3);   // edge: stay
    CHECK(moveFocus(r, 2, Action::Down) == 5);
    CHECK(moveFocus(r, 5, Action::Up) == 2);
    CHECK(moveFocus(r, 3, Action::Up) == 0);
    CHECK(moveFocus(r, 0, Action::Down) == 2);    // centre-most below
    CHECK(moveFocus(r, 4, Action::Left) == 4);
    CHECK(moveFocus(r, 6, Action::Left) == 5);
    CHECK(moveFocus({}, 0, Action::Left) == -1);
    CHECK(moveFocus(r, 99, Action::Left) == 0);
}

static void testBundledThemes() {
    PosixFileSystem fs;
    ThemeCatalog cat;
    cat.scan(fs, "assets/themes", "");
    CHECK(cat.themes().size() >= 20);
    CHECK(cat.errors().empty());
    for (const auto& e : cat.errors()) std::printf("  theme error %s: %s\n", e.file.c_str(), e.message.c_str());
    CHECK(cat.indexOf("midnight") >= 0 && cat.getOrFallback("nope").id == cat.themes().front().id);
    CHECK(cat.neighbour(cat.themes().back().id, 1) == cat.themes().front().id);   // wraps
    CHECK(cat.neighbour(cat.themes().front().id, -1) == cat.themes().back().id);
}

static void testAppNavigation() {
    TestApp t(freshDataDir("nav"));
    CHECK(t.app->init().isOk());
    CHECK(t.app->modules().size() == 13);
    CHECK(t.app->currentSection() == "home");
    CHECK(t.app->frame(1.0f / 60));

    // R1 through all sections wraps back to home
    for (int i = 0; i < 13; ++i) t.press(Action::NextSection);
    CHECK(t.app->currentSection() == "home");
    t.press(Action::PrevSection);
    CHECK(t.app->currentSection() == "diag");
    t.press(Action::Back);                       // Back on a section root -> home
    CHECK(t.app->currentSection() == "home");

    // Tile navigation: focus starts on first tile (games); Right -> cheats
    t.press(Action::Right);
    t.press(Action::Confirm);
    CHECK(t.app->currentSection() == "cheats");
    t.press(Action::Back);
    CHECK(t.app->currentSection() == "home");
    // Hero button opens the theme section
    t.press(Action::Up);
    t.press(Action::Confirm);
    CHECK(t.app->currentSection() == "themes");

    // Only sections confirmed on the console may claim "available".
    for (const auto& m : t.app->modules())
        CHECK(m.status != CapStatus::Available || m.id == "home" || m.id == "settings");
}

static void testTranslationsComplete() {
    // Every screen, both languages: no raw i18n key may reach the screen.
    TestApp t(freshDataDir("i18n"));
    CHECK(t.app->init().isOk());
    for (const char* lang : {"de", "en"}) {
        Settings s = t.app->settings();
        s.language = lang;
        CHECK(t.app->updateSettings(s));
        for (size_t i = 0; i < t.app->modules().size(); ++i) {
            t.app->frame(1.0f / 60);
            for (const auto& text : t.rec->texts()) {
                auto endsWith = [&](const char* e) {
                    const std::string x(e);
                    return text.size() >= x.size() && text.compare(text.size() - x.size(), x.size(), x) == 0;
                };
                const bool fileName = endsWith(".ini") || endsWith(".json") || endsWith(".log") || endsWith(".txt");
                bool rawKey = !fileName && text.find('.') != std::string::npos && text.find(' ') == std::string::npos &&
                              text.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/") == std::string::npos;
                if (rawKey) std::printf("  untranslated (%s, %s): %s\n", lang, t.app->currentSection().c_str(), text.c_str());
                CHECK(!rawKey);
            }
            t.press(Action::NextSection);
        }
    }
}

static void testSettingsPersist() {
    const std::string dir = freshDataDir("persist");
    std::string expectedTheme;
    {
        TestApp t(dir);
        CHECK(t.app->init().isOk());
        CHECK(t.app->settings().language == "de");
        // go to settings (L1 x4 from home), change language and theme
        for (int i = 0; i < 4; ++i) t.press(Action::PrevSection);
        CHECK(t.app->currentSection() == "settings");
        t.press(Action::Right);                  // language de -> en
        CHECK(t.app->settings().language == "en");
        t.press(Action::Down);
        expectedTheme = t.app->themes().neighbour("midnight", 1);
        t.press(Action::Right);                  // theme midnight -> next
        CHECK(t.app->settings().themeId == expectedTheme);
        CHECK(t.app->theme().id == expectedTheme);
        t.press(Action::Down);
        t.press(Action::Down);
        t.press(Action::Right);                  // confirm button cross -> circle
        CHECK(t.app->settings().confirmButton == "circle");
        t.press(Action::Down);
        t.press(Action::Right);                  // boot intro inferno -> era1
        CHECK(t.app->settings().bootAnimation == "era1");
        t.press(Action::Down);
        t.press(Action::Right);                  // sound builtin -> custom
        CHECK(t.app->settings().bootSound == "custom");
        t.press(Action::Down);
        t.press(Action::Left);                   // volume 70 -> 60
        CHECK(t.app->settings().volume == 60);
    }
    {
        TestApp t(dir);                           // "restart"
        CHECK(t.app->init().isOk());
        CHECK(t.app->settings().language == "en");
        CHECK(t.app->settings().themeId == expectedTheme);
        CHECK(t.app->settings().confirmButton == "circle");
        CHECK(t.app->settings().bootAnimation == "era1" && t.app->settings().volume == 60);
        // reset needs two confirmations
        for (int i = 0; i < 4; ++i) t.press(Action::PrevSection);
        t.press(Action::Up);                      // wraps to the last row: reset
        t.press(Action::Confirm);
        CHECK(t.app->settings().language == "en");   // armed only
        t.press(Action::Confirm);
        CHECK(t.app->settings().language == "de" && t.app->settings().themeId == "midnight");
    }
    PosixFileSystem fs;
    CHECK(fs.exists(dir + "/settings.json") && fs.exists(dir + "/settings.json.bak"));
    CHECK(fs.exists(dir + "/logs/app.log"));
}

static void testMissingAssets() {
    // Broken bundle: app must still start (fallback theme, minimal modules).
    TestApp t(freshDataDir("broken"));
    t.p.bundleRoot = "build/does-not-exist";
    CHECK(t.app->init().isOk());
    CHECK(t.app->modules().size() == 2);
    CHECK(t.app->theme().id == "builtin-fallback");
    CHECK(t.app->frame(1.0f / 60));
    CHECK(!t.app->log().recent().empty());
}

void runPhaseCUiTests() {
    testInputMapper();
    testFocus();
    testBundledThemes();
    testAppNavigation();
    testTranslationsComplete();
    testSettingsPersist();
    testMissingAssets();
}
