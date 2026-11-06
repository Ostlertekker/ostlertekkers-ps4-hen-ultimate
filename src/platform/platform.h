// PS4 HEN ULTIMATE - platform/platform.h
// Platform abstraction. Modules use ONLY these interfaces.
//   platform/ps4/  -> OpenOrbis + SDL-PS4 implementation (Phase C)
//   platform/host/ -> desktop implementation for development and tests
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/result.h"

namespace henu {

// ---------------------------------------------------------------- Rendering
struct Color { uint8_t r = 0, g = 0, b = 0, a = 255; };
struct Rect { float x = 0, y = 0, w = 0, h = 0; };  // logical 1920x1080 units

using TextureId = uint32_t;   // 0 = invalid
using FontId = uint32_t;      // 0 = invalid

enum class TextAlign { Left, Center, Right };

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void beginFrame(Color clear) = 0;
    virtual void endFrame() = 0;  // presents; blocks on vsync

    virtual void fillRect(const Rect& r, Color c, float cornerRadius = 0) = 0;
    virtual void gradientRect(const Rect& r, Color from, Color to, float cornerRadius = 0) = 0;
    virtual void strokeRect(const Rect& r, Color c, float width, float cornerRadius = 0) = 0;

    virtual Result<TextureId> loadPng(const std::string& path) = 0;
    virtual void drawTexture(TextureId t, const Rect& dst, float alpha = 1.0f) = 0;
    virtual void freeTexture(TextureId t) = 0;

    virtual Result<FontId> loadFont(const std::string& ttfPath, float pixelSize) = 0;
    virtual void drawText(FontId f, const std::string& utf8, float x, float y, Color c,
                          TextAlign align = TextAlign::Left) = 0;
    virtual float measureText(FontId f, const std::string& utf8) = 0;

    // Physical output size; layout is scaled from 1920x1080.
    virtual int outputWidth() const = 0;
    virtual int outputHeight() const = 0;

    // Optional layer cache for static content (e.g. the background). If a
    // layer with `key` exists it is drawn and true is returned. Otherwise
    // false: draw the content, then call endLayer() to store it.
    virtual bool useCachedLayer(uint64_t key) { (void)key; return false; }
    virtual void endLayer() {}
};

// ---------------------------------------------------------------- Input
enum class Button : uint32_t {
    None = 0,
    Up = 1u << 0, Down = 1u << 1, Left = 1u << 2, Right = 1u << 3,
    Cross = 1u << 4, Circle = 1u << 5, Square = 1u << 6, Triangle = 1u << 7,
    L1 = 1u << 8, R1 = 1u << 9, L2 = 1u << 10, R2 = 1u << 11,
    Options = 1u << 12, TouchPad = 1u << 13,
};

struct PadState {
    uint32_t buttons = 0;        // bitmask of Button
    float lx = 0, ly = 0;        // left stick, -1..1
    float rx = 0, ry = 0;        // right stick, -1..1
    bool connected = false;
    bool quitRequested = false;  // host window closed; never set on PS4
};

class IInput {
public:
    virtual ~IInput() = default;
    virtual PadState poll() = 0;
};

// ---------------------------------------------------------------- Files
struct DirEntry {
    std::string name;
    bool isDir = false;
    uint64_t size = 0;
    int64_t mtime = 0;
};

struct SpaceInfo { uint64_t total = 0, free = 0; };

// Raw file access. Callers that write MUST go through files::SafeFs, which
// enforces PathGuard. Implementations do not check permissions themselves.
class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    virtual bool exists(const std::string& path) = 0;
    virtual Result<std::vector<DirEntry>> list(const std::string& dir) = 0;
    virtual Result<std::string> readFile(const std::string& path, size_t maxBytes) = 0;
    virtual Status writeFile(const std::string& path, const std::string& data) = 0;
    virtual Status makeDir(const std::string& path) = 0;
    virtual Status rename(const std::string& from, const std::string& to) = 0;
    virtual Status remove(const std::string& path) = 0;
    virtual Result<SpaceInfo> space(const std::string& mountPath) = 0;
    // Default: whole file in memory. PosixFileSystem streams instead.
    virtual Status copyFile(const std::string& from, const std::string& to, uint64_t maxBytes = 64ull << 20) {
        auto data = readFile(from, static_cast<size_t>(maxBytes));
        if (!data) return Status::fail(data.error().code, data.error().message);
        return writeFile(to, data.value());
    }
};

// Maps console-absolute paths ("/user/app/...") below a host directory.
// Used by desktop/preview builds; on the PS4 the prefix is empty.
class PrefixFileSystem : public IFileSystem {
public:
    PrefixFileSystem(IFileSystem& inner, std::string prefix) : in_(inner), pre_(std::move(prefix)) {}
    bool exists(const std::string& p) override { return in_.exists(pre_ + p); }
    Result<std::vector<DirEntry>> list(const std::string& d) override { return in_.list(pre_ + d); }
    Result<std::string> readFile(const std::string& p, size_t m) override { return in_.readFile(pre_ + p, m); }
    Status writeFile(const std::string& p, const std::string& d) override { return in_.writeFile(pre_ + p, d); }
    Status makeDir(const std::string& p) override { return in_.makeDir(pre_ + p); }
    Status rename(const std::string& a, const std::string& b) override { return in_.rename(pre_ + a, pre_ + b); }
    Status remove(const std::string& p) override { return in_.remove(pre_ + p); }
    Result<SpaceInfo> space(const std::string& p) override { return in_.space(pre_ + p); }
    Status copyFile(const std::string& a, const std::string& b, uint64_t m) override {
        return in_.copyFile(pre_ + a, pre_ + b, m);
    }

private:
    IFileSystem& in_;
    std::string pre_;
};

// ---------------------------------------------------------------- System
// Every value is optional: "not available" must be shown when unset.
template <typename T>
struct Maybe {
    bool known = false;
    T value{};
    std::string why;  // reason when !known
};

struct SystemSnapshot {
    Maybe<std::string> firmware;      // e.g. "13.52" (as reported by the system)
    Maybe<uint64_t> uptimeSeconds;
    Maybe<std::string> runningTitleId;
    Maybe<std::string> ipAddress;
    Maybe<bool> networkUp;
    Maybe<std::string> language;      // system UI language
    Maybe<bool> circleIsConfirm;      // JP/Asia button layout
};

class ISystemInfo {
public:
    virtual ~ISystemInfo() = default;
    virtual SystemSnapshot snapshot() = 0;
};

// ---------------------------------------------------------------- Network
struct HttpResponse {
    int status = 0;
    std::string body;
};

class INetwork {
public:
    virtual ~INetwork() = default;
    // HTTPS only; certificate verification must be on. maxBytes caps memory.
    virtual Result<HttpResponse> get(const std::string& httpsUrl, size_t maxBytes) = 0;
    virtual Status download(const std::string& httpsUrl, const std::string& destPath,
                            size_t maxBytes) = 0;
};

// ---------------------------------------------------------------- Audio
class IAudio {
public:
    virtual ~IAudio() = default;
    virtual Result<uint32_t> loadWav(const std::string& path) = 0;
    virtual void play(uint32_t soundId, float volume) = 0;
};

// ---------------------------------------------------------------- Bundle
struct Platform {
    std::unique_ptr<IRenderer> renderer;
    std::unique_ptr<IInput> input;
    std::unique_ptr<IFileSystem> fs;
    std::unique_ptr<ISystemInfo> system;
    std::unique_ptr<INetwork> net;   // may be null -> network features unavailable
    std::unique_ptr<IAudio> audio;   // may be null -> sound unavailable

    std::string bundleRoot;   // read-only app files (assets), e.g. "/app0"
    std::string dataRoot;     // writable app data, e.g. "/data/PS4HENULTIMATE"
    std::string sysRoot;      // prefix for console paths ("" on PS4, demo folder on desktop)
};

// Implemented once per platform backend (platform/ps4 or platform/sdl).
Result<Platform> createPlatform();
void platformSleepMs(int ms);

}  // namespace henu
