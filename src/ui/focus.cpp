// PS4 HEN ULTIMATE - ui/focus.cpp
#include "ui/focus.h"

#include <cmath>
#include <limits>

namespace henu {

int moveFocus(const std::vector<Rect>& items, int current, Action dir) {
    if (current < 0 || current >= static_cast<int>(items.size())) return items.empty() ? -1 : 0;
    const Rect& c = items[static_cast<size_t>(current)];
    int best = current;
    float bestScore = std::numeric_limits<float>::max();

    for (size_t i = 0; i < items.size(); ++i) {
        if (static_cast<int>(i) == current) continue;
        const Rect& r = items[i];
        float primary = 0, overlap = 0, offset = 0;
        // primary: gap along the direction (edge to edge); must be >= 0
        switch (dir) {
            case Action::Right: primary = r.x - (c.x + c.w); break;
            case Action::Left:  primary = c.x - (r.x + r.w); break;
            case Action::Down:  primary = r.y - (c.y + c.h); break;
            case Action::Up:    primary = c.y - (r.y + r.h); break;
            default: return current;
        }
        if (primary < -1.0f) continue;   // not in that direction (1px tolerance)
        if (primary < 0) primary = 0;
        if (dir == Action::Left || dir == Action::Right) {
            float top = std::fmax(c.y, r.y), bottom = std::fmin(c.y + c.h, r.y + r.h);
            overlap = bottom - top;
            offset = std::fabs((r.y + r.h / 2) - (c.y + c.h / 2));
        } else {
            float left = std::fmax(c.x, r.x), right = std::fmin(c.x + c.w, r.x + r.w);
            overlap = right - left;
            offset = std::fabs((r.x + r.w / 2) - (c.x + c.w / 2));
        }
        // Items that overlap in the cross axis are strongly preferred.
        float score = primary + offset * (overlap > 0 ? 0.3f : 2.0f) + (overlap > 0 ? 0 : 400.0f);
        if (score < bestScore) { bestScore = score; best = static_cast<int>(i); }
    }
    return best;
}

}  // namespace henu
