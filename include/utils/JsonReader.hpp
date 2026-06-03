/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#ifndef JSONREADER_HPP
#define JSONREADER_HPP

#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <cstdint>

namespace VoidLight {

class JsonValue;

using JsonObject = std::unordered_map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

enum class JsonType { Null, Boolean, Number, String, Array, Object };

// Stream operator for JsonType (for Boost.Test)
inline std::ostream &operator<<(std::ostream &os, JsonType type) {
  switch (type) {
  case JsonType::Null:
    return os << "Null";
  case JsonType::Boolean:
    return os << "Boolean";
  case JsonType::Number:
    return os << "Number";
  case JsonType::String:
    return os << "String";
  case JsonType::Array:
    return os << "Array";
  case JsonType::Object:
    return os << "Object";
  }
  return os << "Unknown";
}

class JsonValue {
public:
  using ValueType = std::variant<std::nullptr_t, // null
                                 bool,           // boolean
                                 double,         // number
                                 std::string,    // string
                                 JsonArray,      // array
                                 JsonObject      // object
                                 >;

private:
  ValueType m_value;

public:
  // Constructors
  JsonValue() : m_value(nullptr) {}
  explicit JsonValue(std::nullptr_t) : m_value(nullptr) {}
  explicit JsonValue(bool value) : m_value(value) {}
  explicit JsonValue(int value) : m_value(static_cast<double>(value)) {}
  explicit JsonValue(double value) : m_value(value) {}
  explicit JsonValue(const std::string &value) : m_value(value) {}
  explicit JsonValue(std::string &&value) : m_value(std::move(value)) {}
  explicit JsonValue(const char *value) : m_value(std::string(value)) {}
  explicit JsonValue(const JsonArray &value) : m_value(value) {}
  explicit JsonValue(JsonArray &&value) : m_value(std::move(value)) {}
  explicit JsonValue(const JsonObject &value) : m_value(value) {}
  explicit JsonValue(JsonObject &&value) : m_value(std::move(value)) {}

  // Type checking
  JsonType getType() const;
  bool isNull() const {
    return std::holds_alternative<std::nullptr_t>(m_value);
  }
  bool isBool() const { return std::holds_alternative<bool>(m_value); }
  bool isNumber() const { return std::holds_alternative<double>(m_value); }
  bool isString() const { return std::holds_alternative<std::string>(m_value); }
  bool isArray() const { return std::holds_alternative<JsonArray>(m_value); }
  bool isObject() const { return std::holds_alternative<JsonObject>(m_value); }

  // Value accessors (throw std::bad_variant_access if wrong type)
  bool asBool() const { return std::get<bool>(m_value); }
  double asNumber() const { return std::get<double>(m_value); }
  int asInt() const { return static_cast<int>(std::get<double>(m_value)); }
  const std::string &asString() const { return std::get<std::string>(m_value); }
  const JsonArray &asArray() const { return std::get<JsonArray>(m_value); }
  const JsonObject &asObject() const { return std::get<JsonObject>(m_value); }

  // Mutable accessors
  JsonArray &asArray() { return std::get<JsonArray>(m_value); }
  JsonObject &asObject() { return std::get<JsonObject>(m_value); }

  // Safe accessors (return optional)
  std::optional<bool> tryAsBool() const;
  std::optional<double> tryAsNumber() const;
  std::optional<int> tryAsInt() const;
  std::optional<std::string> tryAsString() const;
  const JsonArray *tryAsArray() const;
  const JsonObject *tryAsObject() const;

  // Object member access
  bool hasKey(const std::string &key) const;
  const JsonValue &operator[](const std::string &key) const;
  JsonValue &operator[](const std::string &key);

  // Array element access
  const JsonValue &operator[](size_t index) const;
  JsonValue &operator[](size_t index);
  size_t size() const;

  // Utility. When pretty==true, objects/arrays are broken across lines with
  // two-space indentation and a space after ':' — suitable for human-edited
  // config files. Default (compact) output is unchanged.
  std::string toString(bool pretty = false) const;

private:
  // depth >= 0 enables pretty printing; depth < 0 emits compact JSON.
  void writeToStream(std::ostream &stream, int depth = -1) const;
};

enum class JsonTokenType {
  EndOfFile,
  LeftBrace,    // {
  RightBrace,   // }
  LeftBracket,  // [
  RightBracket, // ]
  Comma,        // ,
  Colon,        // :
  String,       // "..."
  Number,       // 123, 12.34, -5, 1e10
  True,         // true
  False,        // false
  Null          // null
};

struct JsonToken {
  JsonTokenType type;
  std::string value;
  size_t line;
  size_t column;

  explicit JsonToken(JsonTokenType t, const std::string &v = "", size_t l = 1,
                     size_t c = 1)
      : type(t), value(v), line(l), column(c) {}
};

class JsonReader {
private:
  std::string m_input;
  size_t m_position;
  size_t m_line;
  size_t m_column;
  std::string m_lastError;
  JsonValue m_root;

  // Tokenizer
  std::vector<JsonToken> tokenize();
  char peek(size_t offset = 0) const;
  char advance();
  void skipWhitespace();
  std::string parseString();
  std::string parseNumber();
  bool isDigit(char c) const;
  bool isHexDigit(char c) const;
  uint32_t parseUnicodeEscape();

  // Parser
  class Parser {
  private:
    const std::vector<JsonToken> &m_tokens;
    size_t m_current;
    std::string &m_error;

  public:
    Parser(const std::vector<JsonToken> &tokens, std::string &error)
        : m_tokens(tokens), m_current(0), m_error(error) {}

    JsonValue parse();

  private:
    JsonValue parseValue();
    JsonObject parseObject();
    JsonArray parseArray();
    bool match(JsonTokenType type);
    bool check(JsonTokenType type) const;
    const JsonToken &advance();
    const JsonToken &peek() const;
    const JsonToken &previous() const;
    bool isAtEnd() const;
    void setError(const std::string &message);
  };

public:
  JsonReader();

  bool loadFromFile(const std::string &path);
  bool parse(const std::string &jsonString);
  const JsonValue &getRoot() const { return m_root; }
  const std::string &getLastError() const { return m_lastError; }
  void clearError() { m_lastError.clear(); }

private:
  void setError(const std::string &message);
};

} // namespace VoidLight

#endif // JSONREADER_HPP
