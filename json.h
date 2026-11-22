#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json();
    Json(std::nullptr_t);
    Json(bool value);
    Json(int value);
    Json(long long value);
    Json(double value);
    Json(const char* value);
    Json(const std::string& value);

    static Json array();
    static Json object();

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool(bool fallback = false) const;
    double asNumber(double fallback = 0.0) const;
    long long asInt(long long fallback = 0) const;
    std::string asString() const;
    std::string asString(const std::string& fallback) const;

    const std::vector<Json>& items() const { return array_; }
    std::vector<Json>& items() { return array_; }
    const std::map<std::string, Json>& fields() const { return object_; }

    Json& operator[](const std::string& key);
    const Json* find(const std::string& key) const;
    bool has(const std::string& key) const;
    Json& push(const Json& value);
    std::size_t size() const;

    std::string dump() const;
    static Json parse(const std::string& text);

private:
    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Json> array_;
    std::map<std::string, Json> object_;
};

class JsonParseError : public std::runtime_error {
public:
    explicit JsonParseError(const std::string& message) : std::runtime_error(message) {}
};
