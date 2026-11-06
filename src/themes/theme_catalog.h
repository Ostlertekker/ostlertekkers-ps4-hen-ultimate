// PS4 HEN ULTIMATE - themes/theme_catalog.h
// Loads bundled themes (read-only, inside the PKG) and user themes (app data).
// Broken files are skipped and reported; at least the fallback theme exists.
#pragma once

#include <string>
#include <vector>

#include "platform/platform.h"
#include "themes/theme.h"

namespace henu {

class ThemeCatalog {
public:
    struct LoadError { std::string file, message; };

    // bundleDir e.g. "/app0/assets/themes", userDir e.g. "/data/PS4HENULTIMATE/themes"
    void scan(IFileSystem& fs, const std::string& bundleDir, const std::string& userDir);

    const std::vector<Theme>& themes() const { return themes_; }
    const std::vector<LoadError>& errors() const { return errors_; }

    int indexOf(const std::string& id) const;              // -1 if missing
    const Theme& getOrFallback(const std::string& id) const;
    // Cycles through themes; returns id of the neighbour.
    std::string neighbour(const std::string& id, int delta) const;
    void addTheme(const Theme& t) { themes_.push_back(t); }

private:
    void loadDir(IFileSystem& fs, const std::string& dir, bool builtIn);
    std::vector<Theme> themes_;
    std::vector<LoadError> errors_;
    Theme fallback_ = defaultTheme();
};

}  // namespace henu
