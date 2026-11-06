// PS4 HEN ULTIMATE - cheats/cheat_naming.cpp
#include "cheats/cheat_naming.h"

#include <cctype>

namespace henu {

const char* toString(CheatFormat f) {
    switch (f) {
        case CheatFormat::Json: return "json";
        case CheatFormat::Shn:  return "shn";
        case CheatFormat::Mc4:  return "mc4";
    }
    return "json";
}

const char* toString(Region r) {
    switch (r) {
        case Region::US:      return "US";
        case Region::EU:      return "EU";
        case Region::JP:      return "JP";
        case Region::Asia:    return "ASIA";
        case Region::Unknown: return "unknown";
    }
    return "unknown";
}

const char* toString(CheatCategory c) {
    switch (c) {
        case CheatCategory::Health:     return "health";
        case CheatCategory::Money:      return "money";
        case CheatCategory::Ammo:       return "ammo";
        case CheatCategory::Stamina:    return "stamina";
        case CheatCategory::Experience: return "experience";
        case CheatCategory::Unlocks:    return "unlocks";
        case CheatCategory::Resources:  return "resources";
        case CheatCategory::Stats:      return "stats";
        case CheatCategory::Gameplay:   return "gameplay";
        case CheatCategory::Other:      return "other";
    }
    return "other";
}

bool isValidTitleId(const std::string& s) {
    if (s.size() != 9) return false;
    for (int i = 0; i < 4; ++i)
        if (s[i] < 'A' || s[i] > 'Z') return false;
    for (int i = 4; i < 9; ++i)
        if (s[i] < '0' || s[i] > '9') return false;
    return true;
}

Result<CheatFileId> parseCheatFileName(const std::string& fileName) {
    using R = Result<CheatFileId>;
    if (fileName.find('/') != std::string::npos || fileName.find('\\') != std::string::npos)
        return R::fail(ErrorCode::InvalidArgument, "file name must not contain a directory");

    // Extension
    size_t dot = fileName.rfind('.');
    if (dot == std::string::npos) return R::fail(ErrorCode::ParseError, "missing extension");
    std::string ext = fileName.substr(dot + 1);
    for (auto& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    CheatFileId id;
    if (ext == "json") id.format = CheatFormat::Json;
    else if (ext == "shn") id.format = CheatFormat::Shn;
    else if (ext == "mc4") id.format = CheatFormat::Mc4;
    else return R::fail(ErrorCode::ParseError, "unsupported extension: " + ext);

    std::string stem = fileName.substr(0, dot);
    // TITLEID(9) '_' VERSION(5) [ '_' process ]
    if (stem.size() < 15 || stem[9] != '_')
        return R::fail(ErrorCode::ParseError, "expected TITLEID_VERSION");
    id.titleId = stem.substr(0, 9);
    if (!isValidTitleId(id.titleId)) return R::fail(ErrorCode::ParseError, "invalid title id");
    if (!AppVersion::parse(stem.substr(10, 5), id.version))
        return R::fail(ErrorCode::ParseError, "invalid version (expected DD.DD)");

    if (stem.size() > 15) {
        if (stem[15] != '_') return R::fail(ErrorCode::ParseError, "unexpected text after version");
        id.variant = stem.substr(16);
        if (id.variant.empty()) return R::fail(ErrorCode::ParseError, "empty suffix");
    }
    return R::ok(id);
}

std::string makeCheatFileName(const CheatFileId& id) {
    std::string s = id.titleId + "_" + id.version.toString();
    if (!id.variant.empty()) s += "_" + id.variant;
    s += ".";
    s += toString(id.format);
    return s;
}

Region regionFromContentId(const std::string& contentId) {
    if (contentId.size() < 2) return Region::Unknown;
    std::string p = contentId.substr(0, 2);
    if (p == "UP") return Region::US;
    if (p == "EP") return Region::EU;
    if (p == "JP") return Region::JP;
    if (p == "HP") return Region::Asia;
    return Region::Unknown;
}

Compatibility checkCompatibility(const CheatFileId& cheat,
                                 const std::string& installedTitleId,
                                 const AppVersion& installedVersion) {
    if (installedTitleId.empty()) return Compatibility::GameNotInstalled;
    if (cheat.titleId != installedTitleId) return Compatibility::WrongTitle;
    if (cheat.version != installedVersion) return Compatibility::VersionMismatch;
    return Compatibility::Compatible;
}

namespace {
std::string lower(const std::string& s) {
    std::string o = s;
    for (auto& ch : o) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return o;
}
bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }
}  // namespace

CheatCategory categorize(const std::string& cheatName) {
    const std::string n = lower(cheatName);
    // Order matters: more specific terms first.
    struct Rule { CheatCategory cat; const char* words[8]; };
    static const Rule rules[] = {
        {CheatCategory::Ammo,       {"ammo", "munition", "bullet", "clip", "grenade", "granate", nullptr}},
        {CheatCategory::Health,     {"health", "leben", " hp", "hp ", "god mode", "godmode", "invincib", "unverwundbar"}},
        {CheatCategory::Stamina,    {"stamina", "ausdauer", "sprint", "endurance", nullptr}},
        {CheatCategory::Experience, {"exp", "xp", "erfahrung", "level up", "skill point", "fertigkeitspunkt", nullptr}},
        {CheatCategory::Money,      {"money", "geld", "gold", "cash", "credit", "coin", "münz", "currency"}},
        {CheatCategory::Unlocks,    {"unlock", "freischalt", "all items", "alle items", nullptr}},
        {CheatCategory::Resources,  {"resource", "ressource", "material", "item", "craft", nullptr}},
        {CheatCategory::Stats,      {"stat", "attribut", "strength", "stärke", "defense", "verteidigung", "attack", nullptr}},
        {CheatCategory::Gameplay,   {"speed", "jump", "time", "zeit", "fly", "no clip", "one hit", "timer"}},
    };
    for (const auto& r : rules) {
        for (const char* w : r.words) {
            if (w && has(n, w)) return r.cat;
        }
    }
    return CheatCategory::Other;
}

}  // namespace henu
