// PS4 HEN ULTIMATE - tools/preview/preview_main.cpp
// Runs the real App with the same software renderer the PS4 uses against a
// generated demo console folder and writes one PNG per scenario step.
// All demo games/cheats are fictional.
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "app/app.h"
#include "core/sfo.h"
#include "files/fs_util.h"
#include "gfx/soft_renderer.h"
#include "platform/posix_fs.h"

using namespace henu;

namespace {
class NoInput : public IInput {
public:
    PadState poll() override { PadState p; p.connected = true; return p; }
};
// Demo values for screenshots only (the PS4 build queries the real system).
class DemoSystem : public ISystemInfo {
public:
    SystemSnapshot snapshot() override {
        SystemSnapshot s;
        s.firmware = {true, "13.52", ""};
        s.language = {true, "de", ""};
        s.circleIsConfirm = {true, false, ""};
        s.ipAddress = {true, "192.168.0.42", ""};
        s.networkUp = {true, true, ""};
        return s;
    }
};

void put(IFileSystem& fs, const std::string& path, const std::string& data) {
    makeDirsUnchecked(fs, parentPath(path));
    fs.writeFile(path, data);
}

void demoGame(IFileSystem& fs, const std::string& id, const std::string& name, const std::string& cid, const std::string& ver) {
    makeDirsUnchecked(fs, "/user/app/" + id);
    Sfo s;
    s.strings["TITLE"] = name;
    s.strings["CONTENT_ID"] = cid;
    s.strings["CATEGORY"] = "gd";
    s.strings["APP_VER"] = ver;
    s.strings["VERSION"] = "01.00";
    put(fs, "/system_data/priv/appmeta/" + id + "/param.sfo", writeSfo(s));
}

std::string demoJson(const std::string& name, const std::string& id, const std::string& ver, const std::vector<std::string>& mods) {
    std::string m;
    for (size_t i = 0; i < mods.size(); ++i)
        m += std::string(i ? "," : "") + "{\"name\":\"" + mods[i] + "\",\"type\":\"checkbox\",\"memory\":[]}";
    return "{\"name\":\"" + name + "\",\"id\":\"" + id + "\",\"version\":\"" + ver +
           "\",\"process\":\"eboot.bin\",\"mods\":[" + m + "],\"credits\":[\"Demo\"]}";
}

void buildDemoRoot(IFileSystem& fs) {
    demoGame(fs, "CUSA99901", "Neon Drift Racer", "EP9999-CUSA99901_00-NEONDRIFT0000000", "01.02");
    demoGame(fs, "CUSA99902", "Skyforge Legends", "UP9999-CUSA99902_00-SKYFORGE00000000", "01.05");
    demoGame(fs, "CUSA99903", "Iron Harbor", "EP9999-CUSA99903_00-IRONHARBOR000000", "01.00");
    demoGame(fs, "CUSA99904", "Pixel Dungeon Tactics", "JP9999-CUSA99904_00-PIXELDUNGEON0000", "02.01");
    demoGame(fs, "CUSA99905", "Hardtekk Arena", "EP9999-CUSA99905_00-HARDTEKKARENA000", "01.10");
    demoGame(fs, "HENU00001", "PS4 HEN ULTIMATE", "IV0000-HENU00001_00-PS4HENULTIMATE00", "01.00");
    demoGame(fs, "LAPY20009", "Demo Homebrew Player", "IV0000-LAPY20009_00-DEMOPLAYER000000", "01.03");
    const std::string c = "/data/GoldHEN/cheats";
    put(fs, c + "/json/CUSA99901_01.02.json", demoJson("Neon Drift Racer", "CUSA99901", "01.02",
        {"Infinite Money", "Max Nitro", "Unlock All Cars", "Freeze Timer", "Max XP"}));
    put(fs, c + "/json/CUSA99902_01.04.json", demoJson("Skyforge Legends", "CUSA99902", "01.04",
        {"Infinite Health", "Infinite Stamina", "Max Gold", "Infinite Crafting Materials", "Max Strength", "One Hit Kill"}));
    put(fs, c + "/json/CUSA99905_01.10.json", demoJson("Hardtekk Arena", "CUSA99905", "01.10",
        {"Infinite Ammo", "God Mode", "Super Jump", "Max Skill Points"}));
    put(fs, c + "/shn/CUSA99906_01.00.shn", "<Trainer Game=\"Starfall Odyssey\" Cusa=\"CUSA99906\" Version=\"01.00\" Process=\"eboot.bin\">"
        "<Cheat Text=\"Infinite Shields\"></Cheat><Cheat Text=\"Max Credits\"></Cheat></Trainer>");
    put(fs, c + "/mc4/CUSA99903_01.00.mc4", "encrypted-demo");
    put(fs, "/data/GoldHEN/config.ini", "[GoldHEN]\n");
    put(fs, "/data/GoldHEN/plugins.ini", "[default]\n/data/GoldHEN/plugins/game_patch.prx\n[CUSA99905]\n/data/GoldHEN/plugins/afr.prx\n");
    put(fs, "/data/GoldHEN/plugins/game_patch.prx", "x");
    put(fs, "/data/GoldHEN/patches/json/CUSA99901.json", "{}");
    put(fs, "/mnt/usb0/CUSA99904_02.01.json", demoJson("Pixel Dungeon Tactics", "CUSA99904", "02.01", {"Infinite Mana", "Max Level"}));
    put(fs, "/mnt/usb0/CUSA99901_01.02.json", demoJson("Neon Drift Racer", "CUSA99901", "01.02", {"Infinite Money"}));
    put(fs, "/mnt/usb0/broken_name.json", "{}");
    makeDirsUnchecked(fs, "/user/home");
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: preview <bundleRoot> <sysRoot> <outDir>\n");
        return 2;
    }
    const std::string bundle = argv[1], sysRoot = argv[2], out = argv[3];
    PosixFileSystem fs;
    Platform p;
    p.fs.reset(new PosixFileSystem);
    auto* rec = new SoftwareRenderer(*p.fs);
    p.renderer.reset(rec);
    p.input.reset(new NoInput);
    p.system.reset(new DemoSystem);
    p.bundleRoot = bundle;
    p.sysRoot = sysRoot;
    p.dataRoot = "/data/PS4HENULTIMATE";
    {
        PrefixFileSystem demo(fs, sysRoot);
        makeDirsUnchecked(fs, sysRoot);
        buildDemoRoot(demo);
    }

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
        std::printf("%s  (section=%s, depth=%zu)\n", file, app.currentSection().c_str(), app.stackDepth());
    };
    auto press = [&](Action a, int times = 1) {
        for (int i = 0; i < times; ++i) { app.inject({a, false}); settle(2); }
    };
    auto section = [&](const std::string& id) {
        for (int i = 0; i < 14 && app.currentSection() != id; ++i) press(Action::NextSection);
    };

    {   // timing
        std::clock_t c0 = std::clock();
        for (int i = 0; i < 60; ++i) { app.inject({i % 2 ? Action::Left : Action::Right, false}); app.frame(1.0f / 60.0f); }
        std::printf("full redraw: %.1f ms per frame\n", 1000.0 * (std::clock() - c0) / CLOCKS_PER_SEC / 60.0);
    }

    capture("home");
    section("games");          capture("games");
    press(Action::Confirm);    capture("games_to_cheats");      // jumps to cheats filtered by the game
    press(Action::Menu); press(Action::Down); press(Action::Confirm);   // menu -> clear search
    capture("cheats_all");
    press(Action::Down);       press(Action::Confirm);           capture("cheat_detail");
    press(Action::Search);     capture("cheat_detail_category");
    press(Action::Back);
    press(Action::Menu);       capture("cheats_menu");
    press(Action::Down);       press(Action::Confirm);           capture("cheat_import");
    press(Action::Back);
    press(Action::Menu);       press(Action::Confirm);           capture("keyboard");
    press(Action::Back);
    section("themes");         press(Action::Right, 5);          capture("themes");
    section("goldhen");        capture("goldhen");
    section("system");         capture("system");
    section("files");          press(Action::Confirm);           capture("files");
    section("network");        capture("network");
    section("homebrew");       capture("homebrew");
    section("backup");         press(Action::Confirm);           capture("backup");
    section("update");         capture("update");
    section("diag");           capture("diag");
    section("home");           capture("home_after");
    return 0;
}
