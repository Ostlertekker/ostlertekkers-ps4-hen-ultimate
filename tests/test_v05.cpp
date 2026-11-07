// OSTLERTEKKERS HEN - tests for v0.5: cheat editing, online parsing, audio, boot intros, logo.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "app/boot.h"
#include "audio/audio.h"
#include "cheats/cheat_db.h"
#include "cheats/cheat_edit.h"
#include "check.h"
#include "core/sfo.h"
#include "core/sha256.h"
#include "games/game_library.h"
#include "gfx/soft_renderer.h"
#include "net/online.h"
#include "platform/mem_fs.h"
#include "themes/theme.h"
#include "ui/logo.h"

using namespace henu;

static const char* kJson = R"({
  "name": "Demo Game", "id": "CUSA99901", "version": "01.02", "process": "eboot.bin",
  "credits": ["someone"],
  "mods": [
    {"name": "Infinite Health", "type": "checkbox", "memory": [{"offset": "0x1000", "on": "90 90", "off": "89 01"}]},
    {"name": "", "type": "checkbox", "memory": []},
    {"name": "Max Money", "type": "checkbox", "memory": [{"offset": "0x2000", "on": "FF", "off": "00"}]},
    {"name": "Unlock All", "type": "checkbox", "memory": []}
  ]
})";

static const char* kShn = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                          "<Trainer Game=\"Demo\" Moder=\"x\" Cusa=\"CUSA99902\" Version=\"01.00\" Process=\"eboot.bin\">\n"
                          "  <Cheat Text=\"Infinite Ammo\" Description=\"a\">\n"
                          "    <Cheatline><Offset>10</Offset><ValueOn>90</ValueOn><ValueOff>01</ValueOff></Cheatline>\n"
                          "  </Cheat>\n"
                          "  <Cheat Text=\"God Mode\">\n"
                          "    <Cheatline><Offset>20</Offset><ValueOn>90</ValueOn><ValueOff>01</ValueOff></Cheatline>\n"
                          "  </Cheat>\n"
                          "</Trainer>\n";

static void testCheatEdit() {
    // JSON remove: index counts named cheats only ("" entry is skipped)
    CheatEdit rm;
    rm.op = CheatEditOp::Remove;
    rm.index = 1;   // "Max Money"
    auto out = editCheatFile("CUSA99901_01.02.json", kJson, rm);
    CHECK(out.isOk());
    if (out) {
        auto f = parseCheatFile("CUSA99901_01.02.json", out.value());
        CHECK(f.isOk() && f.value().cheats.size() == 2);
        if (f) CHECK(f.value().cheats[0].name == "Infinite Health" && f.value().cheats[1].name == "Unlock All");
        CHECK(out.value().find("0x1000") != std::string::npos);       // other data kept
        CHECK(out.value().find("Demo Game") != std::string::npos);
    }
    // JSON rename
    CheatEdit rn;
    rn.op = CheatEditOp::Rename;
    rn.index = 2;
    rn.newName = "Alles freischalten";
    out = editCheatFile("CUSA99901_01.02.json", kJson, rn);
    CHECK(out.isOk());
    if (out) {
        auto f = parseCheatFile("CUSA99901_01.02.json", out.value());
        CHECK(f.isOk() && f.value().cheats.size() == 3 && f.value().cheats[2].name == "Alles freischalten");
    }
    // out of range, empty rename, mc4, broken file
    rm.index = 3;
    CHECK(!editCheatFile("CUSA99901_01.02.json", kJson, rm).isOk());
    rn.newName = "";
    CHECK(!editCheatFile("CUSA99901_01.02.json", kJson, rn).isOk());
    rm.index = 0;
    CHECK(!editCheatFile("CUSA99901_01.02.mc4", "xx", rm).isOk());
    CHECK(!editCheatFile("CUSA99901_01.02.json", "{ broken", rm).isOk());
    CHECK(!editCheatFile("not-a-cheat.json", kJson, rm).isOk());

    // SHN remove + rename
    rm.index = 0;
    out = editCheatFile("CUSA99902_01.00.shn", kShn, rm);
    CHECK(out.isOk());
    if (out) {
        auto f = parseCheatFile("CUSA99902_01.00.shn", out.value());
        CHECK(f.isOk() && f.value().cheats.size() == 1 && f.value().cheats[0].name == "God Mode");
        CHECK(out.value().find("Infinite Ammo") == std::string::npos);
        CHECK(out.value().find("<Offset>20</Offset>") != std::string::npos);
        CHECK(out.value().find("\n\n") == std::string::npos);   // no empty line left behind
    }
    rn.index = 1;
    rn.newName = "Gott & \"Modus\"";
    out = editCheatFile("CUSA99902_01.00.shn", kShn, rn);
    CHECK(out.isOk());
    if (out) {
        CHECK(out.value().find("Text=\"Gott &amp; &quot;Modus&quot;\"") != std::string::npos);
        auto f = parseCheatFile("CUSA99902_01.00.shn", out.value());
        CHECK(f.isOk() && f.value().cheats.size() == 2);
    }
    // UTF-16 LE SHN comes back as UTF-8
    std::string u16 = "\xFF\xFE";
    for (const char* p = kShn; *p; ++p) { u16 += *p; u16 += '\0'; }
    rm.index = 1;
    out = editCheatFile("CUSA99902_01.00.shn", u16, rm);
    CHECK(out.isOk());
    if (out) {
        CHECK(out.value().compare(0, 5, "<?xml") == 0);
        auto f = parseCheatFile("CUSA99902_01.00.shn", out.value());
        CHECK(f.isOk() && f.value().cheats.size() == 1 && f.value().cheats[0].name == "Infinite Ammo");
    }
}

static void testOnlineParsing() {
    const std::string idx = "CUSA00001_01.00.json=Game One\n"
                            "CUSA00002_01.05.json=Game, Two = Deluxe\r\n"
                            "\n"
                            "garbage line\n"
                            "../evil_01.00.json=x\n"
                            "CUSA00003_01.00.shn=Wrong format here\n";
    auto e = parseCheatIndex(idx, "json");
    CHECK(e.size() == 2);
    if (e.size() == 2) {
        CHECK(e[0].titleId == "CUSA00001" && e[0].version == "01.00" && e[0].name == "Game One");
        CHECK(e[1].name == "Game, Two = Deluxe");
        CHECK(e[1].url() == std::string(kCheatRepoBase) + "json/CUSA00002_01.05.json");
    }

    CHECK(buildFromTag("build-12") == 12);
    CHECK(buildFromTag("v1.0") == 0);
    CHECK(buildFromTag("build-") == 0);

    const std::string rel = R"([
      {"tag_name": "build-14", "draft": true, "name": "draft", "assets": [{"name": "a.pkg", "size": 1, "browser_download_url": "https://x/a.pkg"}]},
      {"tag_name": "build-13", "draft": false, "name": "B13", "body": "notes", "published_at": "2026-11-06T10:00:00Z",
       "assets": [{"name": "IV0000-X.pkg", "size": 8000000, "browser_download_url": "https://github.com/o/r/releases/download/build-13/IV0000-X.pkg"},
                  {"name": "SHA256SUMS.txt", "size": 100, "browser_download_url": "https://github.com/o/r/releases/download/build-13/SHA256SUMS.txt"}]},
      {"tag_name": "build-12", "draft": false, "assets": [{"name": "old.pkg", "size": 1, "browser_download_url": "https://x/old.pkg"}]}
    ])";
    auto r = pickNewestRelease(rel);
    CHECK(r.isOk());
    if (r) {
        CHECK(r.value().build == 13 && r.value().pkgName == "IV0000-X.pkg" && r.value().pkgSize == 8000000);
        CHECK(r.value().sumsUrl.find("SHA256SUMS.txt") != std::string::npos);
    }
    // theme PKG listed first: the app PKG is still chosen
    const std::string rel2 = R"([{"tag_name": "build-20", "draft": false, "assets": [
        {"name": "IV0000-HENT00001_00-OSTLERTEKKERS000.pkg", "size": 5, "browser_download_url": "https://x/theme.pkg"},
        {"name": "IV0000-HENU00001_00-PS4HENULTIMATE00.pkg", "size": 9, "browser_download_url": "https://x/app.pkg"}]}])";
    auto r2 = pickNewestRelease(rel2);
    CHECK(r2.isOk() && r2.value().pkgUrl == "https://x/app.pkg" && r2.value().pkgSize == 9);
    CHECK(!pickNewestRelease("[]").isOk());
    CHECK(!pickNewestRelease("not json").isOk());

    const std::string h = sha256Hex("abc");
    auto s = sha256For(h + "  IV0000-X.pkg\nffff  other.pkg\n", "IV0000-X.pkg");
    CHECK(s.isOk() && s.value() == h);
    CHECK(!sha256For("ffff  other.pkg\n", "IV0000-X.pkg").isOk());
}

static void testAudio() {
    Pcm a = synthBootSound(0), b = synthBootSound(0), c = synthBootSound(3);
    CHECK(a.seconds() > 4 && a.seconds() < 9);
    CHECK(a.samples == b.samples);       // deterministic
    CHECK(a.samples != c.samples);       // variants differ
    int peak = 0;
    for (int16_t v : a.samples) peak = std::max(peak, std::abs(static_cast<int>(v)));
    CHECK(peak > 3000 && peak < 32767);  // audible, not clipping

    // WAV roundtrip
    auto back = decodeWav(encodeWav(a), 30);
    CHECK(back.isOk() && back.value().samples.size() == a.samples.size());
    if (back) {
        int maxDiff = 0;
        for (size_t i = 0; i < a.samples.size(); ++i) maxDiff = std::max(maxDiff, std::abs(a.samples[i] - back.value().samples[i]));
        CHECK(maxDiff <= 1);
    }

    // mono 8-bit 24 kHz -> stereo 48 kHz
    std::string w = "RIFF";
    auto u32 = [&w](uint32_t v) { for (int i = 0; i < 4; ++i) w += static_cast<char>((v >> (8 * i)) & 0xFF); };
    auto u16 = [&w](uint16_t v) { w += static_cast<char>(v & 0xFF); w += static_cast<char>(v >> 8); };
    const uint32_t n = 2400;   // 0.1 s
    u32(36 + n); w += "WAVEfmt "; u32(16); u16(1); u16(1); u32(24000); u32(24000); u16(1); u16(8);
    w += "data"; u32(n);
    for (uint32_t i = 0; i < n; ++i) w += static_cast<char>(i % 2 ? 200 : 56);
    auto m = decodeWav(w, 30);
    CHECK(m.isOk());
    if (m) CHECK(m.value().frames() >= 4700 && m.value().frames() <= 4900);
    CHECK(!decodeWav("RIFFxxxxWAVE", 30).isOk());
    CHECK(!decodeWav("", 30).isOk());

    Pcm v = a;
    applyVolume(v, 0);
    bool silent = true;
    for (int16_t s : v.samples) if (s != 0) { silent = false; break; }
    CHECK(silent);
}

static void testBootAndLogo() {
    MemFileSystem fs;
    SoftwareRenderer r(fs, 1920, 1080);
    Fonts f;   // no fonts loaded: drawing must still be safe
    CHECK(bootStyles().size() == 6);
    for (const auto& s : bootStyles()) {
        BootSequence b(s.id);
        for (double t : {0.0, 0.5, 1.7, 3.2, 5.0, 6.3}) {
            r.beginFrame({0, 0, 0, 255});
            b.draw(r, f, t);
            r.endFrame();
        }
        CHECK(b.finished(b.duration()) && !b.finished(1.0));
        // the frame is not empty
        bool lit = false;
        r.beginFrame({0, 0, 0, 255});
        b.draw(r, f, 4.8);
        const Canvas& c = r.canvas();
        for (int y = 0; y < c.height() && !lit; y += 3)
            for (int x = 0; x < c.width(); x += 3) {
                Color p = Canvas::unpack(c.pixel(x, y));
                if (p.r + p.g + p.b > 60) { lit = true; break; }
            }
        r.endFrame();
        CHECK(lit);
    }
    CHECK(resolveBootStyle("off", 1).empty());
    CHECK(resolveBootStyle("era3", 1) == "era3");
    CHECK(resolveBootStyle("nonsense", 1) == "inferno");
    const std::string rnd = resolveBootStyle("random", 7);
    CHECK(!rnd.empty() && rnd != "random");

    // logo: centre of the pentagram is empty, a star tip is drawn
    r.beginFrame({0, 0, 0, 255});
    LogoStyle ls;
    drawPentagram(r, 960, 540, 300, ls);
    r.endFrame();
    const Canvas& c = r.canvas();
    CHECK(Canvas::unpack(c.pixel(960, 540 + 297)).r > 128 || Canvas::unpack(c.pixel(960, 540 + 290)).r > 128);   // ring / bottom tip
    CHECK(Canvas::unpack(c.pixel(960, 540)).r < 128);                                                // inner pentagon empty
    CHECK(std::string(kBrandName) == "OSTLERTEKKERS HEN");
}

static void testParallelRenderer() {
    MemFileSystem fs;
    SoftwareRenderer one(fs, 640, 360), many(fs, 640, 360);
    many.setThreads(4);
    CHECK(one.threads() == 1 && many.threads() == 4);
    Fonts f;
    for (const auto& s : bootStyles()) {
        BootSequence b(s.id);
        for (double t : {0.3, 2.0, 3.5, 3.6, 5.5}) {
            one.beginFrame({0, 0, 0, 255});
            many.beginFrame({0, 0, 0, 255});
            b.draw(one, f, t);
            b.draw(many, f, t);
            one.endFrame();
            many.endFrame();
            CHECK(std::memcmp(one.canvas().data(), many.canvas().data(), 640u * 360u * 4u) == 0);
        }
    }
    // shapes crossing band borders, strokes, gradients, layer cache
    for (SoftwareRenderer* r : {&one, &many}) {
        r->beginFrame({10, 20, 30, 255});
        r->useCachedLayer(7);
        r->gradientRect({13.5f, 7.25f, 600, 340}, {255, 0, 0, 200}, {0, 0, 255, 90}, 40);
        r->endLayer();
        r->strokeRect({100.5f, 50, 300, 200}, {255, 255, 255, 180}, 1.5f, 24);
        r->fillPath({{{320, 10}, {630, 350}, {10, 350}}}, {0, 255, 0, 120});
        r->endFrame();
    }
    CHECK(std::memcmp(one.canvas().data(), many.canvas().data(), 640u * 360u * 4u) == 0);
    for (SoftwareRenderer* r : {&one, &many}) {
        r->beginFrame({0, 0, 0, 255});
        CHECK(r->useCachedLayer(7));
        r->endFrame();
    }
    CHECK(std::memcmp(one.canvas().data(), many.canvas().data(), 640u * 360u * 4u) == 0);
    many.setThreads(1);
    CHECK(many.threads() == 1);
}

static void testGamesFromAppmetaOnly() {
    MemFileSystem fs;
    auto sfo = [](const std::string& title, const std::string& cid) {
        Sfo d;
        d.strings["TITLE"] = title;
        d.strings["CONTENT_ID"] = cid;
        d.strings["APP_VER"] = "01.03";
        d.strings["CATEGORY"] = "gd";
        return writeSfo(d);
    };
    fs.makeDir("/system_data/priv/appmeta");
    fs.makeDir("/system_data/priv/appmeta/CUSA77701");
    fs.writeFile("/system_data/priv/appmeta/CUSA77701/param.sfo", sfo("Meta Game", "EP0000-CUSA77701_00-METAGAME00000000"));
    fs.makeDir("/system_data/priv/appmeta/NPXS20001");
    fs.writeFile("/system_data/priv/appmeta/NPXS20001/param.sfo", sfo("System", "XX"));
    fs.makeDir("/system_data/priv/appmeta/CUSA77702");   // no param.sfo -> skipped
    GameLibrary lib;
    GamePaths paths;
    paths.loadIcons = false;
    Status st = lib.scan(fs, paths);   // /user/app does not exist
    CHECK(st.isOk());
    CHECK(lib.all().size() == 1);
    if (!lib.all().empty()) {
        CHECK(lib.all()[0].titleId == "CUSA77701" && lib.all()[0].name == "Meta Game");
        CHECK(lib.all()[0].region == Region::EU && lib.all()[0].version == "01.03");
        CHECK(lib.all()[0].appPath == "/user/app/CUSA77701");
    }
}

static void testAppearance() {
    Theme base = defaultTheme();
    // default style keeps the theme as-is but records the FX/border flags
    Theme d = applyAppearance(base, "default", "galaxy", true);
    CHECK(d.backgroundFx == "galaxy" && d.rgbBorder && backgroundIsAnimated(d));
    CHECK(!backgroundIsAnimated(applyAppearance(base, "default", "theme", false)));
    // PS5: larger radius, opaque-er surfaces, white focus
    Theme ps5 = applyAppearance(base, "ps5", "theme", false);
    CHECK(ps5.cornerRadius >= base.cornerRadius && ps5.cornerRadius >= 22);
    CHECK(ps5.surfaceOpacity >= base.surfaceOpacity);
    CHECK(ps5.focus.r == 255 && ps5.focus.g == 255 && ps5.focus.b == 255);
    // PS3: small radius, more translucent
    Theme ps3 = applyAppearance(base, "ps3", "wave", false);
    CHECK(ps3.cornerRadius <= 8 && ps3.surfaceOpacity < base.surfaceOpacity);
    CHECK(ps3.backgroundFx == "wave" && backgroundIsAnimated(ps3));
    // rainbow spans the hue wheel and wraps
    Color r0 = rainbow(0.f), r2 = rainbow(2.f / 6), r4 = rainbow(4.f / 6);
    CHECK(r0.r == 255 && r0.g == 0);            // red
    CHECK(r2.g == 255 && r2.r == 0);            // green
    CHECK(r4.b == 255 && r4.g == 0);            // blue
    Color wrap = rainbow(1.0f);                  // == rainbow(0)
    CHECK(wrap.r == r0.r && wrap.g == r0.g && wrap.b == r0.b);
}

void runV05Tests() {
    testAppearance();
    testGamesFromAppmetaOnly();
    testParallelRenderer();
    testCheatEdit();
    testOnlineParsing();
    testAudio();
    testBootAndLogo();
}
