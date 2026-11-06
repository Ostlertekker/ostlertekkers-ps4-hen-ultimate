// OSTLERTEKKERS HEN - cheats/cheat_edit.h
// Editing GoldHEN cheat files (JSON and SHN): remove or rename single cheats.
// `index` counts cheats exactly like parseCheatFile() (entries without a
// name are skipped), so it matches CheatFile::cheats. The result is the new
// file content; writing (with backup) is the caller's job. MC4 is encrypted
// and cannot be edited.
#pragma once

#include <string>

#include "core/result.h"

namespace henu {

enum class CheatEditOp { Remove, Rename };

struct CheatEdit {
    CheatEditOp op = CheatEditOp::Remove;
    size_t index = 0;
    std::string newName;   // Rename only, must not be empty
};

// fileName decides the format (.json / .shn). UTF-16 SHN comes back as UTF-8.
Result<std::string> editCheatFile(const std::string& fileName, const std::string& bytes, const CheatEdit& edit);

}  // namespace henu
