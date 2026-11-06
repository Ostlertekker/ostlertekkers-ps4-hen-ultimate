// PS4 HEN ULTIMATE - ui/focus.h
// Spatial focus navigation: from the focused rectangle, pick the nearest
// rectangle in the pressed direction. Works for any tile layout.
#pragma once

#include <vector>

#include "platform/platform.h"
#include "ui/input.h"

namespace henu {

// Returns the new index, or `current` if nothing lies in that direction.
int moveFocus(const std::vector<Rect>& items, int current, Action dir);

}  // namespace henu
