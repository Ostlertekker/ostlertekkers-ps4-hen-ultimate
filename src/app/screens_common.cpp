// PS4 HEN ULTIMATE - app/screens_common.cpp
#include "app/screens_common.h"

#include "app/app.h"

namespace henu {

float drawPageHeader(const UiContext& ctx, App& app, const std::string& title, const std::string& subtitle) {
    IRenderer& r = ctx.renderer;
    r.drawText(ctx.fonts.label, app.t("module.eyebrow"), kMarginX, 150, ctx.theme.accent);
    r.drawText(ctx.fonts.h2, title, kMarginX, 174, ctx.theme.text);
    if (!subtitle.empty())
        r.drawText(ctx.fonts.body, ellipsize(r, ctx.fonts.body, subtitle, kScreenW - 2 * kMarginX), kMarginX, 236,
                   ctx.theme.textMuted);
    return 292;
}

// =================================================================== Confirm

ConfirmScreen::ConfirmScreen(App& app, std::string title, std::string text, std::string yesLabel,
                             std::function<void()> onYes, bool danger)
    : Screen(app), title_(std::move(title)), text_(std::move(text)), yes_(std::move(yesLabel)),
      onYes_(std::move(onYes)), danger_(danger) {}

ScreenResult ConfirmScreen::handle(const InputEvent& ev) {
    switch (ev.action) {
        case Action::Left: case Action::Up: choice_ = 0; break;
        case Action::Right: case Action::Down: choice_ = 1; break;
        case Action::Confirm:
            if (choice_ == 0 && onYes_) onYes_();
            return ScreenResult::pop();
        case Action::Back: return ScreenResult::pop();
        default: break;
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> ConfirmScreen::hints() const { return {{"@confirm", "hint.select"}}; }

void ConfirmScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    const Rect card{kScreenW / 2 - 520, 300, 1040, 420};
    drawPanel(r, t, card, 28);
    r.drawText(f.title, title_, card.x + 48, card.y + 44, danger_ ? t.error : t.text);
    float y = card.y + 110;
    for (const auto& line : wrapText(r, f.body, text_, card.w - 96)) {
        r.drawText(f.body, line, card.x + 48, y, t.textMuted);
        y += 38;
        if (y > card.y + 300) break;
    }
    const std::string labels[2] = {yes_, app_.t("common.cancel")};
    for (int i = 0; i < 2; ++i) {
        const Rect b{card.x + 48 + i * 300.f, card.y + card.h - 100, 270, 64};
        const bool foc = choice_ == i;
        Color bg = foc ? (i == 0 && danger_ ? t.error : t.focus) : withAlpha(t.text, 0.10f);
        r.fillRect(b, bg, 16);
        r.drawText(f.bodyBold, labels[i], b.x + b.w / 2, b.y + 17, foc ? t.background : t.text, TextAlign::Center);
        if (foc) drawFocusRing(r, t, b, 16, ctx.time);
    }
}

// =================================================================== Action menu

ActionMenuScreen::ActionMenuScreen(App& app, std::string title, std::vector<MenuAction> actions)
    : Screen(app), title_(std::move(title)), actions_(std::move(actions)) {
    for (const auto& a : actions_) {
        ListItem it;
        it.title = a.label;
        it.subtitle = a.detail;
        it.dim = !a.enabled;
        list_.items.push_back(it);
    }
    list_.rowHeight = 84;
}

ScreenResult ActionMenuScreen::handle(const InputEvent& ev) {
    if (ev.action == Action::Back || ev.action == Action::Menu) return ScreenResult::pop();
    if (ev.action == Action::Confirm && !actions_.empty()) {
        const MenuAction& a = actions_[static_cast<size_t>(list_.focus())];
        if (!a.enabled) { app_.toast(a.detail.empty() ? app_.t("common.unavailable") : a.detail); return ScreenResult::none(); }
        return a.run ? a.run() : ScreenResult::pop();
    }
    list_.handle(ev.action);
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> ActionMenuScreen::hints() const { return {{"@confirm", "hint.select"}}; }

void ActionMenuScreen::draw(const UiContext& ctx) {
    const Rect card{kScreenW / 2 - 560, 200, 1120, 760};
    drawPanel(ctx.renderer, ctx.theme, card, 28);
    ctx.renderer.drawText(ctx.fonts.title, title_, card.x + 44, card.y + 40, ctx.theme.text);
    list_.draw(ctx.renderer, ctx.theme, ctx.fonts, {card.x + 36, card.y + 110, card.w - 60, card.h - 140}, ctx.time, "");
}

// =================================================================== Keyboard

namespace {
const char* kKeys[4] = {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-._ "};
constexpr int kCols = 10, kRows = 5;   // last row: space, delete, clear, OK
}  // namespace

KeyboardScreen::KeyboardScreen(App& app, std::string title, std::string initial, std::function<void(const std::string&)> onDone)
    : Screen(app), title_(std::move(title)), text_(std::move(initial)), onDone_(std::move(onDone)) {}

ScreenResult KeyboardScreen::handle(const InputEvent& ev) {
    switch (ev.action) {
        case Action::Up: row_ = (row_ + kRows - 1) % kRows; break;
        case Action::Down: row_ = (row_ + 1) % kRows; break;
        case Action::Left: col_ = (col_ + kCols - 1) % kCols; break;
        case Action::Right: col_ = (col_ + 1) % kCols; break;
        case Action::Search:   // square = delete
            if (!text_.empty()) text_.pop_back();
            break;
        case Action::Menu:     // options = done
            if (onDone_) onDone_(text_);
            return ScreenResult::pop();
        case Action::Back: return ScreenResult::pop();
        case Action::Confirm:
            if (row_ < 4) {
                if (text_.size() < 40) text_ += kKeys[row_][col_];
            } else {
                const int k = col_ / 3;   // 0-2 space, 3-5 delete, 6-8 clear, 9 ok
                if (col_ == 9) { if (onDone_) onDone_(text_); return ScreenResult::pop(); }
                if (k == 0 && text_.size() < 40) text_ += ' ';
                else if (k == 1 && !text_.empty()) text_.pop_back();
                else if (k == 2) text_.clear();
            }
            break;
        default: break;
    }
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> KeyboardScreen::hints() const {
    return {{"@confirm", "hint.type"}, {"\xE2\x96\xA1", "hint.delete"}, {"OPT", "hint.done"}};
}

void KeyboardScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& t = ctx.theme;
    const Fonts& f = ctx.fonts;
    drawPageHeader(ctx, app_, title_, app_.t("kbd.sub"));
    const Rect field{kMarginX, 300, kScreenW - 2 * kMarginX, 80};
    drawPanel(r, t, field, 18);
    r.drawText(f.title, text_ + "_", field.x + 28, field.y + 20, t.text);
    const float kw = 150, kh = 92, gap = 14;
    const float x0 = (kScreenW - (kCols * kw + (kCols - 1) * gap)) / 2, y0 = 420;
    for (int rr = 0; rr < 4; ++rr)
        for (int c = 0; c < kCols; ++c) {
            const Rect k{x0 + c * (kw + gap), y0 + rr * (kh + gap), kw, kh};
            const bool foc = rr == row_ && c == col_;
            r.fillRect(k, foc ? t.focus : withAlpha(t.surface, t.surfaceOpacity), 14);
            const char ch[2] = {kKeys[rr][c], 0};
            r.drawText(f.title, ch[0] == ' ' ? "_" : ch, k.x + kw / 2, k.y + 24, foc ? t.background : t.text,
                       TextAlign::Center);
        }
    const char* specials[4] = {"kbd.space", "kbd.delete", "kbd.clear", "kbd.ok"};
    for (int s = 0; s < 4; ++s) {
        const int c0 = s * 3, span = s == 3 ? 1 : 3;
        const Rect k{x0 + c0 * (kw + gap), y0 + 4 * (kh + gap), span * kw + (span - 1) * gap, kh};
        const bool foc = row_ == 4 && (s == 3 ? col_ == 9 : (col_ / 3 == s && col_ != 9));
        r.fillRect(k, foc ? t.focus : withAlpha(t.accent, 0.25f), 14);
        r.drawText(f.bodyBold, app_.t(specials[s]), k.x + k.w / 2, k.y + 30, foc ? t.background : t.text, TextAlign::Center);
    }
}

}  // namespace henu
