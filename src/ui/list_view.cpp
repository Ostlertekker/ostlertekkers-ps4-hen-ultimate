// PS4 HEN ULTIMATE - ui/list_view.cpp
#include "ui/list_view.h"

#include <algorithm>

namespace henu {

void ListView::setFocus(int i) {
    const int n = static_cast<int>(items.size());
    focus_ = n == 0 ? 0 : std::max(0, std::min(i, n - 1));
}

const ListItem* ListView::current() const {
    if (items.empty()) return nullptr;
    return &items[static_cast<size_t>(std::min<int>(focus_, static_cast<int>(items.size()) - 1))];
}

bool ListView::handle(Action a) {
    const int n = static_cast<int>(items.size());
    if (n == 0) return false;
    const int old = focus_;
    switch (a) {
        case Action::Up: focus_ = std::max(0, focus_ - 1); break;
        case Action::Down: focus_ = std::min(n - 1, focus_ + 1); break;
        case Action::Left: focus_ = std::max(0, focus_ - visibleRows_); break;
        case Action::Right: focus_ = std::min(n - 1, focus_ + visibleRows_); break;
        default: return false;
    }
    return focus_ != old || a == Action::Up || a == Action::Down;
}

void ListView::draw(IRenderer& r, const Theme& t, const Fonts& f, const Rect& area, double time,
                    const std::string& emptyText) {
    const int n = static_cast<int>(items.size());
    if (n == 0) {
        r.drawText(f.body, emptyText, area.x + 8, area.y + 20, t.textMuted);
        return;
    }
    const float gap = 10;
    visibleRows_ = std::max(1, static_cast<int>((area.h + gap) / (rowHeight + gap)));
    focus_ = std::max(0, std::min(focus_, n - 1));
    if (focus_ < top_) top_ = focus_;
    if (focus_ >= top_ + visibleRows_) top_ = focus_ - visibleRows_ + 1;
    top_ = std::max(0, std::min(top_, std::max(0, n - visibleRows_)));

    for (int i = top_; i < n && i < top_ + visibleRows_; ++i) {
        const ListItem& it = items[static_cast<size_t>(i)];
        const Rect row{area.x, area.y + (i - top_) * (rowHeight + gap), area.w - 24, rowHeight};
        const bool foc = i == focus_;
        r.fillRect(row, foc ? withAlpha(t.surfaceAlt, 0.97f) : withAlpha(t.surface, t.surfaceOpacity * 0.85f), 16);
        float x = row.x + 26;
        const float titleY = it.subtitle.empty() ? row.y + (rowHeight - 36) / 2 : row.y + 16 + (rowHeight - 92) / 2;
        if (it.star) {
            r.drawText(f.bodyBold, "\xE2\x98\x85", x, titleY, t.warn);   // ★
            x += 30;
        }
        const Color tc = it.dim ? t.textMuted : t.text;
        float rightW = it.right.empty() ? 0 : r.measureText(f.small, it.right) + 30;
        float badgeW = it.badge.empty() ? 0 : pillWidth(r, f, it.badge) + 16;
        const float maxTitle = row.w - (x - row.x) - rightW - badgeW - 30;
        const std::string title = ellipsize(r, f.bodyBold, it.title, maxTitle);
        r.drawText(f.bodyBold, title, x, titleY, tc);
        if (!it.subtitle.empty()) {
            const std::string sub = ellipsize(r, f.small, it.subtitle, maxTitle);
            r.drawText(f.small, sub, x, titleY + 36, t.textMuted);
        }
        float rx = row.x + row.w - 24;
        if (!it.right.empty()) {
            r.drawText(f.small, it.right, rx, row.y + (rowHeight - 24) / 2, t.textMuted, TextAlign::Right);
            rx -= rightW;
        }
        if (!it.badge.empty()) {
            const float bw = pillWidth(r, f, it.badge);
            Color bg = it.badgeColor.a ? withAlpha(it.badgeColor, 0.22f) : withAlpha(t.text, 0.12f);
            Color fg = it.badgeColor.a ? it.badgeColor : t.text;
            drawPill(r, f, it.badge, rx - bw, row.y + (rowHeight - 34) / 2, bg, fg);
        }
        if (foc) drawFocusRing(r, t, row, 16, time);
    }
    // scrollbar
    if (n > visibleRows_) {
        const float trackH = area.h;
        const float thumbH = std::max(40.f, trackH * visibleRows_ / n);
        const float thumbY = area.y + (trackH - thumbH) * top_ / std::max(1, n - visibleRows_);
        r.fillRect({area.x + area.w - 8, area.y, 6, trackH}, withAlpha(t.text, 0.08f), 3);
        r.fillRect({area.x + area.w - 8, thumbY, 6, thumbH}, withAlpha(t.text, 0.45f), 3);
    }
}

float drawInfoRows(IRenderer& r, const Theme& t, const Fonts& f, float x, float y, float w,
                   const std::vector<std::pair<std::string, std::string>>& rows) {
    for (size_t i = 0; i < rows.size(); ++i) {
        r.drawText(f.small, rows[i].first, x, y, t.textMuted);
        const std::string v = ellipsize(r, f.small, rows[i].second, w * 0.62f);
        r.drawText(f.small, v, x + w, y, t.text, TextAlign::Right);
        y += 40;
        if (i + 1 < rows.size()) r.fillRect({x, y - 10, w, 1}, t.line, 0);
    }
    return y;
}

}  // namespace henu
