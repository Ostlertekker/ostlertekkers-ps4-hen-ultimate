// OSTLERTEKKERS HEN - app/boot.h
// Boot animations shown when the app starts (skippable with any button).
// All animations are original artwork: they evoke console "eras" through
// style only (flat polygons, fog, waves, light trails, minimal glow) and use
// no logos, names, sounds or signature visuals of any console maker.
#pragma once

#include <string>
#include <vector>

#include "ui/draw.h"

namespace henu {

struct BootStyleInfo {
    const char* id;        // settings value
    const char* nameKey;   // i18n key
    int soundVariant;      // synthBootSound(variant)
};

const std::vector<BootStyleInfo>& bootStyles();   // inferno, era1..era5
// Resolves "random" (seeded) and unknown ids; returns "" for "off".
std::string resolveBootStyle(const std::string& setting, unsigned seed);
int bootSoundVariant(const std::string& styleId);

class BootSequence {
public:
    explicit BootSequence(std::string styleId);
    const std::string& style() const { return style_; }
    double duration() const { return 6.4; }
    bool finished(double t) const { return t >= duration(); }
    // Draws the complete frame (no app chrome) for time t since start.
    void draw(IRenderer& r, const Fonts& f, double t) const;
    // Only the style's animated scenery (no brand text, no fade).
    void drawBackdrop(IRenderer& r, double t) const;
    // Signature colour of the style.
    Color accent() const;

private:
    void drawInferno(IRenderer& r, double t) const;
    void drawPolygon(IRenderer& r, double t) const;
    void drawFog(IRenderer& r, double t) const;
    void drawWaves(IRenderer& r, double t) const;
    void drawTrails(IRenderer& r, double t) const;
    void drawGleam(IRenderer& r, double t) const;
    void drawBrand(IRenderer& r, const Fonts& f, double t, Color text, Color accent) const;

    std::string style_;
};

}  // namespace henu
