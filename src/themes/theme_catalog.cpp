// OSTLERTEKKERS HEN - themes/theme_catalog.cpp
#include "themes/theme_catalog.h"

#include <algorithm>

namespace henu {

void ThemeCatalog::loadDir(IFileSystem& fs, const std::string& dir, bool builtIn) {
    if (dir.empty() || !fs.exists(dir)) return;
    auto entries = fs.list(dir);
    if (!entries) {
        errors_.push_back({dir, entries.error().message});
        return;
    }
    std::vector<std::string> names;
    for (const auto& e : entries.value()) {
        if (e.isDir) continue;
        if (e.name.size() > 5 && e.name.compare(e.name.size() - 5, 5, ".json") == 0) names.push_back(e.name);
    }
    std::sort(names.begin(), names.end());
    for (const auto& n : names) {
        const std::string path = dir + "/" + n;
        auto text = fs.readFile(path, 64 * 1024);
        if (!text) { errors_.push_back({path, text.error().message}); continue; }
        auto json = parseJson(text.value());
        if (!json) { errors_.push_back({path, json.error().message}); continue; }
        auto theme = parseTheme(json.value());
        if (!theme) { errors_.push_back({path, theme.error().message}); continue; }
        Theme t = theme.value();
        t.builtIn = builtIn;
        t.folder = dir;
        if (indexOf(t.id) >= 0) {
            // User themes may not silently replace bundled ones.
            errors_.push_back({path, "duplicate theme id '" + t.id + "' ignored"});
            continue;
        }
        themes_.push_back(std::move(t));
    }
}

void ThemeCatalog::scan(IFileSystem& fs, const std::string& bundleDir, const std::string& userDir) {
    themes_.clear();
    errors_.clear();
    loadDir(fs, bundleDir, true);
    loadDir(fs, userDir, false);
}

int ThemeCatalog::indexOf(const std::string& id) const {
    for (size_t i = 0; i < themes_.size(); ++i)
        if (themes_[i].id == id) return static_cast<int>(i);
    return -1;
}

const Theme& ThemeCatalog::getOrFallback(const std::string& id) const {
    int i = indexOf(id);
    if (i >= 0) return themes_[static_cast<size_t>(i)];
    if (!themes_.empty()) return themes_.front();
    return fallback_;
}

std::string ThemeCatalog::neighbour(const std::string& id, int delta) const {
    if (themes_.empty()) return fallback_.id;
    int n = static_cast<int>(themes_.size());
    int i = indexOf(id);
    if (i < 0) i = 0;
    i = ((i + delta) % n + n) % n;
    return themes_[static_cast<size_t>(i)].id;
}

}  // namespace henu
