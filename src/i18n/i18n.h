// PS4 HEN ULTIMATE - i18n/i18n.h
// Key-based translations from assets/lang/<lang>.json. Lookup order:
// current language -> English -> the key itself (never empty on screen).
#pragma once

#include <map>
#include <initializer_list>
#include <string>

#include "core/result.h"

namespace henu {

class I18n {
public:
    // Loads one language from JSON text. "_meta" is ignored.
    Status loadLanguage(const std::string& lang, const std::string& jsonText);
    void setLanguage(const std::string& lang) { current_ = lang; }
    const std::string& language() const { return current_; }
    bool hasLanguage(const std::string& lang) const { return tables_.count(lang) != 0; }

    // Returns a copy: the fallback is the key itself, which may be a temporary.
    std::string t(const std::string& key) const;

    // Replaces {0}, {1}, ... with args.
    std::string fmt(const std::string& key, std::initializer_list<std::string> args) const;

private:
    std::map<std::string, std::map<std::string, std::string>> tables_;
    std::string current_ = "de";
};

}  // namespace henu
