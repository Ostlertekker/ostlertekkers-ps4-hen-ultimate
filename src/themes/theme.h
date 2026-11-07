// OSTLERTEKKERS HEN - themes/theme.h
// Theme model + JSON loader (schema: schemas/theme.schema.json).
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "core/json.h"
#include "core/result.h"
#include "platform/platform.h"

namespace henu {

struct Theme {
    std::string id;
    std::string nameDe, nameEn;
    std::string author, license;
    std::string category = "custom";   // ps5 | dark | neon | minimal | gaming | light | custom
    bool dark = true;
    Color background{8, 11, 20, 255}, surface{17, 24, 39, 255}, surfaceAlt{30, 36, 64, 255};
    Color text{244, 246, 255, 255}, textMuted{146, 155, 177, 255};
    Color accent{139, 124, 255, 255}, accent2{66, 183, 239, 255};
    Color focus{255, 255, 255, 255}, line{255, 255, 255, 23};
    Color ok{72, 201, 140, 255}, warn{229, 182, 94, 255}, error{232, 92, 106, 255};
    std::vector<std::pair<Color, Color>> tileGradients;
    float surfaceOpacity = 1.0f;
    float cornerRadius = 18.0f;
    std::string backgroundType = "gradient";   // solid | gradient | image
    std::string backgroundImage;               // file name inside theme folder
    float backgroundDim = 0.4f;
    float animSpeed = 1.0f;
    float focusScale = 1.05f;
    std::string ambient = "orb";
    std::string soundMove, soundSelect, soundBack;
    std::string folder;       // where the theme file lives (for images/sounds)
    bool builtIn = true;
    // Runtime appearance overlay (not persisted in the theme file; set per
    // frame from Settings by applyAppearance()). Lets any theme be combined
    // with a UI style, an animated background and rainbow focus borders.
    std::string uiStyle = "default";      // default | ps5 | ps3
    std::string backgroundFx = "theme";   // theme | galaxy | wave | aurora
    bool rgbBorder = false;

    const std::string& name(const std::string& lang) const {
        return (lang == "de" && !nameDe.empty()) ? nameDe : nameEn;
    }
};

// "#rrggbb" or "#rrggbbaa"
bool parseColor(const std::string& s, Color& out);
std::string colorToHex(const Color& c, bool withAlpha = false);

// Validates against the schema rules that matter for safety/rendering.
// Unknown fields are rejected so typos in imported themes are visible.
Result<Theme> parseTheme(const Json& j);
Json themeToJson(const Theme& t);

// Fallback used when no theme file can be loaded (never fails).
Theme defaultTheme();

// Overlays the appearance options (UI style, background FX, RGB borders) from
// Settings onto a copy of the theme. PS5 = brighter, larger rounded cards;
// PS3 = XMB-inspired: thin rows, small radius, cool translucent surfaces.
// These only tweak existing render parameters, so every screen follows.
Theme applyAppearance(const Theme& base, const std::string& uiStyle,
                      const std::string& backgroundFx, bool rgbBorders);

// A hue (0..1) -> fully saturated RGB. Used for animated rainbow borders.
Color rainbow(float hue);

}  // namespace henu
