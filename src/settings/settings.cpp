// OSTLERTEKKERS HEN - settings/settings.cpp
#include "settings/settings.h"

#include <cmath>
#include <initializer_list>

namespace henu {

namespace {
void note(std::vector<std::string>* w, const std::string& msg) { if (w) w->push_back(msg); }

std::string pickEnum(const Json& j, const char* key, std::initializer_list<const char*> allowed,
                     const std::string& def, std::vector<std::string>* w) {
    const Json& v = j.get(key);
    if (v.isNull()) return def;
    if (v.isString()) {
        for (const char* a : allowed) if (v.asString() == a) return v.asString();
    }
    note(w, std::string(key) + ": invalid value, using default");
    return def;
}

int pickInt(const Json& j, const char* key, int lo, int hi, int def, std::vector<std::string>* w) {
    const Json& v = j.get(key);
    if (v.isNull()) return def;
    if (!v.isNumber() || v.asNumber() < lo || v.asNumber() > hi || std::floor(v.asNumber()) != v.asNumber()) {
        note(w, std::string(key) + ": out of range, using default");
        return def;
    }
    return v.asInt(def);
}

float pickFloat(const Json& j, const char* key, float lo, float hi, float def, std::vector<std::string>* w) {
    const Json& v = j.get(key);
    if (v.isNull()) return def;
    if (!v.isNumber() || v.asNumber() < lo || v.asNumber() > hi) {
        note(w, std::string(key) + ": out of range, using default");
        return def;
    }
    return static_cast<float>(v.asNumber());
}

bool pickBool(const Json& j, const char* key, bool def, std::vector<std::string>* w) {
    const Json& v = j.get(key);
    if (v.isNull()) return def;
    if (!v.isBool()) { note(w, std::string(key) + ": not a boolean, using default"); return def; }
    return v.asBool();
}

bool validThemeId(const std::string& s) {
    if (s.size() < 2 || s.size() > 32) return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    return true;
}
}  // namespace

Settings settingsFromJson(const Json& j, std::vector<std::string>* w) {
    Settings d;  // defaults
    Settings s;
    if (!j.isObject()) { note(w, "settings: not an object, using defaults"); return d; }
    int ver = j.get("schemaVersion").asInt(0);
    if (ver != 1) note(w, "schemaVersion " + std::to_string(ver) + " unknown, reading known fields only");

    s.language = pickEnum(j, "language", {"de", "en"}, d.language, w);
    s.themeId = j.get("themeId").asString(d.themeId);
    if (!validThemeId(s.themeId)) { note(w, "themeId: invalid, using default"); s.themeId = d.themeId; }
    for (const auto& f : j.get("favoriteThemes").asArray())
        if (f.isString() && validThemeId(f.asString()) && s.favoriteThemes.size() < 64)
            s.favoriteThemes.push_back(f.asString());
    s.randomTheme = pickEnum(j, "randomTheme", {"off", "on_start", "daily"}, d.randomTheme, w);
    s.modeOverride = pickEnum(j, "modeOverride", {"theme", "dark", "light"}, d.modeOverride, w);
    s.tileSize = pickEnum(j, "tileSize", {"small", "medium", "large"}, d.tileSize, w);
    s.animations = pickEnum(j, "animations", {"full", "reduced", "off"}, d.animations, w);

    const Json& snd = j.get("sound");
    s.soundEnabled = pickBool(snd, "enabled", d.soundEnabled, w);
    s.volume = pickInt(snd, "volume", 0, 100, d.volume, w);

    const Json& c = j.get("controller");
    s.confirmButton = pickEnum(c, "confirmButton", {"cross", "circle"}, d.confirmButton, w);
    s.stickDeadzone = pickFloat(c, "stickDeadzone", 0.05f, 0.6f, d.stickDeadzone, w);
    s.repeatDelayMs = pickInt(c, "repeatDelayMs", 150, 800, d.repeatDelayMs, w);
    s.repeatRateMs = pickInt(c, "repeatRateMs", 40, 300, d.repeatRateMs, w);

    s.allowDownloads = pickBool(j.get("network"), "allowDownloads", d.allowDownloads, w);
    s.allowUnverifiedTls = pickBool(j.get("network"), "allowUnverifiedTls", d.allowUnverifiedTls, w);
    s.cheatDbSource = pickEnum(j.get("cheatDb"), "source", {"none", "official_goldhen_repo", "usb_zip"},
                               d.cheatDbSource, w);
    s.updateCheckOnStart = pickBool(j.get("update"), "checkOnStart", d.updateCheckOnStart, w);
    const Json& b = j.get("backup");
    s.backupTarget = pickEnum(b, "target", {"internal", "usb"}, d.backupTarget, w);
    s.backupKeep = pickInt(b, "keep", 1, 20, d.backupKeep, w);
    const Json& boot = j.get("boot");
    s.bootAnimation = pickEnum(boot, "animation", {"inferno", "era1", "era2", "era3", "era4", "era5", "random", "off"},
                               d.bootAnimation, w);
    s.bootSound = pickEnum(boot, "sound", {"builtin", "custom", "off"}, d.bootSound, w);
    s.launchAnimation = pickEnum(boot, "launchAnimation", {"inferno", "era1", "era2", "era3", "era4", "era5", "random", "off"},
                                 d.launchAnimation, w);
    const Json& look = j.get("appearance");
    s.uiStyle = pickEnum(look, "uiStyle", {"default", "ps5", "ps3"}, d.uiStyle, w);
    s.backgroundFx = pickEnum(look, "backgroundFx", {"theme", "galaxy", "wave", "aurora"}, d.backgroundFx, w);
    s.rgbBorders = pickBool(look, "rgbBorders", d.rgbBorders, w);
    return s;
}

Json settingsToJson(const Settings& s) {
    Json j = Json::object();
    j.set("schemaVersion", Json::number(1));
    j.set("language", Json::string(s.language));
    j.set("themeId", Json::string(s.themeId));
    Json fav = Json::array();
    for (const auto& f : s.favoriteThemes) fav.push(Json::string(f));
    j.set("favoriteThemes", fav);
    j.set("randomTheme", Json::string(s.randomTheme));
    j.set("modeOverride", Json::string(s.modeOverride));
    j.set("tileSize", Json::string(s.tileSize));
    j.set("animations", Json::string(s.animations));
    Json snd = Json::object();
    snd.set("enabled", Json::boolean(s.soundEnabled));
    snd.set("volume", Json::number(s.volume));
    j.set("sound", snd);
    Json c = Json::object();
    c.set("confirmButton", Json::string(s.confirmButton));
    c.set("stickDeadzone", Json::number(std::round(s.stickDeadzone * 100.0) / 100.0));
    c.set("repeatDelayMs", Json::number(s.repeatDelayMs));
    c.set("repeatRateMs", Json::number(s.repeatRateMs));
    j.set("controller", c);
    Json n = Json::object();
    n.set("allowDownloads", Json::boolean(s.allowDownloads));
    n.set("allowUnverifiedTls", Json::boolean(s.allowUnverifiedTls));
    j.set("network", n);
    Json cd = Json::object();
    cd.set("source", Json::string(s.cheatDbSource));
    j.set("cheatDb", cd);
    Json u = Json::object();
    u.set("checkOnStart", Json::boolean(s.updateCheckOnStart));
    j.set("update", u);
    Json b = Json::object();
    b.set("target", Json::string(s.backupTarget));
    b.set("keep", Json::number(s.backupKeep));
    j.set("backup", b);
    Json boot = Json::object();
    boot.set("animation", Json::string(s.bootAnimation));
    boot.set("sound", Json::string(s.bootSound));
    boot.set("launchAnimation", Json::string(s.launchAnimation));
    j.set("boot", boot);
    Json look = Json::object();
    look.set("uiStyle", Json::string(s.uiStyle));
    look.set("backgroundFx", Json::string(s.backgroundFx));
    look.set("rgbBorders", Json::boolean(s.rgbBorders));
    j.set("appearance", look);
    return j;
}

SettingsStore::SettingsStore(IFileSystem& fs, std::string dir) : fs_(fs), dir_(std::move(dir)) {}

Settings SettingsStore::load(std::string* source) {
    const std::string main = path();
    const std::string bak = main + ".bak";
    for (const auto& candidate : {main, bak}) {
        if (!fs_.exists(candidate)) continue;
        auto text = fs_.readFile(candidate, 256 * 1024);
        if (!text) continue;
        auto j = parseJson(text.value());
        if (!j || !j.value().isObject()) continue;
        if (source) *source = (candidate == main) ? "file" : "backup";
        return settingsFromJson(j.value());
    }
    if (source) *source = "defaults";
    return Settings();
}

Status SettingsStore::save(const Settings& s) {
    const std::string main = path();
    const std::string tmp = main + ".tmp";
    const std::string bak = main + ".bak";
    if (!fs_.exists(dir_)) {
        Status st = fs_.makeDir(dir_);
        if (!st) return st;
    }
    const std::string data = settingsToJson(s).dump(2);
    Status st = fs_.writeFile(tmp, data);
    if (!st) return st;
    auto back = fs_.readFile(tmp, 256 * 1024);
    if (!back || back.value() != data || !parseJson(back.value())) {
        fs_.remove(tmp);
        return Status::fail(ErrorCode::IoError, "settings verification failed");
    }
    if (fs_.exists(main)) {
        if (fs_.exists(bak)) fs_.remove(bak);
        st = fs_.rename(main, bak);
        if (!st) return st;
    }
    return fs_.rename(tmp, main);
}

Status SettingsStore::resetToDefaults() { return save(Settings()); }

}  // namespace henu
