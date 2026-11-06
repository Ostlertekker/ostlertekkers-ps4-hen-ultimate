// OSTLERTEKKERS HEN - tests for JSON, i18n, themes, settings.
#include <string>

#include "check.h"
#include "core/json.h"
#include "i18n/i18n.h"
#include "platform/mem_fs.h"
#include "settings/settings.h"
#include "themes/theme.h"

using namespace henu;

static void testJson() {
    auto r = parseJson(R"({"a":1,"b":[true,false,null],"c":"x\"y\u00e4\ud83d\ude00","d":-1.5e2,"e":{}})");
    CHECK(r.isOk());
    const Json& j = r.value();
    CHECK(j.get("a").asInt() == 1);
    CHECK(j.get("b").asArray().size() == 3 && j.get("b").asArray()[0].asBool());
    CHECK(j.get("c").asString() == "x\"y\xC3\xA4\xF0\x9F\x98\x80");
    CHECK(j.get("d").asNumber() == -150.0);
    CHECK(j.get("e").isObject() && j.get("missing").isNull());

    // round trip
    auto again = parseJson(j.dump(2));
    CHECK(again.isOk() && again.value().get("c").asString() == j.get("c").asString());
    CHECK(parseJson(j.dump(0)).isOk());
    CHECK(Json::number(3).dump(0) == "3");
    CHECK(Json::string("a\nb").dump(0) == "\"a\\nb\"");

    // malformed input must fail, never crash
    const char* bad[] = {"", "{", "[1,]", "{\"a\":}", "{'a':1}", "01", "1.", "-", "\"abc", "nul",
                         "[1 2]", "{\"a\" 1}", "\"\\x\"", "\"\\ud800\"", "\"\\udc00\"", "{} x",
                         "\"tab\there\""};
    for (const char* b : bad) CHECK(!parseJson(b).isOk());

    // depth limit
    std::string deep(100, '[');
    deep += std::string(100, ']');
    CHECK(!parseJson(deep).isOk());
    JsonLimits big;
    big.maxDepth = 200;
    CHECK(parseJson(deep, big).isOk());
    // size limit
    JsonLimits tiny;
    tiny.maxBytes = 4;
    CHECK(!parseJson("[1,2,3]", tiny).isOk());
    // BOM
    CHECK(parseJson("\xEF\xBB\xBF{\"a\":1}").isOk());
    // duplicate keys: last wins
    CHECK(parseJson(R"({"a":1,"a":2})").value().get("a").asInt() == 2);
    // asInt guards
    CHECK(Json::number(1e20).asInt(-1) == -1);
}

static void testI18n() {
    I18n i;
    CHECK(i.loadLanguage("en", R"({"_meta":{"x":1},"hello":"Hello","only.en":"EN","n":"{0} of {1}"})").isOk());
    CHECK(i.loadLanguage("de", R"({"hello":"Hallo","n":"{0} von {1}"})").isOk());
    i.setLanguage("de");
    CHECK(i.t("hello") == "Hallo");
    CHECK(i.t("only.en") == "EN");          // fallback to English
    CHECK(i.t("nope.key") == "nope.key");   // fallback to key
    CHECK(i.fmt("n", {"3", "5"}) == "3 von 5");
    i.setLanguage("en");
    CHECK(i.fmt("n", {"3"}) == "3 of ");
    CHECK(!i.loadLanguage("xx", "{bad").isOk());
    CHECK(!i.loadLanguage("xx", R"({"a":1})").isOk());
}

static const char* kTheme = R"({
  "schemaVersion": 1, "id": "test-theme", "name": {"de": "Test", "en": "Test EN"},
  "category": "neon", "mode": "dark",
  "colors": {"background":"#000000","surface":"#111111","surfaceAlt":"#222222","text":"#ffffff",
             "textMuted":"#999999","accent":"#ff00ff","accent2":"#00ffff","focus":"#ffffff","line":"#ffffff17"},
  "tileGradients": [["#ff0000","#00ff00"]],
  "surfaceOpacity": 5, "cornerRadius": 12,
  "background": {"type":"image","image":"bg.png","dim":0.5},
  "animation": {"speed":1.5,"focusScale":1.1,"ambient":"particles"},
  "sounds": {"move":"move.wav"}
})";

static void testTheme() {
    auto t = parseTheme(parseJson(kTheme).value());
    CHECK(t.isOk());
    if (!t.isOk()) { std::printf("  theme error: %s\n", t.error().message.c_str()); return; }
    CHECK(t.value().id == "test-theme" && t.value().name("de") == "Test" && t.value().name("en") == "Test EN");
    CHECK(t.value().accent.r == 255 && t.value().accent.g == 0 && t.value().line.a == 0x17);
    CHECK(t.value().surfaceOpacity == 1.0f);   // clamped
    CHECK(t.value().tileGradients.size() == 1);
    CHECK(t.value().backgroundImage == "bg.png" && t.value().soundMove == "move.wav");

    // round trip
    auto back = parseTheme(themeToJson(t.value()));
    CHECK(back.isOk() && back.value().accent2.b == 255);

    auto mutate = [](const std::string& from, const std::string& to) {
        std::string s = kTheme;
        size_t p = s.find(from);
        if (p != std::string::npos) s.replace(p, from.size(), to);
        return parseTheme(parseJson(s).value());
    };
    CHECK(!mutate("\"bg.png\"", "\"../../etc/x.png\"").isOk());   // path traversal
    CHECK(!mutate("\"bg.png\"", "\"bg.jpg\"").isOk());
    CHECK(!mutate("\"move.wav\"", "\"/data/x.wav\"").isOk());
    CHECK(!mutate("\"test-theme\"", "\"Bad Id\"").isOk());
    CHECK(!mutate("\"neon\"", "\"weird\"").isOk());
    CHECK(!mutate("\"#ff00ff\"", "\"red\"").isOk());
    CHECK(!mutate("\"schemaVersion\": 1", "\"schemaVersion\": 2").isOk());
    CHECK(!mutate("\"cornerRadius\"", "\"cornerRadiuss\"").isOk());   // unknown field
    CHECK(!parseTheme(Json::array()).isOk());

    Color c;
    CHECK(parseColor("#0a0B0c", c) && c.r == 10 && c.g == 11 && c.b == 12 && c.a == 255);
    CHECK(!parseColor("#abc", c) && !parseColor("0a0b0c", c) && !parseColor("#gg0000", c));
    CHECK(colorToHex({1, 2, 3, 255}) == "#010203" && colorToHex({1, 2, 3, 4}) == "#01020304");
}

static void testSettings() {
    // defaults + validation
    std::vector<std::string> warn;
    Settings s = settingsFromJson(parseJson(R"({"schemaVersion":1,"language":"fr","themeId":"../x",
        "controller":{"confirmButton":"circle","repeatDelayMs":5},"sound":{"volume":42}})").value(), &warn);
    CHECK(s.language == "de" && s.themeId == "midnight");   // invalid -> defaults
    CHECK(s.confirmButton == "circle" && s.repeatDelayMs == 350 && s.volume == 42);
    CHECK(warn.size() == 3);

    // round trip
    Settings a;
    a.language = "en";
    a.themeId = "aurora";
    a.favoriteThemes = {"aurora", "crimson"};
    a.animations = "reduced";
    a.stickDeadzone = 0.3f;
    Settings b = settingsFromJson(settingsToJson(a));
    CHECK(b.language == "en" && b.themeId == "aurora" && b.favoriteThemes.size() == 2 &&
          b.animations == "reduced" && b.stickDeadzone > 0.29f && b.stickDeadzone < 0.31f);

    // store: defaults -> save -> load -> backup
    MemFileSystem fs;
    SettingsStore store(fs, "/data/OSTLERTEKKERS_HEN");
    std::string src;
    CHECK(store.load(&src).language == "de" && src == "defaults");
    CHECK(store.save(a).isOk());
    CHECK(fs.dirs.count("/data/OSTLERTEKKERS_HEN") == 1);
    CHECK(store.load(&src).themeId == "aurora" && src == "file");
    a.themeId = "crimson";
    CHECK(store.save(a).isOk());
    CHECK(fs.files.count("/data/OSTLERTEKKERS_HEN/settings.json.bak") == 1);
    CHECK(fs.files.count("/data/OSTLERTEKKERS_HEN/settings.json.tmp") == 0);

    // corrupt main file -> backup is used
    fs.files["/data/OSTLERTEKKERS_HEN/settings.json"] = "{ broken";
    CHECK(store.load(&src).themeId == "aurora" && src == "backup");

    // failed or corrupted write must not destroy the current file
    CHECK(store.save(a).isOk());
    const std::string before = fs.files["/data/OSTLERTEKKERS_HEN/settings.json"];
    fs.corruptWrites = true;
    Settings c = a;
    c.themeId = "synthwave";
    CHECK(!store.save(c).isOk());
    CHECK(fs.files["/data/OSTLERTEKKERS_HEN/settings.json"] == before);
    fs.corruptWrites = false;
    fs.failWrites = true;
    CHECK(!store.save(c).isOk());
    CHECK(fs.files["/data/OSTLERTEKKERS_HEN/settings.json"] == before);
    fs.failWrites = false;

    CHECK(store.resetToDefaults().isOk() && store.load().themeId == "midnight");
}

void runPhaseCBaseTests() {
    testJson();
    testI18n();
    testTheme();
    testSettings();
}
