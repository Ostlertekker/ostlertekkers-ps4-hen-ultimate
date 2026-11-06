// OSTLERTEKKERS HEN - core/version.cpp
#include "core/version.h"

#include <cstdio>

namespace henu {

namespace {
bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Parses a non-empty run of up to 4 digits; advances pos.
bool parseNum(const std::string& s, size_t& pos, int& out) {
    size_t start = pos;
    int v = 0;
    while (pos < s.size() && isDigit(s[pos]) && pos - start < 4) {
        v = v * 10 + (s[pos] - '0');
        ++pos;
    }
    if (pos == start) return false;
    if (pos < s.size() && isDigit(s[pos])) return false;  // too long
    out = v;
    return true;
}
}  // namespace

bool AppVersion::parse(const std::string& s, AppVersion& out) {
    if (s.size() != 5 || s[2] != '.') return false;
    if (!isDigit(s[0]) || !isDigit(s[1]) || !isDigit(s[3]) || !isDigit(s[4])) return false;
    out.major = (s[0] - '0') * 10 + (s[1] - '0');
    out.minor = (s[3] - '0') * 10 + (s[4] - '0');
    return true;
}

std::string AppVersion::toString() const {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%02d.%02d", major % 100, minor % 100);
    return buf;
}

bool SemVer::parse(const std::string& s, SemVer& out) {
    size_t pos = 0;
    SemVer v;
    if (!parseNum(s, pos, v.major)) return false;
    if (pos >= s.size() || s[pos] != '.') return false;
    ++pos;
    if (!parseNum(s, pos, v.minor)) return false;
    if (pos >= s.size() || s[pos] != '.') return false;
    ++pos;
    if (!parseNum(s, pos, v.patch)) return false;
    if (pos != s.size()) return false;
    out = v;
    return true;
}

std::string SemVer::toString() const {
    return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
}

int SemVer::compare(const SemVer& o) const {
    if (major != o.major) return major < o.major ? -1 : 1;
    if (minor != o.minor) return minor < o.minor ? -1 : 1;
    if (patch != o.patch) return patch < o.patch ? -1 : 1;
    return 0;
}

}  // namespace henu
