// OSTLERTEKKERS HEN - i18n/i18n.cpp
#include "i18n/i18n.h"

#include <vector>

#include "core/json.h"

namespace henu {

Status I18n::loadLanguage(const std::string& lang, const std::string& jsonText) {
    auto parsed = parseJson(jsonText);
    if (!parsed) return Status::fail(ErrorCode::ParseError, "lang/" + lang + ": " + parsed.error().message);
    if (!parsed.value().isObject()) return Status::fail(ErrorCode::ParseError, "lang/" + lang + ": not an object");
    auto& table = tables_[lang];
    table.clear();
    for (const auto& kv : parsed.value().asObject()) {
        if (kv.first == "_meta") continue;
        if (!kv.second.isString())
            return Status::fail(ErrorCode::ParseError, "lang/" + lang + ": value of '" + kv.first + "' is not a string");
        table[kv.first] = kv.second.asString();
    }
    return Status::ok();
}

std::string I18n::t(const std::string& key) const {
    auto find = [&](const std::string& lang) -> const std::string* {
        auto tit = tables_.find(lang);
        if (tit == tables_.end()) return nullptr;
        auto it = tit->second.find(key);
        return it == tit->second.end() ? nullptr : &it->second;
    };
    if (const std::string* s = find(current_)) return *s;
    if (const std::string* s = find("en")) return *s;
    return key;
}

std::string I18n::fmt(const std::string& key, std::initializer_list<std::string> args) const {
    std::string s = t(key);
    std::vector<std::string> a(args);
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '{' && i + 2 < s.size() && s[i + 1] >= '0' && s[i + 1] <= '9' && s[i + 2] == '}') {
            size_t idx = static_cast<size_t>(s[i + 1] - '0');
            out += idx < a.size() ? a[idx] : std::string();
            i += 2;
        } else {
            out += s[i];
        }
    }
    return out;
}

}  // namespace henu
