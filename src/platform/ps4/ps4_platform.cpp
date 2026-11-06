// OSTLERTEKKERS HEN - platform/ps4/ps4_platform.cpp
// Native PS4 backend using only the OpenOrbis toolchain:
//   VideoOut (2 linear A8R8G8B8 framebuffers, vsync flips), ScePad,
//   UserService, SystemService. Drawing is done by SoftwareRenderer.
//
// API sources (docs/API_NACHWEIS.md P1-P6):
//   include/orbis/Pad.h (scePadInit/scePadOpen/scePadReadState)
//   OpenOrbis samples/_common/graphics.cpp (VideoOut setup, linear buffers
//   placed width*height*4 bytes apart, direct memory type 3 / prot 0x33)
// Only compiled by the PS4 build (`make`), never on the host.
#include <orbis/NetCtl.h>
#include <orbis/Pad.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/VideoOut.h>
#include <orbis/libkernel.h>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <thread>

#include "gfx/soft_renderer.h"
#include "platform/platform.h"
#include "platform/posix_fs.h"
#include "platform/ps4/ps4_services.h"

namespace henu {

namespace {

constexpr int kWidth = 1920;
constexpr int kHeight = 1080;
constexpr int kBuffers = 2;
constexpr size_t kFrameBytes = static_cast<size_t>(kWidth) * kHeight * 4;
constexpr size_t kAlign = 0x200000;   // 2 MiB

// Literal values as used by the OpenOrbis graphics sample.
constexpr int kVideoUserMain = 0xFF;
constexpr int kVideoBusMain = 0;
constexpr uint32_t kPixelFormatA8R8G8B8Srgb = 0x80000000u;
constexpr int kTilingLinear = 1;
constexpr int kFlipModeVsync = 1;
constexpr int kMemTypeWcGarlic = 3;
constexpr int kProtCpuGpuRw = 0x33;

class Ps4Display {
public:
    bool init(std::string& err) {
        handle_ = sceVideoOutOpen(kVideoUserMain, kVideoBusMain, 0, nullptr);
        if (handle_ < 0) { err = "sceVideoOutOpen failed"; return false; }

        const size_t total = ((kFrameBytes * kBuffers) + kAlign - 1) & ~(kAlign - 1);
        off_t phys = 0;
        if (sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(), total, kAlign, kMemTypeWcGarlic, &phys) != 0) {
            err = "sceKernelAllocateDirectMemory failed";
            return false;
        }
        void* mem = nullptr;
        if (sceKernelMapDirectMemory(&mem, total, kProtCpuGpuRw, 0, phys, kAlign) != 0) {
            err = "sceKernelMapDirectMemory failed";
            return false;
        }
        for (int i = 0; i < kBuffers; ++i) {
            buffers_[i] = static_cast<uint8_t*>(mem) + kFrameBytes * i;
            std::memset(buffers_[i], 0, kFrameBytes);
        }

        OrbisVideoOutBufferAttribute attr;
        std::memset(&attr, 0, sizeof(attr));
        sceVideoOutSetBufferAttribute(&attr, kPixelFormatA8R8G8B8Srgb, kTilingLinear, 0, kWidth, kHeight, kWidth);
        if (sceVideoOutRegisterBuffers(handle_, 0, reinterpret_cast<void**>(buffers_), kBuffers, &attr) != 0) {
            err = "sceVideoOutRegisterBuffers failed";
            return false;
        }
        sceVideoOutSetFlipRate(handle_, 0);   // 60 Hz
        return true;
    }

    // Copies the canvas into the back buffer, flips on vsync and waits for it.
    void present(const Canvas& c) {
        if (handle_ < 0) return;
        std::memcpy(buffers_[current_], c.data(), kFrameBytes);
        ++frameId_;
        sceVideoOutSubmitFlip(handle_, current_, kFlipModeVsync, frameId_);
        // Wait until this flip was processed before the buffer is reused.
        for (int guard = 0; guard < 200; ++guard) {
            OrbisVideoOutFlipStatus st;
            std::memset(&st, 0, sizeof(st));
            sceVideoOutGetFlipStatus(handle_, &st);
            if (static_cast<int64_t>(st.flipArg) >= frameId_) break;
            sceKernelUsleep(1000);
        }
        current_ = (current_ + 1) % kBuffers;
    }

private:
    int handle_ = -1;
    uint8_t* buffers_[kBuffers] = {nullptr, nullptr};
    int current_ = 0;
    int64_t frameId_ = 0;
};

class Ps4Input : public IInput {
public:
    Ps4Input() {
        int32_t user = -1;
        sceUserServiceInitialize(nullptr);   // may report "already initialized"
        if (sceUserServiceGetInitialUser(&user) != 0) {
            std::printf("[henu] sceUserServiceGetInitialUser failed\n");
            return;
        }
        if (scePadInit() != 0) {
            std::printf("[henu] scePadInit failed\n");
            return;
        }
        pad_ = scePadOpen(user, 0 /* standard port */, 0, nullptr);
        if (pad_ < 0) std::printf("[henu] scePadOpen failed: 0x%08x\n", static_cast<unsigned>(pad_));
    }

    PadState poll() override {
        PadState s;
        if (pad_ < 0) return s;
        OrbisPadData d;
        std::memset(&d, 0, sizeof(d));
        if (scePadReadState(pad_, &d) != 0 || !d.connected) return s;
        s.connected = true;
        struct M { uint32_t orbis; Button us; } map[] = {
            {ORBIS_PAD_BUTTON_UP, Button::Up},          {ORBIS_PAD_BUTTON_DOWN, Button::Down},
            {ORBIS_PAD_BUTTON_LEFT, Button::Left},      {ORBIS_PAD_BUTTON_RIGHT, Button::Right},
            {ORBIS_PAD_BUTTON_CROSS, Button::Cross},    {ORBIS_PAD_BUTTON_CIRCLE, Button::Circle},
            {ORBIS_PAD_BUTTON_SQUARE, Button::Square},  {ORBIS_PAD_BUTTON_TRIANGLE, Button::Triangle},
            {ORBIS_PAD_BUTTON_L1, Button::L1},          {ORBIS_PAD_BUTTON_R1, Button::R1},
            {ORBIS_PAD_BUTTON_L2, Button::L2},          {ORBIS_PAD_BUTTON_R2, Button::R2},
            {ORBIS_PAD_BUTTON_OPTIONS, Button::Options}, {ORBIS_PAD_BUTTON_TOUCH_PAD, Button::TouchPad},
        };
        for (const auto& m : map)
            if (d.buttons & m.orbis) s.buttons |= static_cast<uint32_t>(m.us);
        // Sticks are 0..255 with 128 = centre.
        s.lx = (static_cast<int>(d.leftStick.x) - 128) / 127.0f;
        s.ly = (static_cast<int>(d.leftStick.y) - 128) / 127.0f;
        s.rx = (static_cast<int>(d.rightStick.x) - 128) / 127.0f;
        s.ry = (static_cast<int>(d.rightStick.y) - 128) / 127.0f;
        return s;
    }

private:
    int32_t pad_ = -1;
};

class Ps4SystemInfo : public ISystemInfo {
public:
    Ps4SystemInfo() { netOk_ = sceNetCtlInit() >= 0; }

    SystemSnapshot snapshot() override {
        SystemSnapshot s;
        // Firmware: OrbisKernelSwVersion.Version is BCD, e.g. 0x13520001 -> "13.52"
        OrbisKernelSwVersion v;
        std::memset(&v, 0, sizeof(v));
        v.Size = sizeof(v);
        if (sceKernelGetSystemSwVersion(&v) == 0 && v.Version != 0) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%x.%02x", (v.Version >> 24) & 0xFF, (v.Version >> 16) & 0xFF);
            s.firmware = {true, buf, ""};
        } else {
            s.firmware.why = "sceKernelGetSystemSwVersion failed";
        }
        int32_t lang = -1;
        if (sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_LANG, &lang) == 0) {
            // PS4 language ids: 1 = English (US), 4 = German, 18 = English (UK) ...
            s.language = {true, lang == 4 ? "de" : (lang == 1 || lang == 18 ? "en" : "id " + std::to_string(lang)), ""};
        }
        int32_t enter = -1;
        if (sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN, &enter) == 0)
            s.circleIsConfirm = {true, enter == 0, ""};   // 0 = circle, 1 = cross
        if (netOk_) {
            OrbisNetCtlInfo info;
            std::memset(&info, 0, sizeof(info));
            if (sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS, &info) == 0 && info.ip_address[0]) {
                s.ipAddress = {true, std::string(info.ip_address), ""};
                s.networkUp = {true, std::string(info.ip_address) != "0.0.0.0", ""};
            }
        }
        if (!s.ipAddress.known) s.ipAddress.why = "not connected";
        return s;
    }

private:
    bool netOk_ = false;
};

Ps4Display g_display;

}  // namespace

Result<Platform> createPlatform() {
    using R = Result<Platform>;
    sceSystemServiceHideSplashScreen();

    std::string err;
    if (!g_display.init(err)) return R::fail(ErrorCode::Unsupported, err);

    Platform p;
    p.fs.reset(new PosixFileSystem);
    auto* renderer = new SoftwareRenderer(*p.fs, kWidth, kHeight);
    renderer->setPresenter([](const Canvas& c) { g_display.present(c); });
    // Apps get 6 of the 8 Jaguar cores; rasterize on all of them.
    {
        unsigned hw = std::thread::hardware_concurrency();
        renderer->setThreads(hw >= 2 && hw <= 8 ? static_cast<int>(std::min(hw, 6u)) : 6);
    }
    p.renderer.reset(renderer);
    p.input.reset(new Ps4Input);
    p.system.reset(new Ps4SystemInfo);
    p.audio = createPs4Audio();
    p.privilege = createPs4Privilege();
    std::string netErr;
    p.net = createPs4Network(netErr);
    if (!p.net) std::printf("[henu] network unavailable: %s\n", netErr.c_str());
    p.bundleRoot = "/app0";
    p.dataRoot = "/data/OSTLERTEKKERS_HEN";
    p.sysRoot = "";
    return R::ok(std::move(p));
}

void platformSleepMs(int ms) { sceKernelUsleep(static_cast<unsigned>(ms) * 1000u); }

}  // namespace henu
