// OSTLERTEKKERS HEN - app/screens.cpp
#include "app/screens.h"

#include <algorithm>
#include <cmath>

#include "app/app.h"
#include "app/screens_common.h"
#include "audio/audio.h"
#include "goldhen/goldhen_info.h"
#include "files/fs_util.h"
#include "ui/focus.h"

namespace henu {

namespace {
Color statusColor(const Theme& t, CapStatus s) {
    switch (s) {
        case CapStatus::Available: return t.ok;
        case CapStatus::Limited: return t.warn;
        case CapStatus::Unverified: return t.accent2;
        case CapStatus::Unavailable: return t.error;
        case CapStatus::NotImplemented: return t.textMuted;
    }
    return t.textMuted;
}
// First UTF-8 code point of s (for monogram icons).
std::string firstGlyph(const std::string& s) {
    if (s.empty()) return s;
    unsigned char c = static_cast<unsigned char>(s[0]);
    size_t n = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
    return s.substr(0, n);
}
}  // namespace

// =================================================================== Home

HomeScreen::HomeScreen(App& app) : Screen(app) { layout(); }

void HomeScreen::layout() {
    tileIds_.clear();
    rects_.clear();
    tileIds_.push_back("@hero");
    rects_.push_back({kMarginX + 44, 500, 300, 56});   // hero button
    std::vector<std::string> ids;
    for (const auto& m : app_.modules())
        if (m.id != "home") ids.push_back(m.id);
    const int cols = 6;
    const float gap = 20;
    const float w = (kScreenW - 2 * kMarginX - gap * (cols - 1)) / cols;
    const float h = 172;
    for (size_t i = 0; i < ids.size(); ++i) {
        int row = static_cast<int>(i) / cols, col = static_cast<int>(i) % cols;
        tileIds_.push_back(ids[i]);
        rects_.push_back({kMarginX + col * (w + gap), 628 + row * (h + gap), w, h});
    }
    scale_.assign(rects_.size(), 1.0f);
    target_.assign(rects_.size(), 1.0f);
    if (focus_ >= static_cast<int>(rects_.size())) focus_ = 0;
}

ScreenResult HomeScreen::handle(const InputEvent& ev) {
    switch (ev.action) {
        case Action::Up: case Action::Down: case Action::Left: case Action::Right:
            focus_ = moveFocus(rects_, focus_, ev.action);
            return ScreenResult::none();
        case Action::Confirm: {
            const std::string& id = tileIds_[static_cast<size_t>(focus_)];
            return ScreenResult::goTo(id == "@hero" ? "themes" : id);
        }
        default:
            return ScreenResult::none();
    }
}

void HomeScreen::update(const UiContext& ctx) {
    for (size_t i = 0; i < scale_.size(); ++i) {
        target_[i] = static_cast<int>(i) == focus_ ? ctx.theme.focusScale : 1.0f;
        scale_[i] = ctx.animate ? approach(scale_[i], target_[i], ctx.dt, 14.0f * ctx.animSpeed) : target_[i];
        if (std::fabs(scale_[i] - target_[i]) < 0.0015f) scale_[i] = target_[i];   // settle -> idle
    }
}

bool HomeScreen::animating() const {
    for (size_t i = 0; i < scale_.size(); ++i)
        if (scale_[i] != target_[i]) return true;
    return false;
}

std::vector<std::pair<std::string, std::string>> HomeScreen::hints() const {
    return {{"@confirm", "hint.open"}};
}

void HomeScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& th = ctx.theme;
    const Fonts& f = ctx.fonts;
    const std::string lang = app_.settings().language;

    // ---- welcome
    r.drawText(f.label, app_.t("home.welcome"), kMarginX, 160, th.accent);
    r.drawText(f.h1, app_.t("home.headline"), kMarginX, 186, th.text);
    r.drawText(f.body, app_.t("home.sub"), kMarginX, 278, th.textMuted);

    const float rx = kScreenW - kMarginX;
    r.drawText(f.label, app_.t("home.target"), rx, 196, th.textMuted, TextAlign::Right);
    r.drawText(f.bodyBold, "PS4 · FW 13.52 · GoldHEN v2.4b18.12", rx, 222, th.text, TextAlign::Right);
    r.drawText(f.small, app_.t("home.targetNote"), rx, 260, th.textMuted, TextAlign::Right);

    // ---- hero card
    const Rect hero{kMarginX, 330, 1104, 238};
    r.gradientRect(hero, mix(th.surfaceAlt, th.accent, 0.25f), withAlpha(th.surface, th.surfaceOpacity), th.cornerRadius + 4);
    r.strokeRect(hero, th.line, 1.5f, th.cornerRadius + 4);
    // decorative orb
    for (int i = 0; i < 4; ++i) {
        float rad = 170.f - i * 30.f;
        r.fillRect({hero.x + 940 - rad, hero.y + 60 - rad, rad * 2, rad * 2},
                   withAlpha(mix(th.accent, th.focus, i * 0.15f), 0.10f + i * 0.06f), rad);
    }
    r.drawText(f.label, app_.t("home.featured"), hero.x + 44, hero.y + 34, mix(th.accent, th.focus, 0.4f));
    float ty = hero.y + 58;
    for (const auto& line : wrapText(r, f.h2, app_.t("home.heroTitle"), 700)) {
        r.drawText(f.h2, line, hero.x + 44, ty, th.text);
        ty += 50;
    }
    // hero button (focusable)
    const Rect& btn = rects_[0];
    Rect b = scaleAround(btn, scale_[0]);
    r.fillRect(b, focus_ == 0 ? th.focus : withAlpha(th.focus, 0.88f), 14);
    r.drawText(f.bodyBold, app_.t("home.explore"), b.x + b.w / 2, b.y + b.h / 2 - 16, th.background, TextAlign::Center);
    if (focus_ == 0) drawFocusRing(r, th, b, 14, ctx.time);

    // ---- status panel
    const Rect sp{kMarginX + 1104 + 24, 330, kScreenW - 2 * kMarginX - 1104 - 24, 238};
    drawPanel(r, th, sp, th.cornerRadius);
    r.drawText(f.bodyBold, app_.t("home.status"), sp.x + 28, sp.y + 24, th.text);
    struct Row { std::string k, v; Color c; };
    if (!statusRead_) {   // once per visit, not every frame
        statusRead_ = true;
        const SystemSnapshot snap = app_.systemInfo();
        firmware_ = snap.firmware.known ? snap.firmware.value : "";
        goldhenFolder_ = detectGoldhen(app_.sysFs()).folderFound;
    }
    const Row rows[] = {
        {app_.t("common.firmware"), firmware_.empty() ? app_.t("common.notQueried") : firmware_,
         firmware_.empty() ? th.textMuted : th.text},
        {"GoldHEN", app_.t(goldhenFolder_ ? "home.goldhenFound" : "home.goldhenMissing"), goldhenFolder_ ? th.ok : th.warn},
        {app_.t("home.themeLabel"), th.name(lang), th.text},
        {app_.t("home.modulesReady"),
         [&] {
             int ready = 0, total = 0;
             for (const auto& m : app_.modules()) {
                 ++total;
                 if (m.status != CapStatus::NotImplemented && m.status != CapStatus::Unavailable) ++ready;
             }
             return std::to_string(ready) + " / " + std::to_string(total);
         }(),
         th.text},
    };
    float ry = sp.y + 70;
    for (const auto& row : rows) {
        r.drawText(f.small, row.k, sp.x + 28, ry, th.textMuted);
        r.drawText(f.small, row.v, sp.x + sp.w - 28, ry, row.c, TextAlign::Right);
        ry += 40;
        if (&row != &rows[3]) r.fillRect({sp.x + 28, ry - 10, sp.w - 56, 1}, th.line, 0);
    }

    // ---- section heading
    r.drawText(f.label, app_.t("home.quick"), kMarginX, 592, th.accent);

    // ---- tiles (focused one drawn last so it overlaps neighbours)
    auto drawTile = [&](size_t i) {
        const std::string& id = tileIds_[i];
        const ModuleDef* m = app_.module(id);
        Rect rc = scaleAround(rects_[i], scale_[i]);
        const auto& grads = th.tileGradients;
        const auto& g = grads[(i - 1) % grads.size()];
        r.gradientRect(rc, g.first, g.second, th.cornerRadius);
        r.strokeRect(rc, th.line, 1.5f, th.cornerRadius);
        // monogram icon
        const std::string& title = app_.t("module." + id);
        r.fillRect({rc.x + 22, rc.y + 20, 46, 46}, withAlpha(th.focus, 0.16f), 14);
        r.drawText(f.bodyBold, firstGlyph(title), rc.x + 45, rc.y + 29, th.focus, TextAlign::Center);
        const FontId tf = r.measureText(f.tile, title) <= rc.w - 44 ? f.tile : f.bodyBold;
        r.drawText(tf, title, rc.x + 22, rc.y + 78, th.text);
        auto lines = wrapText(r, f.small, app_.t("module." + id + ".desc"), rc.w - 44);
        for (size_t li = 0; li < lines.size() && li < 2; ++li)
            r.drawText(f.small, lines[li], rc.x + 22, rc.y + 114 + li * 25.f, withAlpha(th.text, 0.72f));
        if (m && m->status == CapStatus::NotImplemented) {
            const std::string& s = app_.t("status.short.not_implemented");
            drawPill(r, f, s, rc.x + rc.w - 22 - pillWidth(r, f, s), rc.y + 22, withAlpha(th.background, 0.45f),
                     withAlpha(th.text, 0.8f));
        }
        if (static_cast<int>(i) == focus_) drawFocusRing(r, th, rc, th.cornerRadius, ctx.time);
    };
    for (size_t i = 1; i < rects_.size(); ++i)
        if (static_cast<int>(i) != focus_) drawTile(i);
    if (focus_ > 0) drawTile(static_cast<size_t>(focus_));
}

// =================================================================== Module page

ModuleScreen::ModuleScreen(App& app, std::string moduleId) : Screen(app), id_(std::move(moduleId)) {}

ScreenResult ModuleScreen::handle(const InputEvent& ev) {
    if (ev.action == Action::Back) return ScreenResult::pop();
    return ScreenResult::none();
}

std::vector<std::pair<std::string, std::string>> ModuleScreen::hints() const { return {}; }

void ModuleScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& th = ctx.theme;
    const Fonts& f = ctx.fonts;
    const ModuleDef* m = app_.module(id_);
    const CapStatus st = m ? m->status : CapStatus::NotImplemented;

    r.drawText(f.label, app_.t("module.eyebrow"), kMarginX, 170, th.accent);
    r.drawText(f.h1, app_.t("module." + id_), kMarginX, 196, th.text);
    r.drawText(f.body, app_.t("module." + id_ + ".desc"), kMarginX, 290, th.textMuted);

    // status card
    const Rect card{kMarginX, 360, 820, 200};
    drawPanel(r, th, card, th.cornerRadius);
    r.drawText(f.label, app_.t("module.statusLabel"), card.x + 32, card.y + 30, th.textMuted);
    const std::string& stText = app_.t(std::string("status.") + toString(st));
    r.fillRect({card.x + 32, card.y + 70, 14, 14}, statusColor(th, st), 7);
    r.drawText(f.title, stText, card.x + 58, card.y + 58, th.text);
    const std::string& expl = app_.t(std::string("status.explain.") + toString(st));
    float y = card.y + 116;
    for (const auto& line : wrapText(r, f.small, expl, card.w - 64)) {
        r.drawText(f.small, line, card.x + 32, y, th.textMuted);
        y += 30;
    }

    // planned features
    const Rect plan{kMarginX + 820 + 24, 360, kScreenW - 2 * kMarginX - 820 - 24, 560};
    drawPanel(r, th, plan, th.cornerRadius);
    r.drawText(f.label, app_.t("module.plannedLabel"), plan.x + 32, plan.y + 30, th.textMuted);
    y = plan.y + 72;
    for (const auto& line : wrapText(r, f.body, app_.t("module." + id_ + ".planned"), plan.w - 96)) {
        if (line.empty()) { y += 14; continue; }
        const bool bullet = line.size() > 2 && line[0] == '-' && line[1] == ' ';
        if (bullet) r.fillRect({plan.x + 34, y + 13, 8, 8}, th.accent, 4);
        r.drawText(f.body, bullet ? line.substr(2) : line, plan.x + (bullet ? 58 : 32), y, th.text);
        y += 40;
        if (y > plan.y + plan.h - 40) break;
    }

    // limitation note (from features.json note, e.g. "read-only")
    if (m && !m->note.empty()) {
        const Rect nc{kMarginX, 584, 820, 120};
        drawPanel(r, th, nc, th.cornerRadius);
        r.drawText(f.label, app_.t("module.noteLabel"), nc.x + 32, nc.y + 28, th.textMuted);
        const std::string& note = app_.t("module." + id_ + ".note");
        r.drawText(f.small, note, nc.x + 32, nc.y + 64, th.text);
    }
}

// =================================================================== Settings

SettingsScreen::SettingsScreen(App& app) : Screen(app) {}

void SettingsScreen::importWav() {
    const std::string usb = firstUsbRoot(app_.sysFs());
    if (usb.empty()) { app_.toast(app_.t("settings.wav.noUsb")); return; }
    const std::string from = usb + "/boot.wav";
    if (!app_.sysFs().exists(from)) { app_.toast(app_.t("settings.wav.notFound") + " " + from); return; }
    auto bytes = app_.sysFs().readFile(from, 64u << 20);
    if (!bytes) { app_.toast(bytes.error().message); return; }
    auto wav = decodeWav(bytes.value(), 30);
    if (!wav) { app_.toast(app_.t("settings.wav.invalid") + " " + wav.error().message); return; }
    Status st = guardedCopy(app_.sysFs(), app_.guard(), from, app_.customBootSoundPath());
    if (!st) { app_.toast(st.error().message); return; }
    Settings s = app_.settings();
    s.bootSound = "custom";
    if (s.volume == 0) { s.volume = 70; s.soundEnabled = true; }
    app_.updateSettings(s);
    app_.log().info("boot.wav imported from " + from);
    app_.toast(app_.t("settings.wav.done"));
}

std::vector<std::pair<std::string, std::string>> SettingsScreen::hints() const {
    if (focus_ == Reset) return {{"@confirm", "hint.reset"}};
    if (focus_ == BootPreview) return {{"@confirm", "hint.play"}};
    if (focus_ == ImportWav) return {{"@confirm", "hint.import"}};
    return {{"<>", "hint.change"}};
}

void SettingsScreen::change(int delta) {
    Settings s = app_.settings();
    auto cycle = [delta](const std::vector<std::string>& opts, const std::string& cur) {
        int n = static_cast<int>(opts.size()), i = 0;
        for (int k = 0; k < n; ++k) if (opts[static_cast<size_t>(k)] == cur) i = k;
        return opts[static_cast<size_t>(((i + delta) % n + n) % n)];
    };
    switch (focus_) {
        case Language: s.language = cycle({"de", "en"}, s.language); break;
        case ThemeRow: s.themeId = app_.themes().neighbour(s.themeId, delta); break;
        case Animations: s.animations = cycle({"full", "reduced", "off"}, s.animations); break;
        case ConfirmButton: s.confirmButton = cycle({"cross", "circle"}, s.confirmButton); break;
        case BootAnim:
            s.bootAnimation = cycle({"inferno", "era1", "era2", "era3", "era4", "era5", "random", "off"}, s.bootAnimation);
            break;
        case BootSound: s.bootSound = cycle({"builtin", "custom", "off"}, s.bootSound); break;
        case LaunchAnim:
            s.launchAnimation = cycle({"inferno", "era1", "era2", "era3", "era4", "era5", "random", "off"}, s.launchAnimation);
            break;
        case Volume:
            s.volume = std::max(0, std::min(100, (s.volume / 10) * 10 + delta * 10));
            s.soundEnabled = s.volume > 0;
            break;
        default: return;
    }
    if (app_.updateSettings(s)) app_.toast(app_.t("toast.saved"));
}

ScreenResult SettingsScreen::handle(const InputEvent& ev) {
    switch (ev.action) {
        case Action::Up:
            focus_ = (focus_ + RowCount - 1) % RowCount;
            resetArmed_ = false;
            break;
        case Action::Down:
            focus_ = (focus_ + 1) % RowCount;
            resetArmed_ = false;
            break;
        case Action::Left: change(-1); break;
        case Action::Right: change(+1); break;
        case Action::Confirm:
            if (focus_ == Reset) {
                if (!resetArmed_) {
                    resetArmed_ = true;
                    app_.toast(app_.t("settings.resetConfirm"));
                } else {
                    resetArmed_ = false;
                    if (app_.updateSettings(Settings())) app_.toast(app_.t("settings.resetDone"));
                }
            } else if (focus_ == BootPreview) {
                const std::string a = app_.settings().bootAnimation;
                app_.startBoot(a == "off" ? "random" : a);
            } else if (focus_ == ImportWav) {
                importWav();
            } else {
                change(+1);
            }
            break;
        case Action::Back:
            resetArmed_ = false;
            return ScreenResult::pop();
        default: break;
    }
    return ScreenResult::none();
}

void SettingsScreen::update(const UiContext& ctx) {
    highlightTarget_ = 330.f + focus_ * 60.f;
    highlightY_ = (highlightY_ < 0 || !ctx.animate) ? highlightTarget_
                                                     : approach(highlightY_, highlightTarget_, ctx.dt, 18.f * ctx.animSpeed);
    if (std::fabs(highlightY_ - highlightTarget_) < 0.3f) highlightY_ = highlightTarget_;
}

bool SettingsScreen::animating() const { return highlightY_ != highlightTarget_; }

void SettingsScreen::draw(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& th = ctx.theme;
    const Fonts& f = ctx.fonts;
    const Settings& s = app_.settings();

    drawPageHeader(ctx, app_, app_.t("module.settings"), app_.t("settings.sub"));

    const float listW = 1100, rowY0 = 330, rowStep = 60, rowH = 54;
    const bool hasWav = app_.sysFs().exists(app_.customBootSoundPath());
    std::string values[RowCount] = {
        s.language == "de" ? "Deutsch" : "English",
        app_.theme().name(s.language),
        app_.t("settings.anim." + s.animations),
        s.confirmButton == "cross" ? app_.t("settings.confirm.cross") : app_.t("settings.confirm.circle"),
        s.bootAnimation == "random" || s.bootAnimation == "off" ? app_.t("settings.boot." + s.bootAnimation)
                                                                : app_.t("boot.style." + s.bootAnimation),
        app_.t("settings.sound." + s.bootSound) + (s.bootSound == "custom" && !hasWav ? " (" + app_.t("settings.wav.missing") + ")" : ""),
        s.launchAnimation == "random" || s.launchAnimation == "off" ? app_.t("settings.boot." + s.launchAnimation)
                                                                    : app_.t("boot.style." + s.launchAnimation),
        s.volume == 0 ? app_.t("settings.volumeOff") : std::to_string(s.volume) + " %",
        "",
        hasWav ? app_.t("settings.wav.present") : "",
        resetArmed_ ? app_.t("settings.resetArmed") : "",
    };
    const char* labels[RowCount] = {"settings.language", "settings.theme", "settings.animations",
                                    "settings.confirmButton", "settings.bootAnim", "settings.bootSound", "settings.launchAnim",
                                    "settings.volume", "settings.bootPreview", "settings.importWav", "settings.reset"};

    Rect hl{kMarginX, highlightY_, listW, rowH};
    r.fillRect(hl, withAlpha(th.surfaceAlt, 0.95f), 16);
    drawFocusRing(r, th, hl, 16, ctx.time);

    for (int i = 0; i < RowCount; ++i) {
        const float y = rowY0 + i * rowStep;
        const float ty = y + (rowH - 36) / 2;
        const bool action = i == Reset || i == BootPreview || i == ImportWav;
        if (i != focus_) r.fillRect({kMarginX, y, listW, rowH}, withAlpha(th.surface, th.surfaceOpacity * 0.85f), 16);
        r.drawText(f.bodyBold, app_.t(labels[i]), kMarginX + 32, ty, i == Reset ? th.error : th.text);
        if (!action) {
            const float vx = kMarginX + listW - 32;
            r.drawText(f.bodyBold, values[i], vx - 30, ty, th.text, TextAlign::Right);
            float vw = r.measureText(f.bodyBold, values[i]);
            r.drawText(f.bodyBold, "<", vx - 30 - vw - 22, ty, withAlpha(th.text, i == focus_ ? 0.9f : 0.3f), TextAlign::Right);
            r.drawText(f.bodyBold, ">", vx, ty, withAlpha(th.text, i == focus_ ? 0.9f : 0.3f), TextAlign::Right);
        } else if (!values[i].empty()) {
            r.drawText(f.small, values[i], kMarginX + listW - 32, ty + 4, i == Reset ? th.warn : th.textMuted, TextAlign::Right);
        }
    }

    // ---- theme preview + storage info
    const Rect pv{kMarginX + listW + 32, 330, kScreenW - 2 * kMarginX - listW - 32, 470};
    drawPanel(r, th, pv, th.cornerRadius);
    r.drawText(f.label, app_.t("settings.preview"), pv.x + 28, pv.y + 28, th.textMuted);
    r.drawText(f.title, th.name(s.language), pv.x + 28, pv.y + 58, th.text);
    const Color sw[] = {th.background, th.surface, th.accent, th.accent2, th.text};
    for (int i = 0; i < 5; ++i)
        r.fillRect({pv.x + 28 + i * 64.f, pv.y + 120, 52, 52}, sw[i], 14), r.strokeRect({pv.x + 28 + i * 64.f, pv.y + 120, 52, 52}, th.line, 1.5f, 14);
    float gx = pv.x + 28;
    for (size_t i = 0; i < th.tileGradients.size() && i < 6; ++i) {
        r.gradientRect({gx, pv.y + 200, 96, 70}, th.tileGradients[i].first, th.tileGradients[i].second, 12);
        gx += 106;
        if (gx + 96 > pv.x + pv.w - 20) break;
    }
    r.drawText(f.small, app_.t("settings.themeCount") + " " + std::to_string(app_.themes().themes().size()),
               pv.x + 28, pv.y + 300, th.textMuted);
    r.drawText(f.label, app_.t("settings.storedAt"), pv.x + 28, pv.y + 360, th.textMuted);
    r.drawText(f.small, app_.dataRoot() + "/settings.json", pv.x + 28, pv.y + 390, th.text);
}

}  // namespace henu
