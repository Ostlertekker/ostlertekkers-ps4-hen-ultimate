// PS4 HEN ULTIMATE - core/capability.cpp
#include "core/capability.h"

namespace henu {

const char* toString(CapStatus s) {
    switch (s) {
        case CapStatus::Available:      return "available";
        case CapStatus::Limited:        return "limited";
        case CapStatus::Unverified:     return "unverified";
        case CapStatus::Unavailable:    return "unavailable";
        case CapStatus::NotImplemented: return "not_implemented";
    }
    return "not_implemented";
}

bool parseCapStatus(const std::string& s, CapStatus& out) {
    static const std::pair<const char*, CapStatus> table[] = {
        {"available", CapStatus::Available},
        {"limited", CapStatus::Limited},
        {"unverified", CapStatus::Unverified},
        {"unavailable", CapStatus::Unavailable},
        {"not_implemented", CapStatus::NotImplemented},
    };
    for (const auto& e : table) {
        if (s == e.first) { out = e.second; return true; }
    }
    return false;
}

namespace {
// Higher = worse. Used to aggregate a module's status.
int severity(CapStatus s) {
    switch (s) {
        case CapStatus::Available:      return 0;
        case CapStatus::Limited:        return 1;
        case CapStatus::Unverified:     return 2;
        case CapStatus::NotImplemented: return 3;
        case CapStatus::Unavailable:    return 4;
    }
    return 4;
}

bool needsReason(CapStatus s) {
    return s == CapStatus::Limited || s == CapStatus::Unverified || s == CapStatus::Unavailable;
}
}  // namespace

void CapabilityRegistry::set(const std::string& id, CapStatus status, const std::string& reason) {
    Capability c;
    c.id = id;
    c.status = status;
    c.reason = reason;
    if (c.reason.empty() && needsReason(status)) {
        c.reason = "no reason given";
    }
    caps_[id] = c;
}

Capability CapabilityRegistry::get(const std::string& id) const {
    auto it = caps_.find(id);
    if (it != caps_.end()) return it->second;
    Capability c;
    c.id = id;
    c.status = CapStatus::NotImplemented;
    return c;
}

bool CapabilityRegistry::usable(const std::string& id) const {
    CapStatus s = get(id).status;
    return s == CapStatus::Available || s == CapStatus::Limited;
}

CapStatus CapabilityRegistry::moduleStatus(const std::string& moduleId) const {
    const std::string prefix = moduleId + ".";
    bool anyChild = false;
    int worst = -1;
    CapStatus result = CapStatus::NotImplemented;
    for (const auto& kv : caps_) {
        if (kv.first.compare(0, prefix.size(), prefix) == 0) {
            anyChild = true;
            int sev = severity(kv.second.status);
            if (sev > worst) { worst = sev; result = kv.second.status; }
        }
    }
    if (anyChild) return result;
    return get(moduleId).status;
}

std::vector<Capability> CapabilityRegistry::all() const {
    std::vector<Capability> out;
    out.reserve(caps_.size());
    for (const auto& kv : caps_) out.push_back(kv.second);
    return out;
}

}  // namespace henu
