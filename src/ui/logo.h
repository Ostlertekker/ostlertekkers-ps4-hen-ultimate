// OSTLERTEKKERS HEN - ui/logo.h
// The app logo: a pentagram inside a ring, drawn as vector paths (no image
// assets). Colours should be opaque; overlapping strokes are drawn twice.
#pragma once

#include "platform/platform.h"

namespace henu {

struct LogoStyle {
    Color star{255, 255, 255, 255};
    Color ring{255, 255, 255, 255};
    float strokeRatio = 0.075f;   // stroke width relative to radius
    bool showRing = true;
    bool pointDown = true;        // inverted pentagram
    float rotation = 0;           // radians, for animations
    float progress = 1;           // 0..1 draws the strokes partially (boot animation)
};

void drawPentagram(IRenderer& r, float cx, float cy, float radius, const LogoStyle& s);

// Brand name shown in the top bar and on the boot screen.
constexpr const char* kBrandName = "OSTLERTEKKERS HEN";

}  // namespace henu
