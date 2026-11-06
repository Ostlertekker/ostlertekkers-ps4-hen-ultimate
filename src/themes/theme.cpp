// OSTLERTEKKERS HEN - themes/theme.cpp
#include "themes/theme.h"

#include <cstdio>
#include <set>

namespace henu {

namespace {
int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool validId(const std::string& s) {
    if (s.size() < 2 || s.size() > 32) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (i > 0 && (c == '_' || c == '-'));
        if (!ok) return false;
    }
    return true;
}

// File names for images/sounds: no paths, fixed extension.
bool validAsset(const std::string& s, const char* ext) {
    if (s.empty() || s.size() > 64) return false;
    std::string e = std::string(".") + ext;
    if (s.size() <= e.size() || s.compare(s.size() - e.size(), e.size(), e) != 0) return false;
    if (s[0] == '.') return false;
    for (char c : s) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '-' || c == '.';
        if (!ok) return false;
    }
    return s.find("..") == std::string::npos;
}

float clampf(double v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return static_cast<float>(v);
}

using R = Result<Theme>;
}  // namespace

bool parseColor(const std::string& s, Color& out) {
    if ((s.size() != 7 && s.size() != 9) || s[0] != '#') return false;
    int v[8];
    for (size_t i = 1; i < s.size(); ++i) {
        v[i - 1] = hexVal(s[i]);
        if (v[i - 1] < 0) return false;
    }
    out.r = static_cast<uint8_t>(v[0] * 16 + v[1]);
    out.g = static_cast<uint8_t>(v[2] * 16 + v[3]);
    out.b = static_cast<uint8_t>(v[4] * 16 + v[5]);
    out.a = s.size() == 9 ? static_cast<uint8_t>(v[6] * 16 + v[7]) : 255;
    return true;
}

std::string colorToHex(const Color& c, bool withAlpha) {
    char buf[10];
    if (withAlpha || c.a != 255) std::snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", c.r, c.g, c.b, c.a);
    else std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", c.r, c.g, c.b);
    return buf;
}

Theme defaultTheme() {
    Theme t;
    t.id = "builtin-fallback";
    t.nameDe = "Standard";
    t.nameEn = "Default";
    t.category = "dark";
    t.tileGradients = {
        {{73, 59, 145, 255}, {38, 35, 77, 255}},
        {{28, 92, 131, 255}, {24, 42, 73, 255}},
        {{129, 60, 120, 255}, {61, 36, 78, 255}},
        {{35, 109, 105, 255}, {24, 59, 67, 255}},
        {{128, 96, 46, 255}, {64, 51, 31, 255}},
        {{69, 77, 101, 255}, {37, 45, 64, 255}},
    };
    return t;
}

Result<Theme> parseTheme(const Json& j) {
    if (!j.isObject()) return R::fail(ErrorCode::ParseError, "theme must be an object");
    static const std::set<std::string> allowed = {
        "schemaVersion", "id", "name", "author", "license", "category", "mode", "colors",
        "tileGradients", "surfaceOpacity", "cornerRadius", "background", "animation", "sounds"};
    for (const auto& kv : j.asObject())
        if (!allowed.count(kv.first)) return R::fail(ErrorCode::ParseError, "unknown field: " + kv.first);

    if (j.get("schemaVersion").asInt(-1) != 1) return R::fail(ErrorCode::ParseError, "schemaVersion must be 1");

    Theme t = defaultTheme();
    t.id = j.get("id").asString();
    if (!validId(t.id)) return R::fail(ErrorCode::ParseError, "invalid id");

    const Json& name = j.get("name");
    t.nameEn = name.get("en").asString();
    t.nameDe = name.get("de").asString();
    if (t.nameEn.empty() || t.nameEn.size() > 40 || t.nameDe.size() > 40)
        return R::fail(ErrorCode::ParseError, "name.en required (max 40 chars)");
    t.author = j.get("author").asString();
    t.license = j.get("license").asString();

    static const std::set<std::string> cats = {"ps5", "dark", "neon", "minimal", "gaming", "light", "custom"};
    t.category = j.get("category").asString();
    if (!cats.count(t.category)) return R::fail(ErrorCode::ParseError, "invalid category");

    const std::string mode = j.get("mode").asString();
    if (mode != "dark" && mode != "light") return R::fail(ErrorCode::ParseError, "mode must be dark|light");
    t.dark = mode == "dark";

    const Json& c = j.get("colors");
    if (!c.isObject()) return R::fail(ErrorCode::ParseError, "colors missing");
    struct Slot { const char* key; Color* dst; bool required; };
    Slot slots[] = {
        {"background", &t.background, true}, {"surface", &t.surface, true},
        {"surfaceAlt", &t.surfaceAlt, true}, {"text", &t.text, true},
        {"textMuted", &t.textMuted, true},   {"accent", &t.accent, true},
        {"accent2", &t.accent2, true},       {"focus", &t.focus, true},
        {"line", &t.line, true},             {"ok", &t.ok, false},
        {"warn", &t.warn, false},            {"error", &t.error, false},
    };
    for (const auto& kv : c.asObject()) {
        bool known = false;
        for (const auto& s : slots) if (kv.first == s.key) known = true;
        if (!known) return R::fail(ErrorCode::ParseError, "unknown color: " + kv.first);
    }
    for (auto& s : slots) {
        const Json& v = c.get(s.key);
        if (v.isNull()) {
            if (s.required) return R::fail(ErrorCode::ParseError, std::string("missing color: ") + s.key);
            continue;
        }
        if (!parseColor(v.asString(), *s.dst))
            return R::fail(ErrorCode::ParseError, std::string("bad color: ") + s.key);
    }

    if (j.has("tileGradients")) {
        const auto& arr = j.get("tileGradients").asArray();
        if (arr.size() > 16) return R::fail(ErrorCode::ParseError, "too many tileGradients");
        t.tileGradients.clear();
        for (const auto& g : arr) {
            const auto& pair = g.asArray();
            Color a, b;
            if (pair.size() != 2 || !parseColor(pair[0].asString(), a) || !parseColor(pair[1].asString(), b))
                return R::fail(ErrorCode::ParseError, "bad tileGradients entry");
            t.tileGradients.emplace_back(a, b);
        }
        if (t.tileGradients.empty()) t.tileGradients = defaultTheme().tileGradients;
    }

    if (j.has("surfaceOpacity")) t.surfaceOpacity = clampf(j.get("surfaceOpacity").asNumber(1), 0.2f, 1.0f);
    if (j.has("cornerRadius")) t.cornerRadius = clampf(j.get("cornerRadius").asNumber(18), 0, 48);

    const Json& bg = j.get("background");
    if (bg.isObject()) {
        std::string type = bg.get("type").asString("gradient");
        if (type != "solid" && type != "gradient" && type != "image")
            return R::fail(ErrorCode::ParseError, "bad background.type");
        t.backgroundType = type;
        if (type == "image") {
            t.backgroundImage = bg.get("image").asString();
            if (!validAsset(t.backgroundImage, "png"))
                return R::fail(ErrorCode::ParseError, "background.image must be a plain .png file name");
        }
        t.backgroundDim = clampf(bg.get("dim").asNumber(0.4), 0, 1);
    }

    const Json& an = j.get("animation");
    if (an.isObject()) {
        t.animSpeed = clampf(an.get("speed").asNumber(1), 0.25f, 2.0f);
        t.focusScale = clampf(an.get("focusScale").asNumber(1.05), 1.0f, 1.15f);
        std::string amb = an.get("ambient").asString("orb");
        if (amb != "none" && amb != "orb" && amb != "particles" && amb != "waves")
            return R::fail(ErrorCode::ParseError, "bad animation.ambient");
        t.ambient = amb;
    }

    const Json& snd = j.get("sounds");
    if (snd.isObject()) {
        struct S { const char* k; std::string* d; } ss[] = {
            {"move", &t.soundMove}, {"select", &t.soundSelect}, {"back", &t.soundBack}};
        for (auto& s : ss) {
            if (!snd.has(s.k)) continue;
            *s.d = snd.get(s.k).asString();
            if (!validAsset(*s.d, "wav"))
                return R::fail(ErrorCode::ParseError, std::string("sounds.") + s.k + " must be a plain .wav file name");
        }
    }
    return R::ok(t);
}

Json themeToJson(const Theme& t) {
    Json j = Json::object();
    j.set("schemaVersion", Json::number(1));
    j.set("id", Json::string(t.id));
    Json name = Json::object();
    if (!t.nameDe.empty()) name.set("de", Json::string(t.nameDe));
    name.set("en", Json::string(t.nameEn));
    j.set("name", name);
    if (!t.author.empty()) j.set("author", Json::string(t.author));
    if (!t.license.empty()) j.set("license", Json::string(t.license));
    j.set("category", Json::string(t.category));
    j.set("mode", Json::string(t.dark ? "dark" : "light"));
    Json c = Json::object();
    c.set("background", Json::string(colorToHex(t.background)));
    c.set("surface", Json::string(colorToHex(t.surface)));
    c.set("surfaceAlt", Json::string(colorToHex(t.surfaceAlt)));
    c.set("text", Json::string(colorToHex(t.text)));
    c.set("textMuted", Json::string(colorToHex(t.textMuted)));
    c.set("accent", Json::string(colorToHex(t.accent)));
    c.set("accent2", Json::string(colorToHex(t.accent2)));
    c.set("focus", Json::string(colorToHex(t.focus)));
    c.set("line", Json::string(colorToHex(t.line)));
    c.set("ok", Json::string(colorToHex(t.ok)));
    c.set("warn", Json::string(colorToHex(t.warn)));
    c.set("error", Json::string(colorToHex(t.error)));
    j.set("colors", c);
    Json g = Json::array();
    for (const auto& p : t.tileGradients) {
        Json pair = Json::array();
        pair.push(Json::string(colorToHex({p.first.r, p.first.g, p.first.b, 255})));
        pair.push(Json::string(colorToHex({p.second.r, p.second.g, p.second.b, 255})));
        g.push(pair);
    }
    j.set("tileGradients", g);
    j.set("surfaceOpacity", Json::number(t.surfaceOpacity));
    j.set("cornerRadius", Json::number(t.cornerRadius));
    Json bg = Json::object();
    bg.set("type", Json::string(t.backgroundType));
    if (t.backgroundType == "image") bg.set("image", Json::string(t.backgroundImage));
    bg.set("dim", Json::number(t.backgroundDim));
    j.set("background", bg);
    Json an = Json::object();
    an.set("speed", Json::number(t.animSpeed));
    an.set("focusScale", Json::number(t.focusScale));
    an.set("ambient", Json::string(t.ambient));
    j.set("animation", an);
    return j;
}

}  // namespace henu
