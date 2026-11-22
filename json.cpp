#include "json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>

Json::Json() = default;
Json::Json(std::nullptr_t) : type_(Type::Null) {}
Json::Json(bool value) : type_(Type::Bool), bool_(value) {}
Json::Json(int value) : type_(Type::Number), number_(value) {}
Json::Json(long long value) : type_(Type::Number), number_(static_cast<double>(value)) {}
Json::Json(double value) : type_(Type::Number), number_(value) {}
Json::Json(const char* value) : type_(Type::String), string_(value ? value : "") {}
Json::Json(const std::string& value) : type_(Type::String), string_(value) {}

Json Json::array() {
    Json result;
    result.type_ = Type::Array;
    return result;
}

Json Json::object() {
    Json result;
    result.type_ = Type::Object;
    return result;
}

bool Json::asBool(bool fallback) const {
    return type_ == Type::Bool ? bool_ : fallback;
}

double Json::asNumber(double fallback) const {
    return type_ == Type::Number ? number_ : fallback;
}

long long Json::asInt(long long fallback) const {
    return type_ == Type::Number ? static_cast<long long>(number_) : fallback;
}

std::string Json::asString() const {
    return type_ == Type::String ? string_ : std::string();
}

std::string Json::asString(const std::string& fallback) const {
    return type_ == Type::String ? string_ : fallback;
}

Json& Json::operator[](const std::string& key) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        object_.clear();
        array_.clear();
    }
    return object_[key];
}

const Json* Json::find(const std::string& key) const {
    if (type_ != Type::Object) {
        return nullptr;
    }
    auto it = object_.find(key);
    return it == object_.end() ? nullptr : &it->second;
}

bool Json::has(const std::string& key) const {
    return find(key) != nullptr;
}

Json& Json::push(const Json& value) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        array_.clear();
        object_.clear();
    }
    array_.push_back(value);
    return *this;
}

std::size_t Json::size() const {
    if (type_ == Type::Array) {
        return array_.size();
    }
    if (type_ == Type::Object) {
        return object_.size();
    }
    return 0;
}

namespace {

void appendUtf8(std::string& out, unsigned int codePoint) {
    if (codePoint <= 0x7F) {
        out += static_cast<char>(codePoint);
    } else if (codePoint <= 0x7FF) {
        out += static_cast<char>(0xC0 | (codePoint >> 6));
        out += static_cast<char>(0x80 | (codePoint & 0x3F));
    } else if (codePoint <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (codePoint >> 12));
        out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codePoint & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (codePoint >> 18));
        out += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    Json parseDocument() {
        Json value = parseValue();
        skipWhitespace();
        if (index_ != text_.size()) {
            fail("unexpected trailing characters");
        }
        return value;
    }

private:
    const std::string& text_;
    std::size_t index_ = 0;
    int depth_ = 0;

    [[noreturn]] void fail(const std::string& message) const {
        throw JsonParseError(message + " at position " + std::to_string(index_));
    }

    void skipWhitespace() {
        while (index_ < text_.size()) {
            char c = text_[index_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++index_;
            } else {
                break;
            }
        }
    }

    char peek() const {
        return index_ < text_.size() ? text_[index_] : '\0';
    }

    void expect(char expected) {
        if (peek() != expected) {
            fail(std::string("expected '") + expected + "'");
        }
        ++index_;
    }

    void expectWord(const char* word) {
        std::size_t length = std::char_traits<char>::length(word);
        if (text_.compare(index_, length, word) != 0) {
            fail("invalid literal");
        }
        index_ += length;
    }

    Json parseValue() {
        skipWhitespace();
        if (index_ >= text_.size()) {
            fail("unexpected end of input");
        }
        if (++depth_ > 64) {
            fail("nesting too deep");
        }
        char c = text_[index_];
        Json value;
        if (c == '{') {
            value = parseObject();
        } else if (c == '[') {
            value = parseArray();
        } else if (c == '"') {
            value = Json(parseString());
        } else if (c == 't') {
            expectWord("true");
            value = Json(true);
        } else if (c == 'f') {
            expectWord("false");
            value = Json(false);
        } else if (c == 'n') {
            expectWord("null");
            value = Json();
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            value = Json(parseNumber());
        } else {
            fail("unexpected character");
        }
        --depth_;
        return value;
    }

    Json parseObject() {
        expect('{');
        Json result = Json::object();
        skipWhitespace();
        if (peek() == '}') {
            ++index_;
            return result;
        }
        while (true) {
            skipWhitespace();
            if (peek() != '"') {
                fail("expected object key");
            }
            std::string key = parseString();
            skipWhitespace();
            expect(':');
            result[key] = parseValue();
            skipWhitespace();
            char c = peek();
            if (c == ',') {
                ++index_;
                continue;
            }
            if (c == '}') {
                ++index_;
                return result;
            }
            fail("expected ',' or '}'");
        }
    }

    Json parseArray() {
        expect('[');
        Json result = Json::array();
        skipWhitespace();
        if (peek() == ']') {
            ++index_;
            return result;
        }
        while (true) {
            result.push(parseValue());
            skipWhitespace();
            char c = peek();
            if (c == ',') {
                ++index_;
                continue;
            }
            if (c == ']') {
                ++index_;
                return result;
            }
            fail("expected ',' or ']'");
        }
    }

    unsigned int parseHex4() {
        if (index_ + 4 > text_.size()) {
            fail("incomplete unicode escape");
        }
        unsigned int value = 0;
        for (int i = 0; i < 4; ++i) {
            int digit = hexDigit(text_[index_++]);
            if (digit < 0) {
                fail("invalid unicode escape");
            }
            value = (value << 4) | static_cast<unsigned int>(digit);
        }
        return value;
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (true) {
            if (index_ >= text_.size()) {
                fail("unterminated string");
            }
            char c = text_[index_++];
            if (c == '"') {
                break;
            }
            if (c != '\\') {
                if (static_cast<unsigned char>(c) < 0x20) {
                    fail("control character in string");
                }
                out += c;
                continue;
            }
            if (index_ >= text_.size()) {
                fail("unterminated escape sequence");
            }
            char escape = text_[index_++];
            switch (escape) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned int codePoint = parseHex4();
                    if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                        if (index_ + 1 < text_.size() &&
                            text_[index_] == '\\' && text_[index_ + 1] == 'u') {
                            index_ += 2;
                            unsigned int low = parseHex4();
                            if (low < 0xDC00 || low > 0xDFFF) {
                                fail("invalid low surrogate");
                            }
                            codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                        } else {
                            fail("missing low surrogate");
                        }
                    }
                    appendUtf8(out, codePoint);
                    break;
                }
                default:
                    fail("invalid escape sequence");
            }
        }
        return out;
    }

    double parseNumber() {
        std::size_t start = index_;
        if (peek() == '-') {
            ++index_;
        }
        while (index_ < text_.size()) {
            char c = text_[index_];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                ++index_;
            } else {
                break;
            }
        }
        std::string token = text_.substr(start, index_ - start);
        char* end = nullptr;
        double value = std::strtod(token.c_str(), &end);
        if (!end || *end != '\0') {
            fail("invalid number");
        }
        return value;
    }
};

void dumpNumber(double value, std::string& out) {
    if (!std::isfinite(value)) {
        out += "0";
        return;
    }
    if (value == std::floor(value) && std::fabs(value) < 9.0e15) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
        out += buffer;
        return;
    }
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    out += stream.str();
}

void dumpString(const std::string& value, std::string& out) {
    out += '"';
    for (unsigned char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", c);
                    out += buffer;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

void dumpValue(const Json& value, std::string& out) {
    switch (value.type()) {
        case Json::Type::Null:
            out += "null";
            break;
        case Json::Type::Bool:
            out += value.asBool() ? "true" : "false";
            break;
        case Json::Type::Number:
            dumpNumber(value.asNumber(), out);
            break;
        case Json::Type::String:
            dumpString(value.asString(), out);
            break;
        case Json::Type::Array: {
            out += '[';
            bool first = true;
            for (const Json& item : value.items()) {
                if (!first) {
                    out += ',';
                }
                first = false;
                dumpValue(item, out);
            }
            out += ']';
            break;
        }
        case Json::Type::Object: {
            out += '{';
            bool first = true;
            for (const auto& entry : value.fields()) {
                if (!first) {
                    out += ',';
                }
                first = false;
                dumpString(entry.first, out);
                out += ':';
                dumpValue(entry.second, out);
            }
            out += '}';
            break;
        }
    }
}

}  // namespace

std::string Json::dump() const {
    std::string out;
    dumpValue(*this, out);
    return out;
}

Json Json::parse(const std::string& text) {
    Parser parser(text);
    return parser.parseDocument();
}
