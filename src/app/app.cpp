// OSTLERTEKKERS HEN - app/app.cpp
#include "app/app.h"

#include <algorithm>
#include <functional>

#include "app/screens.h"
#include "audio/audio.h"
#include "core/json.h"
#include "files/fs_util.h"
#include "ui/logo.h"

namespace henu {

App::App(Platform& platform, AppOptions opts) : platform_(platform), opts_(opts) {}
App::~App() { log_.flush(); }

const ModuleDef* App::module(const std::string& id) const {
    for (const auto& m : modules_)
        if (m.id == id) return &m;
    return nullptr;
}

void App::toast(const std::string& text) {
    toastText_ = text;
    toastUntil_ = time_ + 2.8;
    dirty_ = true;
}

const std::string& App::currentSection() const { return modules_[section_].id; }
size_t App::stackDepth() const { return stacks_[section_].size(); }

Status App::init() {
    if (!platform_.renderer || !platform_.input || !platform_.fs)
        return Status::fail(ErrorCode::InvalidArgument, "platform incomplete");
    if (!platform_.sysRoot.empty()) sysFs_.reset(new PrefixFileSystem(*platform_.fs, platform_.sysRoot));
    guard_.reset(new PathGuard(platform_.dataRoot));
    IFileSystem& fs = sysFs();

    // v0.4 and older used /data/PS4HENULTIMATE: move it once to the new name.
    // Both folders belong to this app; nothing else is touched.
    if (!platform_.dataRoot.empty() && !fs.exists(platform_.dataRoot) && fs.exists(kLegacyDataRoot))
        migrated_ = fs.rename(kLegacyDataRoot, platform_.dataRoot).isOk();

    // App data folder + log file (console path, below sysRoot on desktop)
    if (!platform_.dataRoot.empty()) {
        makeDirsUnchecked(fs, platform_.dataRoot);
        const std::string logDir = platform_.dataRoot + "/logs";
        if (fs.makeDir(logDir)) log_.attachFile(&fs, logDir);
    }
    if (migrated_) log_.info(std::string("moved old data folder ") + kLegacyDataRoot + " -> " + platform_.dataRoot);
    log_.info("OSTLERTEKKERS HEN starting (bundle=" + platform_.bundleRoot + ", data=" + platform_.dataRoot + ")");

    loadModules();
    loadLanguages();

    store_.reset(new SettingsStore(fs, platform_.dataRoot));
    std::string source;
    settings_ = store_->load(&source);
    log_.info("settings loaded from " + source);
    if (source == "defaults" && platform_.system) {
        // First start: follow the console's language and confirm button.
        SystemSnapshot snap = platform_.system->snapshot();
        if (snap.language.known) settings_.language = snap.language.value == "de" ? "de" : "en";
        if (snap.circleIsConfirm.known) settings_.confirmButton = snap.circleIsConfirm.value ? "circle" : "cross";
    }
    favs_.load(fs, platform_.dataRoot);

    themes_.scan(*platform_.fs, platform_.bundleRoot + "/assets/themes", "");
    if (!platform_.dataRoot.empty()) {
        ThemeCatalog user;   // user themes live in the console data folder
        user.scan(fs, "", platform_.dataRoot + "/themes");
        for (const auto& t : user.themes()) if (themes_.indexOf(t.id) < 0) themes_.addTheme(t);
    }
    for (const auto& e : themes_.errors()) log_.warn("theme " + e.file + ": " + e.message);
    log_.info(std::to_string(themes_.themes().size()) + " themes loaded");

    loadFonts();
    applyTheme();
    applyInput();
    i18n_.setLanguage(settings_.language);

    stacks_.clear();
    stacks_.resize(modules_.size());
    for (size_t i = 0; i < modules_.size(); ++i) stacks_[i].push_back(makeRoot(modules_[i].id));
    section_ = 0;
    top().onEnter();
    if (opts_.bootSequence) startBoot(settings_.bootAnimation);
    log_.flush();
    return Status::ok();
}

void App::loadModules() {
    modules_.clear();
    auto text = platform_.fs->readFile(platform_.bundleRoot + "/config/features.json", 64 * 1024);
    Json cfg;
    if (text) {
        auto j = parseJson(text.value());
        if (j) cfg = j.value();
        else log_.error("features.json: " + j.error().message);
    } else {
        log_.error("features.json: " + text.error().message);
    }
    for (const auto& m : cfg.get("modules").asArray()) {
        ModuleDef d;
        d.id = m.get("id").asString();
        d.order = m.get("order").asInt(99);
        d.note = m.get("note").asString();
        if (d.id.empty()) continue;
        CapStatus st;
        if (!parseCapStatus(m.get("status").asString(), st)) st = CapStatus::NotImplemented;
        // "available" in features.json means: confirmed on the console.
        d.status = st;
        modules_.push_back(d);
    }
    std::stable_sort(modules_.begin(), modules_.end(),
                     [](const ModuleDef& a, const ModuleDef& b) { return a.order < b.order; });
    if (modules_.empty() || modules_.front().id != "home") {
        // Without a valid config, keep a usable minimum (fail closed for the rest).
        log_.warn("module list missing or invalid; using minimal set");
        modules_.clear();
        modules_.push_back({"home", 1, CapStatus::Unverified, ""});
        modules_.push_back({"settings", 2, CapStatus::Unverified, ""});
    }
    for (const auto& m : modules_) {
        std::string reason;
        if (m.status == CapStatus::Unverified) reason = "not yet tested on console";
        caps_.set(m.id, m.status, reason);
    }
}

void App::loadLanguages() {
    for (const char* lang : {"de", "en"}) {
        const std::string path = platform_.bundleRoot + "/assets/lang/" + lang + ".json";
        auto text = platform_.fs->readFile(path, 256 * 1024);
        if (!text) { log_.error(path + ": " + text.error().message); continue; }
        Status st = i18n_.loadLanguage(lang, text.value());
        if (!st) log_.error(st.error().message);
    }
}

void App::loadFonts() {
    IRenderer& r = *platform_.renderer;
    const std::string dir = platform_.bundleRoot + "/assets/fonts/";
    auto load = [&](const char* file, float size) -> FontId {
        auto f = r.loadFont(dir + file, size);
        if (!f) { log_.error("font " + std::string(file) + ": " + f.error().message); return 0; }
        return f.value();
    };
    // Pre-rendered atlases (scripts/make_fonts.py); sizes must match.
    fonts_.small = load("inter-regular-20.hfnt", 20);
    fonts_.body = load("inter-regular-26.hfnt", 26);
    fonts_.bodyBold = load("inter-semibold-26.hfnt", 26);
    fonts_.label = load("inter-bold-18.hfnt", 18);
    fonts_.title = load("inter-bold-34.hfnt", 34);
    fonts_.tile = load("inter-bold-28.hfnt", 28);
    fonts_.h2 = load("inter-bold-44.hfnt", 44);
    fonts_.h1 = load("inter-bold-72.hfnt", 72);
}

void App::applyTheme() {
    theme_ = &themes_.getOrFallback(settings_.themeId);
    if (theme_->id != settings_.themeId)
        log_.warn("theme '" + settings_.themeId + "' not found, using '" + theme_->id + "'");
}

void App::applyInput() {
    InputConfig c;
    c.circleConfirms = settings_.confirmButton == "circle";
    c.stickDeadzone = settings_.stickDeadzone;
    c.repeatDelay = settings_.repeatDelayMs / 1000.0f;
    c.repeatRate = settings_.repeatRateMs / 1000.0f;
    input_.configure(c);
}

bool App::updateSettings(const Settings& s) {
    Settings old = settings_;
    settings_ = s;
    i18n_.setLanguage(settings_.language);
    applyTheme();
    applyInput();
    Status st = store_->save(settings_);
    if (!st) {
        log_.error("saving settings failed: " + st.error().message);
        toast(t("toast.saveFailed"));
        return false;
    }
    return true;
}

Screen& App::top() { return *stacks_[section_].back(); }

std::unique_ptr<Screen> App::makeRoot(const std::string& id) {
    if (id == "home") return std::unique_ptr<Screen>(new HomeScreen(*this));
    if (id == "settings") return std::unique_ptr<Screen>(new SettingsScreen(*this));
    if (id == "games") return std::unique_ptr<Screen>(new GamesScreen(*this, false));
    if (id == "homebrew") return std::unique_ptr<Screen>(new GamesScreen(*this, true));
    if (id == "cheats") return std::unique_ptr<Screen>(new CheatsScreen(*this));
    if (id == "themes") return std::unique_ptr<Screen>(new ThemesScreen(*this));
    if (id == "goldhen") return std::unique_ptr<Screen>(new GoldhenScreen(*this));
    if (id == "system") return std::unique_ptr<Screen>(new SystemScreen(*this));
    if (id == "network") return std::unique_ptr<Screen>(new NetworkScreen(*this));
    if (id == "files") return std::unique_ptr<Screen>(new FilesScreen(*this));
    if (id == "backup") return std::unique_ptr<Screen>(new BackupScreen(*this));
    if (id == "update") return std::unique_ptr<Screen>(new UpdateScreen(*this));
    if (id == "diag") return std::unique_ptr<Screen>(new DiagScreen(*this));
    return std::unique_ptr<Screen>(new ModuleScreen(*this, id));
}

SystemSnapshot App::systemInfo() {
    return platform_.system ? platform_.system->snapshot() : SystemSnapshot();
}

void App::ensureScanned(bool force) {
    if (force || !games_.scanned()) {
        // Installed titles live outside the app sandbox: read them with the
        // GoldHEN SDK escape for the duration of the scan only.
        Status st = Status::ok();
        for (auto& kv : icons_) platform_.renderer->freeTexture(kv.second);
        icons_.clear();
        {
            ElevatedScope elevated(platform_.privilege.get());
            if (elevated.attempted())
                gameScanNote_ = (elevated.ok() ? "ok: " : "failed: ") + elevated.why();
            st = games_.scan(sysFs());
        }
        for (const auto& n : games_.notes()) log_.warn("games: " + n);
        if (!gameScanNote_.empty()) log_.info("games: sandbox access = " + gameScanNote_);
        log_.info("games: " + std::to_string(games_.all().size()) + (st ? "" : " (" + st.error().message + ")"));
    }
    if (force || !cheats_.scanned()) {
        cheats_.scan(sysFs(), cheatRoot());
        log_.info("cheat files: " + std::to_string(cheats_.files().size()) + ", errors: " +
                  std::to_string(cheats_.errors().size()));
    }
}

TextureId App::iconTexture(const GameInfo& g) {
    auto it = icons_.find(g.titleId);
    if (it != icons_.end()) return it->second;
    TextureId id = g.icon ? platform_.renderer->createTexture(kIconSize, kIconSize, g.icon->data()) : 0;
    icons_[g.titleId] = id;
    return id;
}

void App::rescanThemes() {
    themes_.scan(*platform_.fs, platform_.bundleRoot + "/assets/themes", "");
    ThemeCatalog user;
    user.scan(sysFs(), "", platform_.dataRoot + "/themes");
    for (const auto& t : user.themes()) if (themes_.indexOf(t.id) < 0) themes_.addTheme(t);
    for (const auto& e : user.errors()) log_.warn("theme " + e.file + ": " + e.message);
    applyTheme();
    dirty_ = true;
}

void App::reloadUserData() {
    settings_ = store_->load();
    favs_.load(sysFs(), platform_.dataRoot);
    i18n_.setLanguage(settings_.language);
    rescanThemes();
    applyInput();
    dirty_ = true;
}

bool App::goToSectionById(const std::string& id) {
    for (size_t i = 0; i < modules_.size(); ++i)
        if (modules_[i].id == id) { goToSection(i); return true; }
    return false;
}

void App::goToSection(size_t index) {
    if (index >= modules_.size() || index == section_) return;
    section_ = index;
    top().onEnter();
}

void App::inject(const InputEvent& ev) {
    if (launch_) {   // same rules as real input during the start animation
        if (ev.action == Action::Back && !launchRequested_) cancelLaunch();
        return;
    }
    handle(ev);
}

void App::cancelLaunch() {
    if (!launch_) return;
    if (platform_.audio) platform_.audio->stop();
    log_.info("launch cancelled: " + launch_->titleId());
    launch_.reset();
    dirty_ = true;
}

void App::handle(const InputEvent& ev) {
    dirty_ = true;
    const size_t n = modules_.size();
    if (ev.action == Action::PrevSection) { goToSection((section_ + n - 1) % n); return; }
    if (ev.action == Action::NextSection) { goToSection((section_ + 1) % n); return; }

    ScreenResult r = top().handle(ev);
    switch (r.kind) {
        case ScreenResult::Kind::None:
            // Back on a section root returns to Home (like the PS5 home flow).
            if (ev.action == Action::Back && stacks_[section_].size() == 1 && section_ != 0) goToSection(0);
            break;
        case ScreenResult::Kind::Push:
            stacks_[section_].push_back(std::move(r.next));
            top().onEnter();
            break;
        case ScreenResult::Kind::Replace:
            if (stacks_[section_].size() > 1) stacks_[section_].pop_back();
            stacks_[section_].push_back(std::move(r.next));
            top().onEnter();
            break;
        case ScreenResult::Kind::Pop:
            if (stacks_[section_].size() > 1) {
                stacks_[section_].pop_back();
                top().onEnter();
            } else if (section_ != 0) {
                goToSection(0);
            }
            break;
        case ScreenResult::Kind::GoToSection:
            goToSectionById(r.sectionId);
            break;
        case ScreenResult::Kind::Quit:
            quit_ = true;
            break;
    }
}

void App::startBoot(const std::string& setting) {
    const std::string style = resolveBootStyle(setting, static_cast<unsigned>(time_ * 1000) + static_cast<unsigned>(games_.all().size()));
    if (style.empty()) return;
    boot_.reset(new BootSequence(style));
    bootStart_ = time_;
    playBootSound(style);
    dirty_ = true;
}

void App::startLaunch(const GameInfo& g) {
    if (!platform_.launcher) { toast(t("launch.unavailable")); return; }
    log_.info("launch requested: " + g.titleId);
    std::string style = resolveBootStyle(settings_.launchAnimation, static_cast<unsigned>(time_ * 1000));
    launchRequested_ = false;
    launchStatus_.clear();
    launchStart_ = time_;
    if (style.empty()) style = "inferno";   // "off": no animation, but the black frame is still drawn
    launch_.reset(new LaunchSequence(style, g.name, g.titleId, iconTexture(g)));
    if (settings_.launchAnimation == "off") launchStart_ = time_ - launch_->launchAt();
    else if (platform_.audio && settings_.soundEnabled) {
        Pcm pcm = synthLaunchSound(bootSoundVariant(style));
        applyVolume(pcm, settings_.volume / 100.0f);
        platform_.audio->playStereo48k(std::move(pcm.samples));
    }
    dirty_ = true;
}

void App::playBootSound(const std::string& styleId) {
    if (!platform_.audio || !settings_.soundEnabled || settings_.bootSound == "off") return;
    Pcm pcm;
    if (settings_.bootSound == "custom") {
        auto bytes = sysFs().readFile(customBootSoundPath(), 64u << 20);
        if (bytes) {
            auto wav = decodeWav(bytes.value(), 30);
            if (wav) pcm = std::move(wav.value());
            else log_.warn("boot.wav: " + wav.error().message);
        } else {
            log_.warn("boot.wav: " + bytes.error().message);
        }
    }
    if (pcm.samples.empty()) pcm = synthBootSound(bootSoundVariant(styleId));
    applyVolume(pcm, settings_.volume / 100.0f);
    platform_.audio->playStereo48k(std::move(pcm.samples));
}

bool App::frame(float dt) {
    if (dt < 0) dt = 0;
    if (dt > 0.25f) dt = 0.25f;   // after a stall, do not jump animations
    time_ += dt;

    PadState pad = platform_.input->poll();
    if (pad.quitRequested) quit_ = true;
    auto events = input_.update(pad, dt);
    if (boot_) {
        const double bt = time_ - bootStart_;
        const bool skip = !events.empty() && bt > 0.3;
        if (skip && platform_.audio) platform_.audio->stop();
        if (skip || boot_->finished(bt)) {
            boot_.reset();
            dirty_ = true;
        } else {
            IRenderer& r = *platform_.renderer;
            r.beginFrame({0, 0, 0, 255});
            boot_->draw(r, fonts_, bt);
            r.endFrame();
            drawn_ = true;
            return !quit_;
        }
        events.clear();
    }
    if (launch_) {
        double lt = time_ - launchStart_;
        if (!launchRequested_)
            for (const auto& ev : events)
                if (ev.action == Action::Back) {
                    cancelLaunch();
                    return !quit_;
                }
        IRenderer& r = *platform_.renderer;
        r.beginFrame({0, 0, 0, 255});
        launch_->draw(r, fonts_, lt, launchStatus_);
        r.endFrame();
        drawn_ = true;
        if (lt >= launch_->launchAt() && !launchRequested_) {
            launchRequested_ = true;
            Status st = platform_.launcher ? platform_.launcher->launch(launch_->titleId())
                                           : Status::fail(ErrorCode::Unsupported, "no launcher");
            if (!st) {
                log_.error("launch " + launch_->titleId() + " failed: " + st.error().message);
                launch_.reset();
                toast(t("launch.failed") + " " + st.error().message);
                dirty_ = true;
                return !quit_;
            }
            log_.info("launch " + launch_->titleId() + ": ok");
            launchStatus_ = t("launch.starting");
        }
        // The system switches to the game; if we are still here after a while
        // (e.g. the user returned), go back to the menu.
        if (launchRequested_ && lt > launch_->launchAt() + 6) {
            launch_.reset();
            dirty_ = true;
        }
        return !quit_;
    }
    for (const auto& ev : events) handle(ev);

    const bool animate = settings_.animations != "off";
    const float speedFactor = settings_.animations == "reduced" ? 0.5f : 1.0f;
    UiContext ctx{*platform_.renderer, *theme_, fonts_, time_, dt, animate,
                  theme_->animSpeed * speedFactor};
    // A screen that was busy before update() (animation, network job) gets one
    // more frame after it settles, so its final state is shown.
    Screen* before = &top();
    const bool wasAnimating = before->animating();
    before->update(ctx);
    if (&top() != before) dirty_ = true;

    // Redraw only when something changed: input, animation, toast.
    const bool toastVisible = time_ < toastUntil_ && !toastText_.empty();
    const bool needDraw = dirty_ || wasAnimating || top().animating() || toastVisible || toastVisible != toastShown_;
    drawn_ = needDraw;
    if (!needDraw) {
        if (time_ >= nextFlush_) { log_.flush(); nextFlush_ = time_ + 5; }
        return !quit_;
    }
    dirty_ = false;
    toastShown_ = toastVisible;

    IRenderer& r = *platform_.renderer;
    r.beginFrame(theme_->background);
    // The background is static per theme: render once, then reuse.
    const uint64_t bgKey = static_cast<uint64_t>(std::hash<std::string>{}(theme_->id)) | 1u;
    if (!r.useCachedLayer(bgKey)) {
        drawBackground(r, *theme_, time_, animate);
        r.endLayer();
    }
    top().draw(ctx);
    drawChrome(ctx);
    r.endFrame();

    if (time_ >= nextFlush_) { log_.flush(); nextFlush_ = time_ + 5; }
    return !quit_;
}

void App::drawChrome(const UiContext& ctx) {
    IRenderer& r = ctx.renderer;
    const Theme& th = ctx.theme;
    const Fonts& f = ctx.fonts;

    // ---- top bar: brand
    r.gradientRect({kMarginX, 38, 56, 56}, mix(th.accent, th.background, 0.15f), mix(th.accent, th.background, 0.6f), 18);
    LogoStyle logo;
    logo.star = th.focus;
    logo.ring = th.focus;
    drawPentagram(r, kMarginX + 28, 66, 22, logo);
    r.drawText(f.bodyBold, kBrandName, kMarginX + 76, 42, th.text);
    r.drawText(f.label, t("app.subtitle"), kMarginX + 76, 76, th.textMuted);

    // ---- section switcher: L1  prev  CURRENT  next  R1
    const size_t n = modules_.size();
    const std::string& prev = t("module." + modules_[(section_ + n - 1) % n].id);
    const std::string& cur = t("module." + modules_[section_].id);
    const std::string& next = t("module." + modules_[(section_ + 1) % n].id);
    const float cx = kScreenW / 2;
    const float curW = r.measureText(f.bodyBold, cur);
    r.fillRect({cx - curW / 2 - 26, 40, curW + 52, 52}, withAlpha(th.text, 0.10f), 26);
    r.drawText(f.bodyBold, cur, cx, 52, th.text, TextAlign::Center);
    const float sideGap = curW / 2 + 52;
    r.drawText(f.small, prev, cx - sideGap, 56, th.textMuted, TextAlign::Right);
    r.drawText(f.small, next, cx + sideGap, 56, th.textMuted, TextAlign::Left);
    const float prevW = r.measureText(f.small, prev), nextW = r.measureText(f.small, next);
    drawPill(r, f, "L1", cx - sideGap - prevW - 76, 49, withAlpha(th.text, 0.12f), th.text);
    drawPill(r, f, "R1", cx + sideGap + nextW + 24, 49, withAlpha(th.text, 0.12f), th.text);

    // ---- right: language
    const std::string lang = settings_.language == "de" ? "DE" : "EN";
    float lw = pillWidth(r, f, lang);
    drawPill(r, f, lang, kScreenW - kMarginX - lw, 49, withAlpha(th.accent, 0.25f), th.text);

    r.fillRect({kMarginX, 120, kScreenW - 2 * kMarginX, 1.5f}, th.line, 0);

    // ---- bottom hints
    float x = kMarginX;
    const float y = kScreenH - 62;
    const bool swapped = settings_.confirmButton == "circle";
    for (const auto& h : top().hints()) {
        std::string glyph = h.first;
        if (glyph == "@confirm") glyph = swapped ? "\xE2\x97\x8B" : "\xE2\x9C\x95";   // ○ / ✕
        x += drawButtonHint(r, f, th, glyph, t(h.second), x, y);
    }
    if (stacks_[section_].size() > 1 || section_ != 0)
        x += drawButtonHint(r, f, th, swapped ? "\xE2\x9C\x95" : "\xE2\x97\x8B", t("hint.back"), x, y);
    drawButtonHint(r, f, th, "L1", t("hint.section"), x, y);
    r.drawText(f.small, std::string("v") + kAppVersion, kScreenW - kMarginX, y + 5, withAlpha(th.textMuted, 0.7f), TextAlign::Right);

    // ---- toast
    if (time_ < toastUntil_ && !toastText_.empty()) {
        float a = static_cast<float>(std::min(1.0, (toastUntil_ - time_) * 3.0));
        float w = r.measureText(f.bodyBold, toastText_) + 56;
        Rect rc{kScreenW - kMarginX - w, 136, w, 60};
        r.fillRect(rc, withAlpha(th.text, 0.95f * a), 16);
        r.drawText(f.bodyBold, toastText_, rc.x + w / 2, rc.y + 15, withAlpha(th.background, a), TextAlign::Center);
    }
}

}  // namespace henu
