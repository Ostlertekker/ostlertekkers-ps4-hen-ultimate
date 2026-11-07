// OSTLERTEKKERS HEN - tests for input, focus navigation and the App.
// Uses the real assets from the project root (run tests from there).
#include <sys/stat.h>

#include <cstdio>
#include <string>

#include <atomic>

#include "app/app.h"
#include <cstdlib>
#include <unistd.h>
#include "files/fs_util.h"
#include "games/game_library.h"
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
        t.press(Action::Right);                  // game start animation inferno -> era1
        CHECK(t.app->settings().launchAnimation == "era1");
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

namespace {
struct FakeLauncher : ILauncher {
    std::vector<std::string> calls;
    bool fail = false;
    Status launch(const std::string& id) override {
        calls.push_back(id);
        return fail ? Status::fail(ErrorCode::IoError, "0x80940031 (app not found in the app database)") : Status::ok();
    }
};
}  // namespace

namespace {
struct CertFailNet : INetwork {
    std::atomic<bool> allow{false};
    std::atomic<int> calls{0};
    Result<HttpResponse> get(const std::string&, size_t) override {
        ++calls;
        if (!allow) return Result<HttpResponse>::fail(ErrorCode::CertificateError, "TLS certificate not verified (verifyErr 0x00000002)");
        HttpResponse r;
        r.status = 200;
        r.body = R"([{"tag_name":"build-9999","draft":false,"assets":[{"name":"IV0000-HENU00001_00-PS4HENULTIMATE00.pkg","size":4,"browser_download_url":"https://x/a.pkg"}]}])";
        return Result<HttpResponse>::ok(r);
    }
    Status stream(const std::string& u, uint64_t m, const Sink& sink) override {
        auto r = get(u, m);
        if (!r) return Status::fail(r.error().code, r.error().message);
        sink(r.value().body.data(), r.value().body.size(), r.value().body.size(), r.value().body.size());
        return Status::ok();
    }
    void setAllowUnverifiedTls(bool a) override { allow = a; }
};
}  // namespace

static void testUnverifiedTlsDialog() {
    TestApp t(freshDataDir("tls"));
    auto* net = new CertFailNet;
    t.p.net.reset(net);
    CHECK(t.app->init().isOk());
    for (int i = 0; i < 600 && t.app->booting(); ++i) t.app->frame(1.0f / 60);
    Settings s = t.app->settings();
    s.allowDownloads = true;
    CHECK(t.app->updateSettings(s));
    t.app->goToSectionById("update");
    auto settle = [&]() { for (int i = 0; i < 60; ++i) t.app->frame(1.0f / 60); };
    t.press(Action::Confirm);            // check -> certificate error
    settle();
    CHECK(net->calls >= 1 && !t.app->settings().allowUnverifiedTls);
    const int before = net->calls;
    t.press(Action::Confirm);            // offers the dialog (default: cancel)
    t.press(Action::Confirm);            // cancel -> nothing allowed
    settle();
    CHECK(!t.app->settings().allowUnverifiedTls && net->calls == before);
    t.press(Action::Confirm);            // dialog again
    t.press(Action::Left);
    t.press(Action::Confirm);            // allow -> retry
    settle();
    CHECK(t.app->settings().allowUnverifiedTls);
    CHECK(net->allow && net->calls > before);
}

namespace {
struct FakeAppQuery : IAppQuery {
    std::atomic<int> calls{0};
    Result<bool> isInstalled(const std::string& id) override {
        ++calls;
        return Result<bool>::ok(id == "CUSA11111" || id == "CUSA33333");
    }
};
}  // namespace

static void testInstallQueryFallback() {
    TestApp t(freshDataDir("query"));
    // Own console root: cheat files exist, /user/app does not exist at all.
    PosixFileSystem host;
    char cwd[4096];
    const std::string root = std::string(::getcwd(cwd, sizeof(cwd)) ? cwd : ".") + "/build/test-data/query-root";
    if (std::system(("rm -rf '" + root + "'").c_str()) != 0) CHECK(false);
    makeDirsUnchecked(host, root + "/data/GoldHEN/cheats/json");
    host.writeFile(root + "/data/GoldHEN/cheats/json/CUSA11111_01.00.json",
                   R"({"name":"Installed One","id":"CUSA11111","version":"01.00","process":"eboot.bin","mods":[{"name":"A","type":"checkbox","memory":[]}]})");
    host.writeFile(root + "/data/GoldHEN/cheats/json/CUSA22222_01.00.json",
                   R"({"name":"Not Installed","id":"CUSA22222","version":"01.00","process":"eboot.bin","mods":[{"name":"A","type":"checkbox","memory":[]}]})");
    t.p.sysRoot = root;
    t.p.dataRoot = "/data/OSTLERTEKKERS_HEN";
    auto* q = new FakeAppQuery;
    t.p.appQuery.reset(q);
    CHECK(t.app->init().isOk());
    // cached online list adds a third candidate with a name
    t.app->cacheCheatTitles({{"CUSA33333", "From The List"}, {"CUSA22222", "Not Installed"}});
    t.app->ensureScanned(true);
    for (int i = 0; i < 300 && (t.app->queryRunning() || t.app->games().all().size() < 2); ++i) t.app->frame(1.0f / 60);
    CHECK(!t.app->queryRunning());
    CHECK(q->calls == 3);    // CUSA11111, CUSA22222 (deduplicated), CUSA33333
    CHECK(t.app->games().all().size() == 2);
    const GameInfo* a = t.app->games().find("CUSA11111");
    const GameInfo* b = t.app->games().find("CUSA33333");
    CHECK(a && a->fromQuery && a->name == "Installed One" && a->version.empty());
    CHECK(b && b->name == "From The List");
    CHECK(!t.app->games().find("CUSA22222"));
    CHECK(t.app->gameScanNote().find("2 installed of 3") != std::string::npos);
}

static void testGameLaunch() {
    TestApp t(freshDataDir("launch"));
    auto* fl = new FakeLauncher;
    t.p.launcher.reset(fl);
    CHECK(t.app->init().isOk());
    for (int i = 0; i < 600 && t.app->booting(); ++i) t.app->frame(1.0f / 60);   // let the intro finish
    CHECK(t.app->canLaunch());
    GameInfo g;
    g.titleId = "CUSA00001";
    g.name = "Test Game";
    // success: launcher called once, at launchAt(), not before
    t.app->startLaunch(g);
    CHECK(t.app->launching());
    for (int i = 0; i < 60; ++i) t.app->frame(1.0f / 60);    // 1 s
    CHECK(fl->calls.empty());
    for (int i = 0; i < 120; ++i) t.app->frame(1.0f / 60);   // +2 s
    CHECK(fl->calls.size() == 1 && fl->calls[0] == "CUSA00001");
    for (int i = 0; i < 480; ++i) t.app->frame(1.0f / 60);   // still in the app after 8 s -> back to menu
    CHECK(!t.app->launching() && fl->calls.size() == 1);
    // failure: back to the menu with a toast
    fl->fail = true;
    t.app->startLaunch(g);
    for (int i = 0; i < 180; ++i) t.app->frame(1.0f / 60);
    CHECK(!t.app->launching() && fl->calls.size() == 2);
    // cancel with circle before the start
    fl->fail = false;
    t.app->startLaunch(g);
    t.app->frame(1.0f / 60);
    t.press(Action::Back);
    CHECK(!t.app->launching());
    for (int i = 0; i < 180; ++i) t.app->frame(1.0f / 60);
    CHECK(fl->calls.size() == 2);
}

void runPhaseCUiTests() {
    testUnverifiedTlsDialog();
    testInstallQueryFallback();
    testGameLaunch();
    testInputMapper();
    testFocus();
    testBundledThemes();
    testAppNavigation();
    testTranslationsComplete();
    testSettingsPersist();
    testMissingAssets();
}
