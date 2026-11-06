// PS4 HEN ULTIMATE - core/capability.h
// Central registry: every feature reports what it can actually do on this
// system. The UI reads only this registry to decide what to show/lock.
#pragma once

#include <map>
#include <string>
#include <vector>

namespace henu {

enum class CapStatus {
    Available,       // works and was tested
    Limited,         // works with a stated limitation
    Unverified,      // code exists but not confirmed on console
    Unavailable,     // not possible on this system (reason given)
    NotImplemented,  // not built yet
};

const char* toString(CapStatus s);              // "available", "limited", ...
bool parseCapStatus(const std::string& s, CapStatus& out);

struct Capability {
    std::string id;        // e.g. "cheats.import", "goldhen.detect"
    CapStatus status = CapStatus::NotImplemented;
    std::string reason;    // required for Limited/Unavailable/Unverified
};

class CapabilityRegistry {
public:
    // Registers or updates a capability. Missing reason for a status that
    // requires one is replaced by a generic text so the UI never shows blank.
    void set(const std::string& id, CapStatus status, const std::string& reason = "");

    // Unknown ids are reported as NotImplemented (fail closed).
    Capability get(const std::string& id) const;

    // True only for Available and Limited.
    bool usable(const std::string& id) const;

    // Module status = worst status of its children ("cheats" <- "cheats.*"),
    // or the module's own entry if it has no children.
    CapStatus moduleStatus(const std::string& moduleId) const;

    std::vector<Capability> all() const;

private:
    std::map<std::string, Capability> caps_;
};

}  // namespace henu
