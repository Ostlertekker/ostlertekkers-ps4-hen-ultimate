// OSTLERTEKKERS HEN - platform/ps4/ps4_services.h
#pragma once

#include <memory>
#include <string>

#include "platform/platform.h"

namespace henu {

std::unique_ptr<IAudio> createPs4Audio();
std::unique_ptr<IPrivilege> createPs4Privilege();
std::unique_ptr<INetwork> createPs4Network(std::string& err);   // null on failure

}  // namespace henu
