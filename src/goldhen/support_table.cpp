// PS4 HEN ULTIMATE - goldhen/support_table.cpp
// Firmware support per GoldHEN build, taken ONLY from developer changelogs.
// Sources (2026-11-06):
//   v2.4b18     github.com/GoldHEN/GoldHEN/releases  -> 5.05, 6.71/6.72, 9.00,
//               9.60, 10.00/10.01, 10.50, 10.70/10.71, 11.00
//   v2.4b18.11  ko-fi.com/s/ccbe9f7bf0 (SiSTRo)       -> "Added support for 13.52"
//   v2.4b18.12  ko-fi.com/s/85ea22bd2f (SiSTRo)       -> "Added support for 13.02,
//               13.04 and 13.50"
// Builds between 18 and 18.11 are intentionally NOT listed (not verified here).
#include "goldhen/goldhen_info.h"

#include <cstdio>
#include <initializer_list>

namespace henu {

namespace {
// Parses "2.4b18.12" -> {2,4,18,12}; returns false if format differs.
bool parseBuild(const std::string& v, int out[4]) {
    int a = 0, b = 0, c = 0, d = 0;
    char tail = 0;
    int n = std::sscanf(v.c_str(), "%d.%db%d.%d%c", &a, &b, &c, &d, &tail);
    if (n == 4) { out[0] = a; out[1] = b; out[2] = c; out[3] = d; return true; }
    n = std::sscanf(v.c_str(), "%d.%db%d%c", &a, &b, &c, &tail);
    if (n == 3) { out[0] = a; out[1] = b; out[2] = c; out[3] = 0; return true; }
    return false;
}

bool atLeast(const int v[4], int a, int b, int c, int d) {
    const int w[4] = {a, b, c, d};
    for (int i = 0; i < 4; ++i) {
        if (v[i] != w[i]) return v[i] > w[i];
    }
    return true;
}

bool in(const std::string& fw, std::initializer_list<const char*> list) {
    for (const char* s : list) if (fw == s) return true;
    return false;
}
}  // namespace

FirmwareSupport lookupSupport(const std::string& goldhenVersion, const std::string& firmware) {
    int v[4];
    if (firmware.empty() || !parseBuild(goldhenVersion, v)) return FirmwareSupport::Unknown;
    if (v[0] != 2 || v[1] != 4) return FirmwareSupport::NotListed;  // only 2.4 line tracked

    if (atLeast(v, 2, 4, 18, 0) &&
        in(firmware, {"5.05", "6.71", "6.72", "9.00", "9.60", "10.00", "10.01",
                      "10.50", "10.70", "10.71", "11.00"}))
        return FirmwareSupport::SupportedBeta;
    if (atLeast(v, 2, 4, 18, 11) && firmware == "13.52") return FirmwareSupport::SupportedBeta;
    if (atLeast(v, 2, 4, 18, 12) && in(firmware, {"13.02", "13.04", "13.50"}))
        return FirmwareSupport::SupportedBeta;
    return FirmwareSupport::NotListed;
}

}  // namespace henu
