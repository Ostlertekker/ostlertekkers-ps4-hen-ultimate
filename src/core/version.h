// OSTLERTEKKERS HEN - core/version.h
// PS4 application versions ("01.00", "01.23") and dashboard semver ("1.0.0").
#pragma once

#include <string>

namespace henu {

// PS4 APP_VER as stored in param.sfo / used in GoldHEN cheat file names:
// two 2-digit groups separated by a dot, e.g. "01.05".
struct AppVersion {
    int major = 0;
    int minor = 0;

    // Strict: exactly "DD.DD". Returns false for anything else.
    static bool parse(const std::string& s, AppVersion& out);
    std::string toString() const;  // always "DD.DD"

    bool operator==(const AppVersion& o) const { return major == o.major && minor == o.minor; }
    bool operator!=(const AppVersion& o) const { return !(*this == o); }
    bool operator<(const AppVersion& o) const {
        return major != o.major ? major < o.major : minor < o.minor;
    }
};

// Dashboard version "MAJOR.MINOR.PATCH".
struct SemVer {
    int major = 0, minor = 0, patch = 0;
    static bool parse(const std::string& s, SemVer& out);
    std::string toString() const;
    int compare(const SemVer& o) const;  // <0, 0, >0
};

}  // namespace henu
