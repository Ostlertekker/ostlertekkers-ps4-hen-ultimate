// PS4 HEN ULTIMATE - core/sfo.h
// Reader/writer for PS4 param.sfo files (PSF format).
// Layout: header "\0PSF", version, keyTableStart, dataTableStart, count;
// then `count` index entries {u16 keyOffset, u16 fmt, u32 len, u32 maxLen,
// u32 dataOffset}. fmt 0x0204 = UTF-8 string, 0x0404 = int32,
// 0x0004 = UTF-8 "special" (not NUL-terminated).
#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "core/result.h"

namespace henu {

struct Sfo {
    std::map<std::string, std::string> strings;
    std::map<std::string, int64_t> ints;

    std::string str(const std::string& key, const std::string& def = "") const {
        auto it = strings.find(key);
        return it == strings.end() ? def : it->second;
    }
};

Result<Sfo> parseSfo(const std::string& bytes);
std::string writeSfo(const Sfo& sfo);   // for tests and demo data

}  // namespace henu
