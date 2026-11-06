// OSTLERTEKKERS HEN - gfx/soft_renderer.h
// IRenderer on top of Canvas + BitmapFont. Used on every platform: the PS4
// backend copies the canvas into the VideoOut framebuffer, the desktop
// backend into an SDL texture, the preview tool into a PNG.
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "gfx/canvas.h"
#include "gfx/font.h"
#include "platform/platform.h"

namespace henu {

class SoftwareRenderer : public IRenderer {
public:
    using Presenter = std::function<void(const Canvas&)>;

    SoftwareRenderer(IFileSystem& fs, int width = 1920, int height = 1080);
    ~SoftwareRenderer() override;

    void setPresenter(Presenter p) { present_ = std::move(p); }
    // Pending draw commands are executed first.
    const Canvas& canvas() const;
    // Number of threads that rasterize a frame (1 = only the calling thread).
    // Every thread owns whole horizontal bands, so results are identical.
    void setThreads(int n);
    int threads() const { return static_cast<int>(workers_.size()) + 1; }

    void beginFrame(Color clear) override;
    void endFrame() override;
    void fillRect(const Rect& r, Color c, float radius) override;
    void gradientRect(const Rect& r, Color from, Color to, float radius) override;
    void strokeRect(const Rect& r, Color c, float width, float radius) override;
    Result<TextureId> loadPng(const std::string& path) override;
    TextureId createTexture(int w, int h, const uint32_t* argb) override;
    void drawTexture(TextureId t, const Rect& dst, float alpha, float cornerRadius) override;
    void fillPath(const std::vector<std::vector<Pt>>& contours, Color c) override;
    void freeTexture(TextureId t) override;
    Result<FontId> loadFont(const std::string& path, float size) override;
    void drawText(FontId f, const std::string& utf8, float x, float y, Color c, TextAlign align) override;
    float measureText(FontId f, const std::string& utf8) override;
    int outputWidth() const override { return canvas_.width(); }
    int outputHeight() const override { return canvas_.height(); }
    bool useCachedLayer(uint64_t key) override;
    void endLayer() override;

    // Text drawn in the current frame (tests, diagnostics).
    const std::vector<std::string>& texts() const { return texts_; }
    uint64_t frameCount() const { return frames_; }

private:
    struct Cmd {
        enum Kind { Clear, Fill, Gradient, Stroke, Path, Text, Texture, CopyLayer } kind;
        Rect r{};
        Color a{}, b{};
        float f = 0, f2 = 0;
        uint32_t id = 0;
        int y0 = 0, y1 = 0;   // rows touched
        std::string text;
        std::vector<std::vector<Pt>> path;
    };
    void flushClear();
    void push(Cmd c);
    void flush() const;                 // runs the command list (const: canvas() is const)
    void run(Canvas& band, const Cmd& c) const;
    void workerLoop();
    IFileSystem& fs_;
    bool pendingClear_ = false;
    Color clearColor_;
    mutable Canvas canvas_;
    mutable std::vector<Cmd> cmds_;
    // thread pool
    std::vector<std::thread> workers_;
    mutable std::mutex m_;
    mutable std::condition_variable cv_, doneCv_;
    mutable uint64_t job_ = 0;
    mutable int busy_ = 0;
    mutable std::atomic<int> nextBand_{0};
    mutable int bands_ = 0;
    bool stop_ = false;
    std::unique_ptr<Canvas> layer_;
    uint64_t layerKey_ = 0, recordingKey_ = 0;
    std::vector<BitmapFont> fonts_;
    struct Tex { int w = 0, h = 0; std::vector<uint32_t> px; };
    std::vector<Tex> textures_;   // id = index + 1; freed slots keep w == 0
    std::vector<std::string> texts_;
    Presenter present_;
    uint64_t frames_ = 0;
};

// PNG encoder (stored/uncompressed deflate, no zlib needed). For previews.
std::string encodePng(const Canvas& c);

}  // namespace henu
