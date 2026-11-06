// PS4 HEN ULTIMATE - platform/sdl/sdl_platform.cpp
// Desktop backend (macOS/Linux/Windows) for development: SDL2 shows the
// SoftwareRenderer canvas - exactly the pixels the PS4 shows - and maps
// keyboard and gamepads to the PS4 buttons. Needs only SDL2 (no ttf/image).
// Built with `make -f Makefile.host app`.
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

#include <cstdlib>
#include <string>

#include "gfx/soft_renderer.h"
#include "platform/platform.h"
#include "platform/posix_fs.h"

namespace henu {

namespace {

constexpr int kW = 1920;
constexpr int kH = 1080;

struct SdlDisplay {
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* tex = nullptr;

    void present(const Canvas& c) {
        SDL_UpdateTexture(tex, nullptr, c.data(), c.width() * 4);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    }
};

SdlDisplay g_display;

class SdlInput : public IInput {
public:
    SdlInput() {
        for (int i = 0; i < SDL_NumJoysticks(); ++i) open(i);
    }

    PadState poll() override {
        PadState s;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) s.quitRequested = true;
            else if (e.type == SDL_CONTROLLERDEVICEADDED && !pad_) open(e.cdevice.which);
            else if (e.type == SDL_CONTROLLERDEVICEREMOVED && pad_ &&
                     e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad_))) {
                SDL_GameControllerClose(pad_);
                pad_ = nullptr;
            }
        }
        uint32_t b = 0;
        if (pad_) {
            // SDL standard layout: A = bottom (Cross), B = right (Circle), X = left (Square), Y = top (Triangle)
            struct M { SDL_GameControllerButton sdl; Button us; } map[] = {
                {SDL_CONTROLLER_BUTTON_DPAD_UP, Button::Up},       {SDL_CONTROLLER_BUTTON_DPAD_DOWN, Button::Down},
                {SDL_CONTROLLER_BUTTON_DPAD_LEFT, Button::Left},   {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, Button::Right},
                {SDL_CONTROLLER_BUTTON_A, Button::Cross},          {SDL_CONTROLLER_BUTTON_B, Button::Circle},
                {SDL_CONTROLLER_BUTTON_X, Button::Square},         {SDL_CONTROLLER_BUTTON_Y, Button::Triangle},
                {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Button::L1},  {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Button::R1},
                {SDL_CONTROLLER_BUTTON_START, Button::Options},
            };
            for (const auto& m : map)
                if (SDL_GameControllerGetButton(pad_, m.sdl)) b |= static_cast<uint32_t>(m.us);
            s.lx = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
            s.ly = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
        }
        const Uint8* k = SDL_GetKeyboardState(nullptr);
        struct K { SDL_Scancode sc; Button us; } keys[] = {
            {SDL_SCANCODE_UP, Button::Up},            {SDL_SCANCODE_DOWN, Button::Down},
            {SDL_SCANCODE_LEFT, Button::Left},        {SDL_SCANCODE_RIGHT, Button::Right},
            {SDL_SCANCODE_RETURN, Button::Cross},     {SDL_SCANCODE_ESCAPE, Button::Circle},
            {SDL_SCANCODE_BACKSPACE, Button::Circle}, {SDL_SCANCODE_S, Button::Square},
            {SDL_SCANCODE_T, Button::Triangle},       {SDL_SCANCODE_Q, Button::L1},
            {SDL_SCANCODE_E, Button::R1},             {SDL_SCANCODE_O, Button::Options},
        };
        for (const auto& m : keys)
            if (k[m.sc]) b |= static_cast<uint32_t>(m.us);
        s.connected = true;
        s.buttons = b;
        return s;
    }

private:
    void open(int index) {
        if (!pad_ && SDL_IsGameController(index)) pad_ = SDL_GameControllerOpen(index);
    }
    SDL_GameController* pad_ = nullptr;
};

class NullSystemInfo : public ISystemInfo {
public:
    SystemSnapshot snapshot() override {
        SystemSnapshot s;
        s.firmware.why = "desktop build";
        return s;
    }
};

std::string envOr(const char* name, const char* def) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : def;
}

}  // namespace

Result<Platform> createPlatform() {
    using R = Result<Platform>;
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0)
        return R::fail(ErrorCode::Unsupported, std::string("SDL_Init: ") + SDL_GetError());
    g_display.win = SDL_CreateWindow("PS4 HEN ULTIMATE (Desktop)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     kW / 2, kH / 2, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!g_display.win) return R::fail(ErrorCode::Unsupported, std::string("SDL_CreateWindow: ") + SDL_GetError());
    g_display.ren = SDL_CreateRenderer(g_display.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_display.ren) g_display.ren = SDL_CreateRenderer(g_display.win, -1, 0);
    if (!g_display.ren) return R::fail(ErrorCode::Unsupported, std::string("SDL_CreateRenderer: ") + SDL_GetError());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    SDL_RenderSetLogicalSize(g_display.ren, kW, kH);
    // Canvas pixels are 0xAARRGGBB words = SDL_PIXELFORMAT_ARGB8888.
    g_display.tex = SDL_CreateTexture(g_display.ren, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, kW, kH);
    if (!g_display.tex) return R::fail(ErrorCode::Unsupported, std::string("SDL_CreateTexture: ") + SDL_GetError());

    Platform p;
    p.fs.reset(new PosixFileSystem);
    auto* r = new SoftwareRenderer(*p.fs, kW, kH);
    r->setPresenter([](const Canvas& c) { g_display.present(c); });
    p.renderer.reset(r);
    p.input.reset(new SdlInput);
    p.system.reset(new NullSystemInfo);
    p.bundleRoot = envOr("HENU_BUNDLE", ".");
    // Console paths ("/data/...", "/mnt/usb0") are mapped below HENU_SYSROOT.
    p.sysRoot = envOr("HENU_SYSROOT", "build/host-root");
    p.dataRoot = "/data/PS4HENULTIMATE";
    return R::ok(std::move(p));
}

void platformSleepMs(int ms) { SDL_Delay(static_cast<Uint32>(ms)); }

}  // namespace henu
