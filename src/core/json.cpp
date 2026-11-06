// OSTLERTEKKERS HEN - core/json.cpp
#include "core/json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace henu {

Json Json::boolean(bool b) { Json j; j.type_ = Type::Bool; j.b_ = b; return j; }
Json Json::number(double d) { Json j; j.type_ = Type::Number; j.d_ = d; return j; }
Json Json::string(std::string s) { Json j; j.type_ = Type::String; j.s_ = std::move(s); return j; }
Json Json::array(Array a) { Json j; j.type_ = Type::Array; j.a_ = std::move(a); return j; }
Json Json::object(Object o) { Json j; j.type_ = Type::Object; j.o_ = std::move(o); return j; }

int Json::asInt(int def) const {
    if (!isNumber() || !std::isfinite(d_)) return def;
    if (d_ > 2147483647.0 || d_ < -2147483648.0) return def;
    return static_cast<int>(d_);
}

const std::string& Json::asString() const {
    static const std::string empty;
    return isString() ? s_ : empty;
}

std::string Json::asString(const std::string& def) const { return isString() ? s_ : def; }

const Json::Array& Json::asArray() const {
    static const Array empty;
    return isArray() ? a_ : empty;
}

const Json::Object& Json::asObject() const {
    static const Object empty;
    return isObject() ? o_ : empty;
}

const Json& Json::get(const std::string& key) const {
    static const Json null;
    if (!isObject()) return null;
    for (const auto& kv : o_)
        if (kv.first == key) return kv.second;
    return null;
}

bool Json::has(const std::string& key) const {
    if (!isObject()) return false;
    for (const auto& kv : o_)
        if (kv.first == key) return true;
    return false;
}

void Json::set(const std::string& key, Json value) {
    if (!isObject()) return;
    for (auto& kv : o_) {
        if (kv.first == key) { kv.second = std::move(value); return; }
    }
    o_.emplace_back(key, std::move(value));
}

void Json::push(Json value) {
    if (isArray()) a_.push_back(std::move(value));
}

// ---------------------------------------------------------------- writer
namespace {
void escapeTo(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

void dumpTo(std::string& out, const Json& j, int indent, int level) {
    auto nl = [&](int lvl) {
        if (indent <= 0) return;
        out += '\n';
        out.append(static_cast<size_t>(indent * lvl), ' ');
    };
    switch (j.type()) {
        case Json::Type::Null: out += "null"; break;
        case Json::Type::Bool: out += j.asBool() ? "true" : "false"; break;
        case Json::Type::Number: {
            double d = j.asNumber();
            if (!std::isfinite(d)) { out += "null"; break; }
            char buf[32];
            if (d == std::floor(d) && std::fabs(d) < 1e15) std::snprintf(buf, sizeof(buf), "%.0f", d);
            else std::snprintf(buf, sizeof(buf), "%.17g", d);
            out += buf;
            break;
        }
        case Json::Type::String: escapeTo(out, j.asString()); break;
        case Json::Type::Array: {
            const auto& a = j.asArray();
            if (a.empty()) { out += "[]"; break; }
            out += '[';
            for (size_t i = 0; i < a.size(); ++i) {
                if (i) out += ',';
                nl(level + 1);
                dumpTo(out, a[i], indent, level + 1);
            }
            nl(level);
            out += ']';
            break;
        }
        case Json::Type::Object: {
            const auto& o = j.asObject();
            if (o.empty()) { out += "{}"; break; }
            out += '{';
            for (size_t i = 0; i < o.size(); ++i) {
                if (i) out += ',';
                nl(level + 1);
                escapeTo(out, o[i].first);
                out += indent > 0 ? ": " : ":";
                dumpTo(out, o[i].second, indent, level + 1);
            }
            nl(level);
            out += '}';
            break;
        }
    }
}
}  // namespace

std::string Json::dump(int indent) const {
    std::string out;
    dumpTo(out, *this, indent, 0);
    if (indent > 0) out += '\n';
    return out;
}

// ---------------------------------------------------------------- parser
namespace {
class Parser {
public:
    Parser(const std::string& t, const JsonLimits& l) : t_(t), lim_(l) {}

    Result<Json> run() {
        skipWs();
        Json v;
        if (!value(v, 0)) return fail();
        skipWs();
        if (p_ != t_.size()) { err_ = "trailing characters"; return fail(); }
        return Result<Json>::ok(std::move(v));
    }

private:
    Result<Json> fail() {
        return Result<Json>::fail(ErrorCode::ParseError,
                                  err_ + " at offset " + std::to_string(p_));
    }

    void skipWs() {
        while (p_ < t_.size() && (t_[p_] == ' ' || t_[p_] == '\n' || t_[p_] == '\r' || t_[p_] == '\t')) ++p_;
    }

    bool lit(const char* s) {
        size_t n = 0;
        while (s[n]) ++n;
        if (t_.compare(p_, n, s) != 0) return false;
        p_ += n;
        return true;
    }

    bool value(Json& out, int depth) {
        if (depth > lim_.maxDepth) { err_ = "nesting too deep"; return false; }
        if (p_ >= t_.size()) { err_ = "unexpected end"; return false; }
        char c = t_[p_];
        if (c == '{') return object(out, depth);
        if (c == '[') return array(out, depth);
        if (c == '"') { std::string s; if (!str(s)) return false; out = Json::string(std::move(s)); return true; }
        if (c == 't') { if (lit("true")) { out = Json::boolean(true); return true; } }
        else if (c == 'f') { if (lit("false")) { out = Json::boolean(false); return true; } }
        else if (c == 'n') { if (lit("null")) { out = Json(); return true; } }
        else if (c == '-' || (c >= '0' && c <= '9')) return num(out);
        err_ = "unexpected character";
        return false;
    }

    bool num(Json& out) {
        size_t start = p_;
        if (t_[p_] == '-') ++p_;
        if (p_ >= t_.size()) { err_ = "bad number"; return false; }
        if (t_[p_] == '0') { ++p_; }
        else if (t_[p_] >= '1' && t_[p_] <= '9') { while (p_ < t_.size() && isdig(t_[p_])) ++p_; }
        else { err_ = "bad number"; return false; }
        if (p_ < t_.size() && t_[p_] == '.') {
            ++p_;
            if (p_ >= t_.size() || !isdig(t_[p_])) { err_ = "bad fraction"; return false; }
            while (p_ < t_.size() && isdig(t_[p_])) ++p_;
        }
        if (p_ < t_.size() && (t_[p_] == 'e' || t_[p_] == 'E')) {
            ++p_;
            if (p_ < t_.size() && (t_[p_] == '+' || t_[p_] == '-')) ++p_;
            if (p_ >= t_.size() || !isdig(t_[p_])) { err_ = "bad exponent"; return false; }
            while (p_ < t_.size() && isdig(t_[p_])) ++p_;
        }
        std::string s = t_.substr(start, p_ - start);
        out = Json::number(std::strtod(s.c_str(), nullptr));
        return true;
    }

    static bool isdig(char c) { return c >= '0' && c <= '9'; }

    static void utf8(std::string& o, unsigned cp) {
        if (cp < 0x80) o += static_cast<char>(cp);
        else if (cp < 0x800) { o += static_cast<char>(0xC0 | (cp >> 6)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            o += static_cast<char>(0xE0 | (cp >> 12));
            o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            o += static_cast<char>(0xF0 | (cp >> 18));
            o += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool hex4(unsigned& v) {
        if (p_ + 4 > t_.size()) { err_ = "bad \\u escape"; return false; }
        v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = t_[p_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
            else { err_ = "bad \\u escape"; return false; }
        }
        return true;
    }

    bool str(std::string& out) {
        ++p_;  // opening quote
        while (true) {
            if (p_ >= t_.size()) { err_ = "unterminated string"; return false; }
            unsigned char c = static_cast<unsigned char>(t_[p_++]);
            if (c == '"') return true;
            if (c < 0x20) { err_ = "control character in string"; return false; }
            if (c != '\\') { out += static_cast<char>(c); continue; }
            if (p_ >= t_.size()) { err_ = "bad escape"; return false; }
            char e = t_[p_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        unsigned lo;
                        if (p_ + 2 > t_.size() || t_[p_] != '\\' || t_[p_ + 1] != 'u') { err_ = "lone surrogate"; return false; }
                        p_ += 2;
                        if (!hex4(lo) || lo < 0xDC00 || lo > 0xDFFF) { err_ = "bad surrogate pair"; return false; }
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) { err_ = "lone surrogate"; return false; }
                    utf8(out, cp);
                    break;
                }
                default: err_ = "bad escape"; return false;
            }
        }
    }

    bool array(Json& out, int depth) {
        ++p_;
        Json::Array a;
        skipWs();
        if (p_ < t_.size() && t_[p_] == ']') { ++p_; out = Json::array(std::move(a)); return true; }
        while (true) {
            skipWs();
            Json v;
            if (!value(v, depth + 1)) return false;
            a.push_back(std::move(v));
            skipWs();
            if (p_ >= t_.size()) { err_ = "unterminated array"; return false; }
            if (t_[p_] == ',') { ++p_; continue; }
            if (t_[p_] == ']') { ++p_; break; }
            err_ = "expected ',' or ']'";
            return false;
        }
        out = Json::array(std::move(a));
        return true;
    }

    bool object(Json& out, int depth) {
        ++p_;
        Json::Object o;
        skipWs();
        if (p_ < t_.size() && t_[p_] == '}') { ++p_; out = Json::object(std::move(o)); return true; }
        while (true) {
            skipWs();
            if (p_ >= t_.size() || t_[p_] != '"') { err_ = "expected key"; return false; }
            std::string key;
            if (!str(key)) return false;
            skipWs();
            if (p_ >= t_.size() || t_[p_] != ':') { err_ = "expected ':'"; return false; }
            ++p_;
            skipWs();
            Json v;
            if (!value(v, depth + 1)) return false;
            bool dup = false;
            for (auto& kv : o) if (kv.first == key) { kv.second = std::move(v); dup = true; break; }
            if (!dup) o.emplace_back(std::move(key), std::move(v));
            skipWs();
            if (p_ >= t_.size()) { err_ = "unterminated object"; return false; }
            if (t_[p_] == ',') { ++p_; continue; }
            if (t_[p_] == '}') { ++p_; break; }
            err_ = "expected ',' or '}'";
            return false;
        }
        out = Json::object(std::move(o));
        return true;
    }

    const std::string& t_;
    JsonLimits lim_;
    size_t p_ = 0;
    std::string err_;
};
}  // namespace

Result<Json> parseJson(const std::string& text, const JsonLimits& limits) {
    if (text.size() > limits.maxBytes)
        return Result<Json>::fail(ErrorCode::ParseError, "file too large");
    // Skip UTF-8 BOM
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        std::string rest = text.substr(3);
        Parser p(rest, limits);
        return p.run();
    }
    Parser p(text, limits);
    return p.run();
}

}  // namespace henu
