// PS4 HEN ULTIMATE - gfx/soft_renderer.h
// IRenderer on top of Canvas + BitmapFont. Used on every platform: the PS4
// backend copies the canvas into the VideoOut framebuffer, the desktop
// backend into an SDL texture, the preview tool into a PNG.
#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gfx/canvas.h"
#include "gfx/font.h"
#include "platform/platform.h"

namespace henu {

class SoftwareRenderer : public IRenderer {
public:
    using Presenter = std::function<void(const Canvas&)>;

    SoftwareRenderer(IFileSystem& fs, int width = 1920, int height = 1080);

    void setPresenter(Presenter p) { present_ = std::move(p); }
    const Canvas& canvas() const { return canvas_; }

    void beginFrame(Color clear) override;
    void endFrame() override;
    void fillRect(const Rect& r, Color c, float radius) override;
    void gradientRect(const Rect& r, Color from, Color to, float radius) override;
    void strokeRect(const Rect& r, Color c, float width, float radius) override;
    Result<TextureId> loadPng(const std::string& path) override;
    void drawTexture(TextureId t, const Rect& dst, float alpha) override;
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
    void flushClear();
    IFileSystem& fs_;
    bool pendingClear_ = false;
    Color clearColor_;
    Canvas canvas_;
    std::unique_ptr<Canvas> layer_;
    uint64_t layerKey_ = 0, recordingKey_ = 0;
    std::vector<BitmapFont> fonts_;
    std::vector<std::string> texts_;
    Presenter present_;
    uint64_t frames_ = 0;
};

// PNG encoder (stored/uncompressed deflate, no zlib needed). For previews.
std::string encodePng(const Canvas& c);

}  // namespace henu
