// PS4 HEN ULTIMATE - goldhen/goldhen_info.cpp
#include "goldhen/goldhen_info.h"

#include "files/fs_util.h"

namespace henu {

namespace {
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}
size_t countFiles(IFileSystem& fs, const std::string& dir) {
    auto e = fs.list(dir);
    if (!e) return 0;
    size_t n = 0;
    for (const auto& x : e.value()) if (!x.isDir) ++n;
    return n;
}
}  // namespace

std::vector<GoldhenPlugin> parsePluginsIni(const std::string& text) {
    std::vector<GoldhenPlugin> out;
    std::string section = "default";
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        std::string line = trim(text.substr(start, nl == std::string::npos ? std::string::npos : nl - start));
        start = nl == std::string::npos ? text.size() + 1 : nl + 1;
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        out.push_back({section, line, false});
        if (out.size() > 500) break;
    }
    return out;
}

GoldhenStatus detectGoldhen(IFileSystem& fs, const std::string& root) {
    GoldhenStatus s;
    s.folderFound = fs.exists(root);
    if (!s.folderFound) return s;
    s.configFound = fs.exists(root + "/config.ini");
    s.cheatsJson = countFiles(fs, root + "/cheats/json");
    s.cheatsShn = countFiles(fs, root + "/cheats/shn");
    s.cheatsMc4 = countFiles(fs, root + "/cheats/mc4");
    s.patchFiles = listFilesRecursive(fs, root + "/patches", 2).size();
    auto ini = fs.readFile(root + "/plugins.ini", 256 * 1024);
    if (ini) {
        s.pluginsIniFound = true;
        s.plugins = parsePluginsIni(ini.value());
        for (auto& p : s.plugins) p.fileExists = fs.exists(p.path);
    }
    return s;
}

std::string minimumGoldhenFor(const std::string& fw) {
    for (const char* f : {"5.05", "6.71", "6.72", "9.00", "9.60", "10.00", "10.01", "10.50", "10.70", "10.71", "11.00"})
        if (fw == f) return "v2.4b18";
    if (fw == "13.52") return "v2.4b18.11";
    if (fw == "13.02" || fw == "13.04" || fw == "13.50") return "v2.4b18.12";
    return "";
}

}  // namespace henu
