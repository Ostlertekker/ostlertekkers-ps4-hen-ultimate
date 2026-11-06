// PS4 HEN ULTIMATE - cheats/cheat_naming.h
// Parsing/creating GoldHEN cheat file names, title IDs, regions and the
// compatibility decision. Pure logic, fully unit-tested on host.
#pragma once

#include <string>

#include "cheats/cheat_types.h"
#include "core/result.h"

namespace henu {

// Title ID: 4 uppercase letters + 5 digits, e.g. "CUSA01234".
bool isValidTitleId(const std::string& s);

// Parses GoldHEN cheat file names: "{TITLEID}_{VERSION}[_{anything}].{json|shn|mc4}",
// e.g. "CUSA01234_01.01.json", "CUSA01234_01.01_2.json",
// "CUSA00018_01.21_default_mp.elf.json". Only a bare file name is accepted.
Result<CheatFileId> parseCheatFileName(const std::string& fileName);

// Inverse of parseCheatFileName.
std::string makeCheatFileName(const CheatFileId& id);

// Region from a content ID prefix ("UP9000-CUSA..." -> US).
// UP=US, EP=EU, JP=JP, HP=Asia; anything else = Unknown.
Region regionFromContentId(const std::string& contentId);

// Compatibility rule (Master-Prompt §4): only "Compatible" when title ID and
// version match exactly. installedTitleId empty = game not installed.
Compatibility checkCompatibility(const CheatFileId& cheat,
                                 const std::string& installedTitleId,
                                 const AppVersion& installedVersion);

// Keyword-based category guess from a cheat name (German + English).
CheatCategory categorize(const std::string& cheatName);

}  // namespace henu
