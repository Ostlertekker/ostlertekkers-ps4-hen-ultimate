// OSTLERTEKKERS HEN - tools/preview/preview_main.cpp
// Runs the real App with the same software renderer the PS4 uses against a
// generated demo console folder and writes one PNG per scenario step.
// All demo games/cheats are fictional.
#include <cstdio>
#include <map>
#include <ctime>
#include <string>
#include <vector>

#include "app/app.h"
#include "net/online.h"
#include "core/sfo.h"
#include "core/sha256.h"
#include "files/fs_util.h"
#include "gfx/soft_renderer.h"
#include "platform/posix_fs.h"

using namespace henu;

namespace {
class NoInput : public IInput {
public:
    PadState poll() override { PadState p; p.connected = true; return p; }
};
// Fictional online data for screenshots (no real network access).
class DemoNet : public INetwork {
public:
    DemoNet() {
        pkg_ = std::string(4096, 'P');
        body_[std::string(kCheatRepoBase) + "json.txt"] =
            "CUSA99901_01.02.json=Neon Drift Racer\nCUSA99902_01.05.json=Skyforge Legends\nCUSA99902_01.03.json=Skyforge Legends\n"
            "CUSA99903_01.00.json=Iron Harbor\nCUSA99905_01.08.json=Hardtekk Arena\nCUSA99905_01.10.json=Hardtekk Arena\n"
            "CUSA99977_01.00.json=Some Other Game\n";
        body_[std::string(kCheatRepoBase) + "shn.txt"] = "CUSA99904_02.01.shn=Pixel Dungeon Tactics\n";
        body_[std::string(kCheatRepoBase) + "mc4.txt"] = "";
        body_[std::string(kCheatRepoBase) + "json/CUSA99905_01.10.json"] =
            "{\"name\":\"Hardtekk Arena\",\"id\":\"CUSA99905\",\"version\":\"01.10\",\"process\":\"eboot.bin\","
            "\"mods\":[{\"name\":\"Infinite Health\",\"type\":\"checkbox\",\"memory\":[]}],\"credits\":[\"Demo\"]}";
        const std::string base = "https://github.com/demo/releases/download/build-99/";
        body_["https://api.github.com/repos/" + std::string(kUpdateRepo) + "/releases?per_page=10"] =
            "[{\"tag_name\":\"build-99\",\"draft\":false,\"name\":\"OSTLERTEKKERS HEN v0.5.0 - Build 99\","
            "\"body\":\"Demo-Release fuer die Vorschau\",\"published_at\":\"2026-11-06T10:00:00Z\",\"assets\":["
            "{\"name\":\"IV0000-HENU00001_00-PS4HENULTIMATE00.pkg\",\"size\":4096,\"browser_download_url\":\"" + base + "IV0000-HENU00001_00-PS4HENULTIMATE00.pkg\"},"
            "{\"name\":\"SHA256SUMS.txt\",\"size\":100,\"browser_download_url\":\"" + base + "SHA256SUMS.txt\"}]}]";
        body_[base + "IV0000-HENU00001_00-PS4HENULTIMATE00.pkg"] = pkg_;
        body_[base + "SHA256SUMS.txt"] = sha256Hex(pkg_) + "  IV0000-HENU00001_00-PS4HENULTIMATE00.pkg\n";
    }
    Result<HttpResponse> get(const std::string& url, size_t maxBytes) override {
        HttpResponse r;
        auto it = body_.find(url);
        r.status = it == body_.end() ? 404 : 200;
        if (it != body_.end()) r.body = it->second.substr(0, maxBytes);
        return Result<HttpResponse>::ok(r);
    }
    Status stream(const std::string& url, uint64_t maxBytes, const Sink& sink) override {
        auto it = body_.find(url);
        if (it == body_.end()) return Status::fail(ErrorCode::NetworkError, "HTTP 404");
        if (it->second.size() > maxBytes) return Status::fail(ErrorCode::NetworkError, "too large");
        sink(it->second.data(), it->second.size(), it->second.size(), it->second.size());
        return Status::ok();
    }

private:
    std::map<std::string, std::string> body_;
    std::string pkg_;
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
    demoGame(fs, "HENU00001", "OSTLERTEKKERS HEN", "IV0000-HENU00001_00-PS4HENULTIMATE00", "01.00");
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
    p.net.reset(new DemoNet);
    p.bundleRoot = bundle;
    p.sysRoot = sysRoot;
    p.dataRoot = "/data/OSTLERTEKKERS_HEN";
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
    press(Action::Down, 4);    press(Action::Confirm);           capture("game_detail");
    press(Action::Down);       press(Action::Confirm);
    press(Action::Confirm);                                      capture("online_consent");
    press(Action::Left);       press(Action::Confirm);   settle(30);   capture("online_cheats");
    section("cheats");         capture("cheats_all");
    press(Action::Down);       press(Action::Confirm);           capture("cheat_detail");
    press(Action::Menu);       capture("cheat_edit_menu");
    press(Action::Confirm);    press(Action::Left);  press(Action::Confirm);   capture("cheat_removed");
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
    section("update");         press(Action::Confirm);   settle(30);  capture("update");
    press(Action::Confirm);    press(Action::Left);  press(Action::Confirm);   settle(30);  capture("update_ready");
    section("diag");           capture("diag");
    section("settings");       press(Action::Down, 4);           capture("settings");
    section("home");           capture("home_after");
    app.startBoot("inferno");  settle(150);                      capture("boot_inferno");
    return 0;
}
