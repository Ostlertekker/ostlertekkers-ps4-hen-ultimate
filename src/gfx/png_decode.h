// OSTLERTEKKERS HEN - gfx/png_decode.h
// Small, dependency-free PNG decoder (zlib inflate + PNG filters) for game
// icons (icon0.png). Supports 8-bit grey, grey+alpha, RGB, RGBA, palette and
// 16-bit variants (reduced to 8 bit). Interlaced images are rejected.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/result.h"

namespace henu {

struct Image {
    int w = 0, h = 0;
    std::vector<uint32_t> px;   // A8R8G8B8 (non-premultiplied)
};

// RFC 1950/1951. maxOut caps the output size (zip bombs).
Result<std::string> zlibInflate(const std::string& in, size_t maxOut);
// Raw deflate stream (no zlib header), e.g. from ZIP entries.
Result<std::string> rawInflate(const uint8_t* data, size_t len, size_t maxOut, size_t* consumed = nullptr);

Result<Image> decodePng(const std::string& file, int maxDim = 4096);

// Box/bilinear downscale to fit within maxW x maxH (keeps aspect ratio).
Image scaleImage(const Image& src, int w, int h);

}  // namespace henu
