// PS4 HEN ULTIMATE - app/favorites.cpp
#include "app/favorites.h"

#include <algorithm>

#include "core/json.h"

namespace henu {

void Favorites::load(IFileSystem& fs, const std::string& dataRoot) {
    fs_ = &fs;
    path_ = dataRoot + "/favorites.json";
    games_.clear();
    cheats_.clear();
    recent_.clear();
    auto text = fs.readFile(path_, 512 * 1024);
    if (!text) return;
    auto j = parseJson(text.value());
    if (!j) return;
    for (const auto& v : j.value().get("games").asArray()) if (v.isString()) games_.insert(v.asString());
    for (const auto& v : j.value().get("cheatFiles").asArray()) if (v.isString()) cheats_.insert(v.asString());
    for (const auto& v : j.value().get("recentCheatFiles").asArray())
        if (v.isString() && recent_.size() < 20) recent_.push_back(v.asString());
}

Status Favorites::save() {
    if (!fs_) return Status::fail(ErrorCode::InvalidArgument, "favorites not loaded");
    Json j = Json::object();
    j.set("schemaVersion", Json::number(1));
    Json g = Json::array(), c = Json::array(), r = Json::array();
    for (const auto& s : games_) g.push(Json::string(s));
    for (const auto& s : cheats_) c.push(Json::string(s));
    for (const auto& s : recent_) r.push(Json::string(s));
    j.set("games", g);
    j.set("cheatFiles", c);
    j.set("recentCheatFiles", r);
    const std::string tmp = path_ + ".tmp";
    Status st = fs_->writeFile(tmp, j.dump(2));
    if (!st) return st;
    if (fs_->exists(path_)) fs_->remove(path_);
    return fs_->rename(tmp, path_);
}

void Favorites::toggleGame(const std::string& id) {
    if (!games_.erase(id)) games_.insert(id);
    save();
}

void Favorites::toggleCheat(const std::string& f) {
    if (!cheats_.erase(f)) cheats_.insert(f);
    save();
}

void Favorites::addRecent(const std::string& f) {
    recent_.erase(std::remove(recent_.begin(), recent_.end(), f), recent_.end());
    recent_.insert(recent_.begin(), f);
    if (recent_.size() > 20) recent_.resize(20);
    save();
}

}  // namespace henu
