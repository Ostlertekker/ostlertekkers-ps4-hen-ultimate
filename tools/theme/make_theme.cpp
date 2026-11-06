// OSTLERTEKKERS HEN - tools/theme/make_theme.cpp
// Builds the content folder of the EXPERIMENTAL PS4 system theme
// "OSTLERTEKKERS HEN" (a separate PKG, see docs/SYSTEM_THEME.md):
//
//   theme.xml                              format-version + themecolor are mandatory
//   texture/background/homescreen.png      1920x1080
//   texture/background/functionscreen.png  1920x1080
//   texture/preview.png                    740x416 (theme list thumbnail)
//   sce_sys/icon0.png                      512x512
//   sce_sys/pic1.png                       1920x1080
//   theme.gp4                              PkgTool project, volume pkg_ps4_theme
//
// Layout, sizes and theme.xml schema: PS4 Ultimate Theme Creator (MIT,
// github.com/Imxnxl/PS4-Ultimate-Theme-Creator, app/config.py, theme_xml.py,
// pkg.py), checked against a working console theme by its author.
// param.sfo is written by PkgTool in the Makefile (CATEGORY ac, IRO_TAG 2).
//
// usage: make_theme <repo root> <out dir> <content id>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#include "gfx/soft_renderer.h"
#include "platform/posix_fs.h"
#include "ui/draw.h"
#include "ui/logo.h"

using namespace henu;

namespace {

float hash01(int i, int salt) {
    uint32_t x = static_cast<uint32_t>(i) * 747796405u + static_cast<uint32_t>(salt) * 2891336453u + 12345u;
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (x & 0xFFFFFF) / 16777215.0f;
}

void circle(IRenderer& r, float x, float y, float rad, Color c) { r.fillRect({x - rad, y - rad, 2 * rad, 2 * rad}, c, rad); }

// Shared artwork: dark crimson gradient, glow, embers, big inverted pentagram.
void drawScene(IRenderer& r, float W, float H, float cx, float cy, float rad, float glow, int seed) {
    r.gradientRect({0, 0, W, H}, {8, 2, 4, 255}, {34, 4, 10, 255}, 0);
    for (int k = 0; k < 28; ++k) circle(r, cx, cy, rad * (0.7f + k * 0.1f), withAlpha({200, 16, 24, 255}, 0.011f * glow));
    for (int i = 0; i < 160; ++i) {
        const float x = hash01(i, seed) * W, y = hash01(i, seed + 1) * H;
        const float s = (1.2f + 2.6f * hash01(i, seed + 2)) * W / 1920.f;
        const Color c = mix({255, 70, 30, 255}, {255, 190, 110, 255}, hash01(i, seed + 3));
        circle(r, x, y, s, withAlpha(c, 0.15f + 0.45f * hash01(i, seed + 4)));
    }
    LogoStyle ls;
    ls.star = ls.ring = withAlpha({255, 40, 34, 255}, 0.10f * glow);
    ls.strokeRatio = 0.16f;
    drawPentagram(r, cx, cy, rad + 10, ls);
    ls.star = ls.ring = withAlpha({255, 60, 48, 255}, 0.22f * glow);
    ls.strokeRatio = 0.09f;
    drawPentagram(r, cx, cy, rad + 4, ls);
    ls.star = ls.ring = withAlpha({255, 214, 200, 255}, 0.55f * glow);
    ls.strokeRatio = 0.035f;
    drawPentagram(r, cx, cy, rad, ls);
}

bool savePng(const Canvas& c, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    f << encodePng(c);
    return static_cast<bool>(f);
}

// Downscale a canvas into a new renderer of size w x h.
void downscale(SoftwareRenderer& dst, const Canvas& src) {
    std::vector<uint32_t> px(src.data(), src.data() + static_cast<size_t>(src.width()) * src.height());
    TextureId t = dst.createTexture(src.width(), src.height(), px.data());
    dst.drawTexture(t, {0, 0, static_cast<float>(dst.outputWidth()), static_cast<float>(dst.outputHeight())}, 1, 0);
}

std::string xmlEsc(const std::string& s) {
    std::string o;
    for (char ch : s) {
        if (ch == '&') o += "&amp;"; else if (ch == '<') o += "&lt;"; else if (ch == '>') o += "&gt;";
        else if (ch == '"') o += "&quot;"; else o += ch;
    }
    return o;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <repo root> <out dir> <content id>\n", argv[0]);
        return 2;
    }
    const std::string root = argv[1], out = argv[2], contentId = argv[3];
    if (contentId.size() != 36) { std::fprintf(stderr, "content id must have 36 characters\n"); return 2; }
    PosixFileSystem fs;
    for (const char* d : {"", "/sce_sys", "/texture", "/texture/background"}) fs.makeDir(out + d);

    SoftwareRenderer big(fs, 1920, 1080);
    big.setThreads(4);
    auto title = big.loadFont(root + "/assets/fonts/inter-bold-34.hfnt", 34);
    auto small = big.loadFont(root + "/assets/fonts/inter-semibold-26.hfnt", 26);
    if (!title || !small) { std::fprintf(stderr, "fonts missing under %s/assets/fonts\n", root.c_str()); return 1; }

    // Home screen: icons sit in the upper middle, so the star stays right and low.
    big.beginFrame({0, 0, 0, 255});
    drawScene(big, 1920, 1080, 1460, 700, 330, 1.0f, 11);
    big.drawText(small.value(), "OSTLERTEKKERS HEN", 96, 1000, withAlpha({255, 200, 190, 255}, 0.55f), TextAlign::Left);
    big.endFrame();
    if (!savePng(big.canvas(), out + "/texture/background/homescreen.png")) return 1;
    if (!savePng(big.canvas(), out + "/sce_sys/pic1.png")) return 1;
    // Preview (theme list): same picture, scaled, with the name.
    {
        SoftwareRenderer pv(fs, 740, 416);
        auto pf = pv.loadFont(root + "/assets/fonts/inter-bold-34.hfnt", 34);
        pv.beginFrame({0, 0, 0, 255});
        downscale(pv, big.canvas());
        if (pf) pv.drawText(pf.value(), "OSTLERTEKKERS HEN", 34, 350, {255, 255, 255, 255}, TextAlign::Left);
        pv.endFrame();
        if (!savePng(pv.canvas(), out + "/texture/preview.png")) return 1;
    }
    // Function screen (top menu): darker, star on the left.
    big.beginFrame({0, 0, 0, 255});
    drawScene(big, 1920, 1080, 420, 640, 300, 0.6f, 23);
    big.fillRect({0, 0, 1920, 1080}, {0, 0, 0, 90}, 0);
    big.endFrame();
    if (!savePng(big.canvas(), out + "/texture/background/functionscreen.png")) return 1;
    // icon0 512x512.
    {
        SoftwareRenderer ic(fs, 512, 512);
        ic.beginFrame({0, 0, 0, 255});
        drawScene(ic, 512, 512, 256, 262, 190, 1.4f, 37);
        ic.endFrame();
        if (!savePng(ic.canvas(), out + "/sce_sys/icon0.png")) return 1;
    }

    // theme.xml (LF line ends; label only [A-Za-z0-9_-]).
    {
        std::ofstream x(out + "/theme.xml", std::ios::binary);
        x << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
             "<theme>\n"
             "    <format-version>4.0</format-version>\n"
             "    <themecolor>2</themecolor>\n"
             "    <label>OSTLERTEKKERS-HEN</label>\n"
             "    <fontcolor>#FFFFFFFF</fontcolor>\n"
             "    <fontshadowcolor>#FF000000</fontshadowcolor>\n"
             "    <focuscolor>#FFFF4A3C</focuscolor>\n"
             "    <homescreen-dimmer>#00FFFFFF</homescreen-dimmer>\n"
             "    <functionscreen-dimmer>#00FFFFFF</functionscreen-dimmer>\n"
             "    <titlename-dimmer>#00FFFFFF</titlename-dimmer>\n"
             "    <homebgm-enable>False</homebgm-enable>\n"
             "</theme>\n";
        if (!x) return 1;
    }

    // theme.gp4: no chunk_info and no storage/app type for themes.
    {
        const std::vector<std::string> files = {"sce_sys/param.sfo", "sce_sys/icon0.png", "sce_sys/pic1.png", "theme.xml",
                                                "texture/preview.png", "texture/background/homescreen.png",
                                                "texture/background/functionscreen.png"};
        char ts[32];
        const std::time_t now = std::time(nullptr);
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::gmtime(&now));
        std::ofstream g(out + "/theme.gp4", std::ios::binary);
        g << "<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"yes\"?>\n"
             "<psproject fmt=\"gp4\" version=\"1000\">\n"
             "  <volume>\n"
             "    <volume_type>pkg_ps4_theme</volume_type>\n"
             "    <volume_id>PS4VOLUME</volume_id>\n"
             "    <volume_ts>" << ts << "</volume_ts>\n"
             "    <package content_id=\"" << xmlEsc(contentId) << "\" passcode=\"00000000000000000000000000000000\"/>\n"
             "  </volume>\n"
             "  <files img_no=\"0\">\n";
        for (const auto& f : files) g << "    <file targ_path=\"" << f << "\" orig_path=\"" << f << "\"/>\n";
        g << "  </files>\n"
             "  <rootdir>\n"
             "    <dir targ_name=\"sce_sys\"/>\n"
             "    <dir targ_name=\"texture\">\n"
             "      <dir targ_name=\"background\"/>\n"
             "    </dir>\n"
             "  </rootdir>\n"
             "</psproject>\n";
        if (!g) return 1;
    }
    std::printf("theme content written to %s\n", out.c_str());
    return 0;
}
