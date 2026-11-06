// PS4 HEN ULTIMATE - tests for the software renderer (canvas, fonts, PNG).
#include <string>

#include "check.h"
#include "gfx/canvas.h"
#include "gfx/font.h"
#include "gfx/soft_renderer.h"
#include "platform/mem_fs.h"
#include "platform/posix_fs.h"

using namespace henu;

namespace {
Color px(const Canvas& c, int x, int y) { return Canvas::unpack(c.pixel(x, y)); }
bool near(Color a, Color b, int tol = 2) {
    auto d = [](int x, int y) { return x > y ? x - y : y - x; };
    return d(a.r, b.r) <= tol && d(a.g, b.g) <= tol && d(a.b, b.b) <= tol;
}
}  // namespace

static void testCanvas() {
    Canvas c(200, 100);
    c.clear({10, 20, 30, 255});
    CHECK(near(px(c, 0, 0), {10, 20, 30, 255}));
    CHECK(px(c, 5, 5).a == 255);

    // opaque rounded rect
    c.fillRoundRect({20, 20, 100, 60}, {200, 100, 50, 255}, 20);
    CHECK(near(px(c, 70, 50), {200, 100, 50, 255}));     // inside
    CHECK(near(px(c, 10, 50), {10, 20, 30, 255}));       // left of it
    CHECK(near(px(c, 21, 21), {10, 20, 30, 255}, 8));    // corner cut off by radius
    Color edge = px(c, 20, 50);                          // left edge pixel fully covered
    CHECK(near(edge, {200, 100, 50, 255}));

    // fractional edge -> anti-aliased (between both colors)
    Canvas a(40, 40);
    a.clear({0, 0, 0, 255});
    a.fillRoundRect({10.5f, 10, 20, 20}, {255, 255, 255, 255}, 0);
    Color half = px(a, 10, 15);
    CHECK(half.r > 100 && half.r < 155);
    CHECK(px(a, 15, 15).r == 255);

    // 50 % alpha blend
    Canvas b(10, 10);
    b.clear({0, 0, 0, 255});
    b.fillRoundRect({0, 0, 10, 10}, {200, 200, 200, 128}, 0);
    CHECK(near(px(b, 5, 5), {100, 100, 100, 255}, 2));

    // stroke leaves the inside untouched
    Canvas s(60, 60);
    s.clear({0, 0, 0, 255});
    s.strokeRoundRect({10, 10, 40, 40}, {255, 0, 0, 255}, 3, 8);
    CHECK(px(s, 30, 30).r == 0);
    CHECK(px(s, 30, 11).r > 200);
    CHECK(px(s, 5, 5).r == 0);

    // gradient: TL ~ from, BR ~ to (within dither tolerance)
    Canvas g(100, 100);
    g.gradientRoundRect({0, 0, 100, 100}, {0, 0, 0, 255}, {255, 255, 255, 255}, 0);
    CHECK(px(g, 1, 1).r < 8);
    CHECK(px(g, 98, 98).r > 247);
    CHECK(px(g, 50, 50).r > 115 && px(g, 50, 50).r < 140);

    // off-screen and degenerate shapes must not crash
    c.fillRoundRect({-500, -500, 2000, 2000}, {1, 2, 3, 255}, 900);
    c.fillRoundRect({150, 80, 0, 0}, {1, 2, 3, 255}, 5);
    c.strokeRoundRect({190, 90, 50, 50}, {1, 2, 3, 255}, 100, 10);
    c.gradientRoundRect({-10, -10, 5, 5}, {1, 2, 3, 255}, {4, 5, 6, 255}, 2);
    CHECK(near(px(c, 0, 0), {1, 2, 3, 255}));
}

static void testFonts() {
    PosixFileSystem fs;
    const char* files[] = {"inter-regular-20", "inter-regular-26", "inter-semibold-26", "inter-bold-18",
                           "inter-bold-28", "inter-bold-34", "inter-bold-44", "inter-bold-72"};
    for (const char* f : files) {
        auto bytes = fs.readFile(std::string("assets/fonts/") + f + ".hfnt", 4 << 20);
        CHECK(bytes.isOk());
        if (!bytes) continue;
        auto font = BitmapFont::parse(bytes.value());
        CHECK(font.isOk());
        if (!font) { std::printf("  %s: %s\n", f, font.error().message.c_str()); continue; }
        CHECK(font.value().glyphCount() >= 200);
        CHECK(font.value().measure("") == 0);
        CHECK(font.value().measure("Hallo") > 0);
        // PS button glyphs are present (DejaVu)
        CHECK(font.value().measure("\xE2\x9C\x95") > 0);   // ✕
        // kerning reduces "AV"
        CHECK(font.value().measure("AV") < font.value().measure("A") + font.value().measure("V"));
        // German umlauts measured as real glyphs
        CHECK(font.value().measure("\xC3\x84") > 0);       // Ä

        // corruption must fail cleanly
        std::string broken = bytes.value();
        broken.resize(broken.size() / 2);
        CHECK(!BitmapFont::parse(broken).isOk());
        broken = bytes.value();
        broken[0] = 'X';
        CHECK(!BitmapFont::parse(broken).isOk());
    }
    CHECK(!BitmapFont::parse("").isOk());

    auto cps = decodeUtf8("a\xC3\xA4\xE2\x82\xAC\xF0\x9F\x98\x80\xFF");
    CHECK(cps.size() == 5 && cps[0] == 'a' && cps[1] == 0xE4 && cps[2] == 0x20AC && cps[3] == 0x1F600 && cps[4] == 0xFFFD);
}

static void testSoftwareRenderer() {
    PosixFileSystem fs;
    SoftwareRenderer r(fs, 320, 180);
    auto f = r.loadFont("assets/fonts/inter-bold-34.hfnt", 34);
    CHECK(f.isOk());
    CHECK(!r.loadFont("assets/fonts/inter-bold-34.hfnt", 20).isOk());   // size mismatch
    CHECK(!r.loadFont("assets/fonts/missing.hfnt", 20).isOk());

    int presented = 0;
    r.setPresenter([&](const Canvas&) { ++presented; });
    r.beginFrame({0, 0, 0, 255});
    CHECK(!r.useCachedLayer(42));
    r.fillRect({0, 0, 320, 180}, {50, 60, 70, 255}, 0);
    r.endLayer();
    r.drawText(f.value(), "Test", 10, 10, {255, 255, 255, 255}, TextAlign::Left);
    r.endFrame();
    CHECK(presented == 1 && r.texts().size() == 1);
    // text was really drawn (some pixel brighter than background)
    bool lit = false;
    for (int y = 10; y < 50 && !lit; ++y)
        for (int x = 10; x < 90; ++x)
            if (Canvas::unpack(r.canvas().pixel(x, y)).r > 200) { lit = true; break; }
    CHECK(lit);

    // second frame: background layer comes from cache
    r.beginFrame({0, 0, 0, 255});
    CHECK(r.useCachedLayer(42));
    r.endFrame();
    CHECK(near(Canvas::unpack(r.canvas().pixel(300, 170)), {50, 60, 70, 255}));
    CHECK(r.texts().empty());
    // a different key re-renders
    r.beginFrame({0, 0, 0, 255});
    CHECK(!r.useCachedLayer(43));
    r.endLayer();
    r.endFrame();

    // PNG
    std::string png = encodePng(r.canvas());
    CHECK(png.size() > 320 * 180 * 3);
    CHECK(png.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0);
    CHECK(png.compare(png.size() - 8, 4, "IEND") == 0);
}

void runGfxTests() {
    testCanvas();
    testFonts();
    testSoftwareRenderer();
}
