// OSTLERTEKKERS HEN - cheats/cheat_edit.cpp
#include "cheats/cheat_edit.h"

#include "cheats/cheat_db.h"
#include "cheats/cheat_naming.h"
#include "core/json.h"

namespace henu {

namespace {

using R = Result<std::string>;

std::string xmlEscape(const std::string& s) {
    std::string o;
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default: o += c;
        }
    }
    return o;
}

// Value range [begin, end) of attribute `name` inside tag text, or false.
bool attrRange(const std::string& s, size_t tagBegin, size_t tagEnd, const std::string& name, size_t& b, size_t& e) {
    size_t p = tagBegin;
    const std::string key = " " + name + "=\"";
    while ((p = s.find(key, p)) != std::string::npos && p < tagEnd) {
        b = p + key.size();
        e = s.find('"', b);
        if (e == std::string::npos || e > tagEnd) return false;
        return true;
    }
    return false;
}

R editJson(const std::string& bytes, const CheatEdit& ed) {
    auto j = parseJson(bytes);
    if (!j) return R::fail(ErrorCode::ParseError, j.error().message);
    Json root = j.value();
    if (!root.isObject() || !root.get("mods").isArray()) return R::fail(ErrorCode::ParseError, "no 'mods' array");
    Json::Array mods;
    size_t named = 0;
    bool hit = false;
    for (const auto& m : root.get("mods").asArray()) {
        const bool counts = !m.get("name").asString().empty();
        if (counts && named++ == ed.index) {
            hit = true;
            if (ed.op == CheatEditOp::Remove) continue;
            Json copy = m;
            copy.set("name", Json::string(ed.newName));
            mods.push_back(copy);
            continue;
        }
        mods.push_back(m);
    }
    if (!hit) return R::fail(ErrorCode::InvalidArgument, "cheat index out of range");
    root.set("mods", Json::array(std::move(mods)));
    return R::ok(root.dump(4) + "\n");
}

R editShn(const std::string& raw, const CheatEdit& ed) {
    std::string s = toUtf8(raw);
    size_t p = 0, named = 0;
    while ((p = s.find("<Cheat ", p)) != std::string::npos) {
        const size_t tagEnd = s.find('>', p);
        if (tagEnd == std::string::npos) break;
        size_t vb = 0, ve = 0;
        const bool hasText = attrRange(s, p, tagEnd, "Text", vb, ve) && ve > vb;
        if (hasText && named++ == ed.index) {
            if (ed.op == CheatEditOp::Rename) {
                s.replace(vb, ve - vb, xmlEscape(ed.newName));
                return R::ok(s);
            }
            size_t end;
            if (s[tagEnd - 1] == '/') {
                end = tagEnd + 1;
            } else {
                const size_t close = s.find("</Cheat>", tagEnd);
                if (close == std::string::npos) return R::fail(ErrorCode::ParseError, "<Cheat> without </Cheat>");
                end = close + 8;
            }
            // take the indentation before the element and the line break after it
            size_t begin = p;
            while (begin > 0 && (s[begin - 1] == ' ' || s[begin - 1] == '\t')) --begin;
            if (end < s.size() && s[end] == '\r') ++end;
            if (end < s.size() && s[end] == '\n') ++end;
            s.erase(begin, end - begin);
            return R::ok(s);
        }
        p = tagEnd;
    }
    return R::fail(ErrorCode::InvalidArgument, "cheat index out of range");
}

}  // namespace

Result<std::string> editCheatFile(const std::string& fileName, const std::string& bytes, const CheatEdit& ed) {
    auto id = parseCheatFileName(fileName);
    if (!id) return R::fail(id.error().code, id.error().message);
    if (ed.op == CheatEditOp::Rename && ed.newName.empty()) return R::fail(ErrorCode::InvalidArgument, "empty name");
    R out = R::fail(ErrorCode::InvalidArgument, "MC4 files are encrypted and cannot be edited");
    if (id.value().format == CheatFormat::Json) out = editJson(bytes, ed);
    else if (id.value().format == CheatFormat::Shn) out = editShn(bytes, ed);
    if (!out) return out;
    // the result must still be a valid cheat file
    auto check = parseCheatFile(fileName, out.value());
    if (!check) return R::fail(ErrorCode::ParseError, "edited file is invalid: " + check.error().message);
    return out;
}

}  // namespace henu
