// PS4 HEN ULTIMATE - core/json.h
// Small strict JSON reader/writer. Designed for untrusted import files:
// size and depth limits, no exceptions, clear error positions.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/result.h"

namespace henu {

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;  // keeps key order

    Json() = default;                       // null
    static Json boolean(bool b);
    static Json number(double d);
    static Json string(std::string s);
    static Json array(Array a = {});
    static Json object(Object o = {});

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool(bool def = false) const { return isBool() ? b_ : def; }
    double asNumber(double def = 0) const { return isNumber() ? d_ : def; }
    int asInt(int def = 0) const;
    const std::string& asString() const;                 // "" if not a string
    std::string asString(const std::string& def) const;
    const Array& asArray() const;                        // empty if not an array
    const Object& asObject() const;                      // empty if not an object

    // Object access. get() returns a shared null for missing keys.
    const Json& get(const std::string& key) const;
    bool has(const std::string& key) const;
    void set(const std::string& key, Json value);         // object only; replaces
    void push(Json value);                                // array only

    std::string dump(int indent = 2) const;               // indent 0 = compact

private:
    Type type_ = Type::Null;
    bool b_ = false;
    double d_ = 0;
    std::string s_;
    Array a_;
    Object o_;
};

struct JsonLimits {
    size_t maxBytes = 4 * 1024 * 1024;
    int maxDepth = 64;
};

Result<Json> parseJson(const std::string& text, const JsonLimits& limits = JsonLimits());

}  // namespace henu
