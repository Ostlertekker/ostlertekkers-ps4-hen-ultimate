// OSTLERTEKKERS HEN - core/sha256.h (FIPS 180-4)
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace henu {

class Sha256 {
public:
    Sha256();
    void update(const void* data, size_t len);
    std::string hexDigest();   // finalizes

private:
    void block(const uint8_t* p);
    uint32_t h_[8];
    uint8_t buf_[64];
    size_t bufLen_ = 0;
    uint64_t total_ = 0;
};

std::string sha256Hex(const std::string& data);

}  // namespace henu
