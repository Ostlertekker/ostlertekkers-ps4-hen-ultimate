// OSTLERTEKKERS HEN - ui/list_view.h
// Scrollable list with focus, used by games, cheats, files, diagnostics ...
#pragma once

#include <string>
#include <vector>

#include "ui/draw.h"
#include "ui/input.h"

namespace henu {

struct ListItem {
    std::string title;
    std::string subtitle;
    std::string right;         // right-aligned text (version, size ...)
    std::string badge;         // small pill (e.g. "PASST")
    Color badgeColor{0, 0, 0, 0};
    bool dim = false;
    bool star = false;         // favourite marker
    std::string key;           // id for the owner
};

class ListView {
public:
    std::vector<ListItem> items;
    float rowHeight = 92;

    int focus() const { return focus_; }
    void setFocus(int i);
    const ListItem* current() const;
    // Up/Down move by one, Left/Right by a page. Returns true if handled.
    bool handle(Action a);
    void draw(IRenderer& r, const Theme& t, const Fonts& f, const Rect& area, double time,
              const std::string& emptyText);

private:
    int focus_ = 0;
    int top_ = 0;
    int visibleRows_ = 6;
};

// Key/value rows inside a panel; returns the y after the last row.
float drawInfoRows(IRenderer& r, const Theme& t, const Fonts& f, float x, float y, float w,
                   const std::vector<std::pair<std::string, std::string>>& rows);

}  // namespace henu
