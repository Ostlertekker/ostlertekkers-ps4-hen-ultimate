// OSTLERTEKKERS HEN - app/boot.cpp
#include "app/boot.h"

#include <algorithm>
#include <cmath>

#include "ui/logo.h"

namespace henu {

namespace {
constexpr float kPi = 3.14159265358979f;
constexpr double kHit = 3.45;   // logo "impact", in sync with synthBootSound()
constexpr float kCx = kScreenW / 2, kCy = 430;

float clamp01(double v) { return static_cast<float>(v < 0 ? 0 : (v > 1 ? 1 : v)); }
float ramp(double t, double a, double b) { return clamp01((t - a) / (b - a)); }
float easeOut(float x) { return 1 - (1 - x) * (1 - x) * (1 - x); }
float easeInOut(float x) { return x < 0.5f ? 4 * x * x * x : 1 - std::pow(-2 * x + 2, 3.f) / 2; }

// Deterministic per-index pseudo random in 0..1.
float hash01(int i, int salt) {
    uint32_t x = static_cast<uint32_t>(i) * 747796405u + static_cast<uint32_t>(salt) * 2891336453u + 12345u;
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return (x & 0xFFFFFF) / 16777215.0f;
}

void circle(IRenderer& r, float x, float y, float rad, Color c) { r.fillRect({x - rad, y - rad, 2 * rad, 2 * rad}, c, rad); }

void ringPulse(IRenderer& r, float cx, float cy, float rad, float w, Color c) {
    std::vector<IRenderer::Pt> o, i;
    const int n = std::max(48, static_cast<int>(rad / 3));
    for (int k = 0; k < n; ++k) {
        const float a = 2 * kPi * k / n;
        o.push_back({cx + rad * std::cos(a), cy + rad * std::sin(a)});
        i.push_back({cx + (rad - w) * std::cos(a), cy + (rad - w) * std::sin(a)});
    }
    r.fillPath({o, i}, c);
}

void glowingLogo(IRenderer& r, float cx, float cy, float radius, float progress, Color core, Color glow, float glowAmt) {
    LogoStyle s;
    s.progress = progress;
    if (glowAmt > 0) {
        s.star = s.ring = withAlpha(glow, 0.10f * glowAmt);
        s.strokeRatio = 0.22f;
        drawPentagram(r, cx, cy, radius + 8, s);
        s.star = s.ring = withAlpha(glow, 0.22f * glowAmt);
        s.strokeRatio = 0.13f;
        drawPentagram(r, cx, cy, radius + 3, s);
    }
    s.star = s.ring = core;
    s.strokeRatio = 0.06f;
    drawPentagram(r, cx, cy, radius, s);
}
}  // namespace

const std::vector<BootStyleInfo>& bootStyles() {
    static const std::vector<BootStyleInfo> v = {
        {"inferno", "boot.style.inferno", 0}, {"era1", "boot.style.era1", 1}, {"era2", "boot.style.era2", 2},
        {"era3", "boot.style.era3", 3},       {"era4", "boot.style.era4", 4}, {"era5", "boot.style.era5", 5},
    };
    return v;
}

std::string resolveBootStyle(const std::string& setting, unsigned seed) {
    if (setting == "off") return "";
    const auto& st = bootStyles();
    if (setting == "random") return st[seed % st.size()].id;
    for (const auto& s : st) if (setting == s.id) return s.id;
    return "inferno";
}

int bootSoundVariant(const std::string& id) {
    for (const auto& s : bootStyles()) if (id == s.id) return s.soundVariant;
    return 0;
}

BootSequence::BootSequence(std::string styleId) : style_(std::move(styleId)) {}

void BootSequence::draw(IRenderer& r, const Fonts& f, double t) const {
    Color text{255, 255, 255, 255}, accent{255, 80, 70, 255};
    if (style_ == "era1") { drawPolygon(r, t); accent = {255, 196, 60, 255}; }
    else if (style_ == "era2") { drawFog(r, t); accent = {120, 170, 255, 255}; }
    else if (style_ == "era3") { drawWaves(r, t); accent = {255, 170, 90, 255}; }
    else if (style_ == "era4") { drawTrails(r, t); accent = {90, 200, 255, 255}; }
    else if (style_ == "era5") { drawGleam(r, t); accent = {200, 200, 210, 255}; }
    else { drawInferno(r, t); }
    drawBrand(r, f, t, text, accent);
    // Fade to black at the end.
    const float out = ramp(t, duration() - 0.8, duration());
    if (out > 0) r.fillRect({0, 0, kScreenW, kScreenH}, withAlpha({0, 0, 0, 255}, out), 0);
}

void BootSequence::drawBrand(IRenderer& r, const Fonts& f, double t, Color text, Color accent) const {
    const float a = easeOut(ramp(t, kHit, kHit + 0.6));
    if (a <= 0) return;
    const float y = 720 + (1 - a) * 30;
    r.drawText(f.h1, kBrandName, kCx, y, withAlpha(text, a), TextAlign::Center);
    r.drawText(f.title, "P S 4   \xC2\xB7   G O L D H E N", kCx, y + 104, withAlpha(accent, a), TextAlign::Center);
    const float lineW = 420 * easeOut(ramp(t, kHit + 0.2, kHit + 1.2));
    r.fillRect({kCx - lineW / 2, y + 160, lineW, 2}, withAlpha(accent, 0.7f * a), 1);
}

// ---------------------------------------------------------------- inferno
void BootSequence::drawInferno(IRenderer& r, double t) const {
    r.fillRect({0, 0, kScreenW, kScreenH}, {6, 2, 4, 255}, 0);
    const float heat = ramp(t, 0.2, 3.4);
    for (int k = 0; k < 5; ++k)
        circle(r, kCx, kCy + 40, 260.f + k * 150.f * (0.6f + 0.4f * heat), withAlpha({200, 20, 24, 255}, 0.05f * heat));
    // Rising embers.
    for (int i = 0; i < 110; ++i) {
        const float speed = 60 + 160 * hash01(i, 1);
        const float x0 = hash01(i, 2) * kScreenW;
        const float y = kScreenH + 40 - std::fmod(static_cast<float>(t) * speed + hash01(i, 3) * 1200.f, 1300.f);
        const float x = x0 + std::sin(static_cast<float>(t) * (0.8f + hash01(i, 4)) + i) * 26;
        const float flick = 0.5f + 0.5f * std::sin(static_cast<float>(t) * (5 + 6 * hash01(i, 5)) + i);
        const float size = 1.5f + 3.5f * hash01(i, 6);
        const Color c = mix({255, 70, 20, 255}, {255, 200, 90, 255}, hash01(i, 7));
        circle(r, x, y, size, withAlpha(c, (0.25f + 0.6f * flick) * std::min(1.f, heat * 1.5f)));
    }
    const float draw = easeInOut(ramp(t, 0.6, 3.2));
    const float glow = 0.6f + 0.4f * std::sin(static_cast<float>(t) * 3) + (t > kHit ? 1.2f * std::exp(-(t - kHit) * 3) : 0);
    glowingLogo(r, kCx, kCy, 230, draw, {255, 226, 210, 255}, {255, 40, 30, 255}, glow);
    if (t >= kHit) {
        const float fl = static_cast<float>(std::exp(-(t - kHit) * 7));
        r.fillRect({0, 0, kScreenW, kScreenH}, withAlpha({255, 120, 90, 255}, 0.45f * fl), 0);
        ringPulse(r, kCx, kCy, 240 + static_cast<float>(t - kHit) * 900, 10, withAlpha({255, 90, 60, 255}, 0.6f * fl));
    }
}

// ---------------------------------------------------------------- era 1: flat polygons
void BootSequence::drawPolygon(IRenderer& r, double t) const {
    r.fillRect({0, 0, kScreenW, kScreenH}, {12, 12, 16, 255}, 0);
    const float intro = ramp(t, 0.0, 0.35);
    if (t < 0.6) r.fillRect({0, 0, kScreenW, kScreenH}, withAlpha({255, 255, 255, 255}, (1 - intro) * 0.9f), 0);
    // Extruded star: 10 outline points (outer/inner radius), depth 60.
    const float spin = t < kHit ? static_cast<float>(4.2 * (1 - std::pow(1 - t / kHit, 2.5))) * 2 * kPi : 0.f;
    const float tilt = 0.35f * (1 - ramp(t, kHit - 0.6, kHit));
    const float scale = 0.3f + 0.7f * easeOut(ramp(t, 0.3, 2.4));
    struct V { float x, y, z; };
    std::vector<V> front, back;
    for (int i = 0; i < 10; ++i) {
        const float a = kPi / 2 + i * kPi / 5;
        const float rad = (i % 2 ? 95.f : 250.f) * scale;
        front.push_back({rad * std::cos(a), rad * std::sin(a), -38 * scale});
        back.push_back({rad * std::cos(a), rad * std::sin(a), 38 * scale});
    }
    auto xf = [&](V v) {
        const float cy = std::cos(spin), sy = std::sin(spin), ct = std::cos(tilt), st = std::sin(tilt);
        V a{v.x * cy + v.z * sy, v.y, -v.x * sy + v.z * cy};
        return V{a.x, a.y * ct - a.z * st, a.y * st + a.z * ct};
    };
    auto proj = [&](V v) { const float d = 900.f / (900.f + v.z); return IRenderer::Pt{kCx + v.x * d, kCy + v.y * d}; };
    struct Face { std::vector<IRenderer::Pt> pts; float depth; Color c; };
    std::vector<Face> faces;
    const V light{0.3f, -0.5f, -0.8f};
    auto shade = [&](V n, Color base) {
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z) + 1e-6f;
        const float d = std::max(0.f, -(n.x * light.x + n.y * light.y + n.z * light.z) / len);
        const float k = 0.25f + 0.75f * d;
        // Posterised: 5 levels, like low-colour hardware.
        const float q = std::round(k * 5) / 5;
        return Color{static_cast<uint8_t>(base.r * q), static_cast<uint8_t>(base.g * q), static_cast<uint8_t>(base.b * q), 255};
    };
    std::vector<V> F, B;
    for (int i = 0; i < 10; ++i) { F.push_back(xf(front[i])); B.push_back(xf(back[i])); }
    auto normal = [](V a, V b, V c) {
        V u{b.x - a.x, b.y - a.y, b.z - a.z}, w{c.x - a.x, c.y - a.y, c.z - a.z};
        return V{u.y * w.z - u.z * w.y, u.z * w.x - u.x * w.z, u.x * w.y - u.y * w.x};
    };
    for (int i = 0; i < 10; ++i) {
        const int j = (i + 1) % 10;
        V n = normal(F[i], F[j], B[j]);
        Face fc{{proj(F[i]), proj(F[j]), proj(B[j]), proj(B[i])}, (F[i].z + F[j].z + B[i].z + B[j].z) / 4,
                shade(n, {230, 40, 40, 255})};
        faces.push_back(fc);
    }
    // Star caps as 5 triangles (+ centre pentagon) each, so they stay convex.
    auto cap = [&](const std::vector<V>& P, Color base, bool isFront) {
        V c{0, 0, 0};
        for (const auto& p : P) { c.x += p.x / 10; c.y += p.y / 10; c.z += p.z / 10; }
        V n = normal(P[0], P[2], P[4]);
        if (!isFront) n = {-n.x, -n.y, -n.z};
        const Color col = shade(n, base);
        for (int i = 0; i < 10; ++i) {
            const int j = (i + 1) % 10;
            faces.push_back({{proj(c), proj(P[i]), proj(P[j])}, c.z, col});
        }
    };
    cap(F, {255, 200, 60, 255}, true);
    cap(B, {255, 200, 60, 255}, false);
    std::sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.depth > b.depth; });
    for (const auto& fc : faces) r.fillPath({fc.pts}, fc.c);
    if (t >= kHit) {
        const float fl = static_cast<float>(std::exp(-(t - kHit) * 6));
        r.fillRect({0, 0, kScreenW, kScreenH}, withAlpha({255, 255, 255, 255}, 0.5f * fl), 0);
    }
}

// ---------------------------------------------------------------- era 2: fog
void BootSequence::drawFog(IRenderer& r, double t) const {
    r.fillRect({0, 0, kScreenW, kScreenH}, {2, 4, 14, 255}, 0);
    const float in = ramp(t, 0, 1.5);
    for (int i = 0; i < 9; ++i) {   // slow fog banks
        const float x = std::fmod(hash01(i, 11) * 2400.f + static_cast<float>(t) * (20 + 30 * hash01(i, 12)), 2600.f) - 340;
        const float y = 200 + hash01(i, 13) * 700;
        circle(r, x, y, 260 + 200 * hash01(i, 14), withAlpha({40, 70, 160, 255}, 0.05f * in));
    }
    for (int i = 0; i < 160; ++i) {   // particles flying toward the viewer
        const float z = 1.f - std::fmod(static_cast<float>(t) * (0.12f + 0.1f * hash01(i, 21)) + hash01(i, 22), 1.f);
        const float d = 1.f / (0.05f + z);
        const float x = kCx + (hash01(i, 23) - 0.5f) * 260 * d;
        const float y = kCy + (hash01(i, 24) - 0.5f) * 160 * d;
        const float a = (1 - z) * in * 0.8f;
        circle(r, x, y, 0.8f + 1.6f * d * 0.25f, withAlpha({170, 200, 255, 255}, a));
    }
    const float logo = easeInOut(ramp(t, 1.2, 3.3));
    const Color core = mix({2, 4, 14, 255}, {225, 235, 255, 255}, logo);
    glowingLogo(r, kCx, kCy, 220, 1.0f, core, {90, 140, 255, 255}, logo * (1 + (t > kHit ? 1.5f * static_cast<float>(std::exp(-(t - kHit) * 3)) : 0)));
}

// ---------------------------------------------------------------- era 3: waves
void BootSequence::drawWaves(IRenderer& r, double t) const {
    if (!r.useCachedLayer(0xB007000000000003ull)) {
        r.gradientRect({0, 0, kScreenW, kScreenH}, {40, 14, 10, 255}, {8, 4, 10, 255}, 0);
        r.endLayer();
    }
    const float in = ramp(t, 0, 1.2);
    for (int band = 0; band < 4; ++band) {
        std::vector<IRenderer::Pt> poly;
        const float base = 600 + band * 26.f, amp = 70 + band * 18.f, thick = 18 + band * 10.f;
        const float sp = 0.5f + band * 0.17f, k = 0.0032f + band * 0.0006f;
        for (int x = -20; x <= 1940; x += 40) {
            const float y = base + amp * std::sin(x * k + static_cast<float>(t) * sp + band) +
                            30 * std::sin(x * k * 2.3f - static_cast<float>(t) * sp * 0.7f);
            poly.push_back({static_cast<float>(x), y});
        }
        for (int x = 1940; x >= -20; x -= 40) {
            const float y = base + thick + amp * std::sin(x * k + static_cast<float>(t) * sp + band + 0.35f) +
                            30 * std::sin(x * k * 2.3f - static_cast<float>(t) * sp * 0.7f + 0.2f);
            poly.push_back({static_cast<float>(x), y});
        }
        r.fillPath({poly}, withAlpha({255, 170, 110, 255}, (0.08f + 0.05f * band) * in));
    }
    const float logo = easeInOut(ramp(t, 1.0, 3.2));
    glowingLogo(r, kCx, kCy - 40, 200, logo, {255, 236, 220, 255}, {255, 140, 60, 255}, logo);
}

// ---------------------------------------------------------------- era 4: light trails
void BootSequence::drawTrails(IRenderer& r, double t) const {
    r.fillRect({0, 0, kScreenW, kScreenH}, {3, 6, 16, 255}, 0);
    const float conv = easeInOut(ramp(t, 0.2, kHit));
    for (int i = 0; i < 70; ++i) {
        const float ang = hash01(i, 31) * 2 * kPi;
        const float dist = (1 - conv) * (500 + 900 * hash01(i, 32)) + 240;
        const float len = 60 + 220 * hash01(i, 33) * (1 - conv * 0.7f);
        const float w = 1.2f + 2.5f * hash01(i, 34);
        const float x0 = kCx + std::cos(ang) * dist, y0 = kCy + std::sin(ang) * dist;
        const float x1 = kCx + std::cos(ang) * (dist + len), y1 = kCy + std::sin(ang) * (dist + len);
        const float nx = -std::sin(ang) * w, ny = std::cos(ang) * w;
        const float a = (0.3f + 0.6f * hash01(i, 35)) * ramp(t, 0, 0.6) * (t < kHit ? 1.f : static_cast<float>(std::exp(-(t - kHit) * 4)));
        r.fillPath({{{x0 + nx, y0 + ny}, {x1 + nx * 0.2f, y1 + ny * 0.2f}, {x1 - nx * 0.2f, y1 - ny * 0.2f}, {x0 - nx, y0 - ny}}},
                   withAlpha(mix({90, 200, 255, 255}, {230, 250, 255, 255}, hash01(i, 36)), a));
    }
    const float logo = ramp(t, kHit - 0.3, kHit);
    glowingLogo(r, kCx, kCy, 220, 1.0f, mix({3, 6, 16, 255}, {230, 248, 255, 255}, logo), {60, 170, 255, 255}, logo);
    if (t >= kHit) {
        for (int k = 0; k < 3; ++k) {
            const float tt = static_cast<float>(t - kHit) - k * 0.18f;
            if (tt < 0) continue;
            ringPulse(r, kCx, kCy, 235 + tt * 700, 6, withAlpha({120, 210, 255, 255}, 0.5f * std::exp(-tt * 2.5f)));
        }
    }
}

// ---------------------------------------------------------------- era 5: gleam
void BootSequence::drawGleam(IRenderer& r, double t) const {
    r.fillRect({0, 0, kScreenW, kScreenH}, {0, 0, 0, 255}, 0);
    const float line = easeOut(ramp(t, 0.3, 1.8));
    const float open = easeInOut(ramp(t, 1.8, kHit));
    const float w = 1600 * line;
    const float h = 2 + 420 * open;
    const float fadeBand = 1 - 0.85f * open;
    for (int k = 0; k < 4; ++k)
        r.fillRect({kCx - w / 2 - k * 20, kCy - h / 2 - k * 6, w + k * 40, h + k * 12},
                   withAlpha({255, 255, 255, 255}, (0.5f - k * 0.11f) * fadeBand * line), h / 2 + k * 6);
    const float logo = easeOut(ramp(t, 2.6, kHit + 0.2));
    glowingLogo(r, kCx, kCy, 210, 1.0f, mix({0, 0, 0, 255}, {255, 255, 255, 255}, logo), {255, 255, 255, 255}, logo * 0.6f);
}

}  // namespace henu
