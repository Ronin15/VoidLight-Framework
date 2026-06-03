/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
*/

#include "utils/JsonReader.hpp"
#include <cmath>
#include <format>
#include <fstream>
#include <sstream>

namespace VoidLight {

// JsonValue implementation
JsonType JsonValue::getType() const {
  if (std::holds_alternative<std::nullptr_t>(m_value))
    return JsonType::Null;
  if (std::holds_alternative<bool>(m_value))
    return JsonType::Boolean;
  if (std::holds_alternative<double>(m_value))
    return JsonType::Number;
  if (std::holds_alternative<std::string>(m_value))
    return JsonType::String;
  if (std::holds_alternative<JsonArray>(m_value))
    return JsonType::Array;
  if (std::holds_alternative<JsonObject>(m_value))
    return JsonType::Object;
  return JsonType::Null; // Should never reach here
}

std::optional<bool> JsonValue::tryAsBool() const {
  if (isBool())
    return asBool();
  return std::nullopt;
}

std::optional<double> JsonValue::tryAsNumber() const {
  if (isNumber())
    return asNumber();
  return std::nullopt;
}

std::optional<int> JsonValue::tryAsInt() const {
  if (isNumber())
    return asInt();
  return std::nullopt;
}

std::optional<std::string> JsonValue::tryAsString() const {
  if (isString())
    return asString();
  return std::nullopt;
}

const JsonArray *JsonValue::tryAsArray() const {
  if (isArray())
    return &asArray();
  return nullptr;
}

const JsonObject *JsonValue::tryAsObject() const {
  if (isObject())
    return &asObject();
  return nullptr;
}

bool JsonValue::hasKey(const std::string &key) const {
  if (!isObject())
    return false;
  const auto &obj = asObject();
  return obj.find(key) != obj.end();
}

const JsonValue &JsonValue::operator[](const std::string &key) const {
  static const JsonValue null_value;
  if (!isObject())
    return null_value;
  const auto &obj = asObject();
  auto it = obj.find(key);
  return (it != obj.end()) ? it->second : null_value;
}

JsonValue &JsonValue::operator[](const std::string &key) {
  if (!isObject()) {
    m_value = JsonObject{};
  }
  return asObject()[key];
}

const JsonValue &JsonValue::operator[](size_t index) const {
  static const JsonValue null_value;
  if (!isArray() || index >= asArray().size())
    return null_value;
  return asArray()[index];
}

JsonValue &JsonValue::operator[](size_t index) {
  if (!isArray()) {
    m_value = JsonArray{};
  }
  auto &arr = asArray();
  if (index >= arr.size()) {
    arr.resize(index + 1);
  }
  return arr[index];
}

size_t JsonValue::size() const {
  if (isArray())
    return asArray().size();
  if (isObject())
    return asObject().size();
  return 0;
}

std::string JsonValue::toString(bool pretty) const {
  std::ostringstream oss;
  writeToStream(oss, pretty ? 0 : -1);
  return oss.str();
}

namespace {
void writeIndent(std::ostream &stream, int depth) {
  for (int i = 0; i < depth; ++i) {
    stream << "  ";
  }
}

void writeEscapedString(std::ostream &stream, std::string_view s) {
  stream << '"';
  for (unsigned char c : s) {
    switch (c) {
    case '"':  stream << "\\\""; break;
    case '\\': stream << "\\\\"; break;
    case '\b': stream << "\\b";  break;
    case '\f': stream << "\\f";  break;
    case '\n': stream << "\\n";  break;
    case '\r': stream << "\\r";  break;
    case '\t': stream << "\\t";  break;
    default:
      if (c < 0x20) {
        stream << std::format("\\u{:04x}", static_cast<unsigned int>(c));
      } else {
        stream << static_cast<char>(c);
      }
      break;
    }
  }
  stream << '"';
}
} // namespace

void JsonValue::writeToStream(std::ostream &stream, int depth) const {
  const bool pretty = depth >= 0;
  switch (getType()) {
  case JsonType::Null:
    stream << "null";
    break;
  case JsonType::Boolean:
    stream << (asBool() ? "true" : "false");
    break;
  case JsonType::Number: {
    double num = asNumber();
    if (std::floor(num) == num && std::abs(num) < 1e15) {
      stream << static_cast<long long>(num);
    } else {
      stream << num;
    }
    break;
  }
  case JsonType::String:
    writeEscapedString(stream, asString());
    break;
  case JsonType::Array: {
    const auto &arr = asArray();
    if (arr.empty()) {
      stream << "[]";
      break;
    }
    stream << "[";
    for (size_t i = 0; i < arr.size(); ++i) {
      if (i > 0)
        stream << ",";
      if (pretty) {
        stream << "\n";
        writeIndent(stream, depth + 1);
      }
      arr[i].writeToStream(stream, pretty ? depth + 1 : -1);
    }
    if (pretty) {
      stream << "\n";
      writeIndent(stream, depth);
    }
    stream << "]";
    break;
  }
  case JsonType::Object: {
    const auto &obj = asObject();
    if (obj.empty()) {
      stream << "{}";
      break;
    }
    stream << "{";
    bool first = true;
    for (const auto &[key, value] : obj) {
      if (!first)
        stream << ",";
      first = false;
      if (pretty) {
        stream << "\n";
        writeIndent(stream, depth + 1);
      }
      writeEscapedString(stream, key);
      stream << ":";
      if (pretty)
        stream << " ";
      value.writeToStream(stream, pretty ? depth + 1 : -1);
    }
    if (pretty) {
      stream << "\n";
      writeIndent(stream, depth);
    }
    stream << "}";
    break;
  }
  }
}

// JsonReader implementation
JsonReader::JsonReader() : m_position(0), m_line(1), m_column(1) {}

bool JsonReader::loadFromFile(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    setError("Could not open file: " + path);
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  return parse(buffer.str());
}

bool JsonReader::parse(const std::string &jsonString) {
  clearError();
  m_input = jsonString;
  m_position = 0;
  m_line = 1;
  m_column = 1;

  try {
    auto tokens = tokenize();
    if (!m_lastError.empty()) {
      return false;
    }

    Parser parser(tokens, m_lastError);
    m_root = parser.parse();
    return m_lastError.empty();
  } catch (const std::exception &e) {
    setError("Parse error: " + std::string(e.what()));
    return false;
  }
}

void JsonReader::setError(const std::string &message) {
  m_lastError = std::format("Line {}, Column {}: {}", m_line, m_column, message);
}

// Tokenizer implementation
std::vector<JsonToken> JsonReader::tokenize() {
  std::vector<JsonToken> tokens;
  tokens.reserve(256); // PERFORMANCE: Pre-allocate reasonable size

  while (m_position < m_input.length()) {
    skipWhitespace();
    if (m_position >= m_input.length())
      break;

    char c = peek();
    size_t tokenLine = m_line;
    size_t tokenColumn = m_column;

    // PERFORMANCE OPTIMIZATION: Use static strings to avoid allocations
    static const std::string LEFT_BRACE("{");
    static const std::string RIGHT_BRACE("}");
    static const std::string LEFT_BRACKET("[");
    static const std::string RIGHT_BRACKET("]");
    static const std::string COMMA(",");
    static const std::string COLON(":");
    static const std::string TRUE_STR("true");
    static const std::string FALSE_STR("false");
    static const std::string NULL_STR("null");

    switch (c) {
    case '{':
      tokens.emplace_back(JsonTokenType::LeftBrace, LEFT_BRACE, tokenLine,
                          tokenColumn);
      advance();
      break;
    case '}':
      tokens.emplace_back(JsonTokenType::RightBrace, RIGHT_BRACE, tokenLine,
                          tokenColumn);
      advance();
      break;
    case '[':
      tokens.emplace_back(JsonTokenType::LeftBracket, LEFT_BRACKET, tokenLine,
                          tokenColumn);
      advance();
      break;
    case ']':
      tokens.emplace_back(JsonTokenType::RightBracket, RIGHT_BRACKET, tokenLine,
                          tokenColumn);
      advance();
      break;
    case ',':
      tokens.emplace_back(JsonTokenType::Comma, COMMA, tokenLine, tokenColumn);
      advance();
      break;
    case ':':
      tokens.emplace_back(JsonTokenType::Colon, COLON, tokenLine, tokenColumn);
      advance();
      break;
    case '"': {
      std::string str = parseString();
      if (!m_lastError.empty())
        return tokens;
      tokens.emplace_back(JsonTokenType::String, std::move(str), tokenLine,
                          tokenColumn);
      break;
    }
    case 't':
      // PERFORMANCE: Check characters directly instead of substr
      if (m_position + 4 <= m_input.length() &&
          m_input[m_position + 1] == 'r' && m_input[m_position + 2] == 'u' &&
          m_input[m_position + 3] == 'e') {
        tokens.emplace_back(JsonTokenType::True, TRUE_STR, tokenLine,
                            tokenColumn);
        m_position += 4;
        m_column += 4;
      } else {
        setError("Invalid token starting with 't'");
        return tokens;
      }
      break;
    case 'f':
      // PERFORMANCE: Check characters directly instead of substr
      if (m_position + 5 <= m_input.length() &&
          m_input[m_position + 1] == 'a' && m_input[m_position + 2] == 'l' &&
          m_input[m_position + 3] == 's' && m_input[m_position + 4] == 'e') {
        tokens.emplace_back(JsonTokenType::False, FALSE_STR, tokenLine,
                            tokenColumn);
        m_position += 5;
        m_column += 5;
      } else {
        setError("Invalid token starting with 'f'");
        return tokens;
      }
      break;
    case 'n':
      // PERFORMANCE: Check characters directly instead of substr
      if (m_position + 4 <= m_input.length() &&
          m_input[m_position + 1] == 'u' && m_input[m_position + 2] == 'l' &&
          m_input[m_position + 3] == 'l') {
        tokens.emplace_back(JsonTokenType::Null, NULL_STR, tokenLine,
                            tokenColumn);
        m_position += 4;
        m_column += 4;
      } else {
        setError("Invalid token starting with 'n'");
        return tokens;
      }
      break;
    default:
      if (isDigit(c) || c == '-') {
        std::string num = parseNumber();
        if (!m_lastError.empty())
          return tokens;
        tokens.emplace_back(JsonTokenType::Number, std::move(num), tokenLine,
                            tokenColumn);
      } else {
        setError("Unexpected character: " + std::string(1, c));
        return tokens;
      }
      break;
    }
  }

  tokens.emplace_back(JsonTokenType::EndOfFile, "", m_line, m_column);
  return tokens;
}

char JsonReader::peek(size_t offset) const {
  size_t pos = m_position + offset;
  return (pos < m_input.length()) ? m_input[pos] : '\0';
}

char JsonReader::advance() {
  if (m_position >= m_input.length())
    return '\0';

  char c = m_input[m_position++];
  if (c == '\n') {
    m_line++;
    m_column = 1;
  } else {
    m_column++;
  }
  return c;
}

void JsonReader::skipWhitespace() {
  while (m_position < m_input.length()) {
    char c = peek();
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      advance();
    } else {
      break;
    }
  }
}

std::string JsonReader::parseString() {
  if (peek() != '"') {
    setError("Expected '\"' at start of string");
    return "";
  }
  advance(); // Skip opening quote

  std::string result;
  while (m_position < m_input.length()) {
    char c = peek();

    if (c == '"') {
      advance(); // Skip closing quote
      return result;
    }

    if (c == '\\') {
      advance(); // Skip backslash
      if (m_position >= m_input.length()) {
        setError("Unexpected end of input in string escape");
        return "";
      }

      char escaped = peek();
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        result += escaped;
        advance();
        break;
      case 'b':
        result += '\b';
        advance();
        break;
      case 'f':
        result += '\f';
        advance();
        break;
      case 'n':
        result += '\n';
        advance();
        break;
      case 'r':
        result += '\r';
        advance();
        break;
      case 't':
        result += '\t';
        advance();
        break;
      case 'u': {
        advance(); // Skip 'u'
        uint32_t codepoint = parseUnicodeEscape();
        if (!m_lastError.empty())
          return "";

        // Convert Unicode codepoint to UTF-8
        if (codepoint <= 0x7F) {
          result += static_cast<char>(codepoint);
        } else if (codepoint <= 0x7FF) {
          result += static_cast<char>(0xC0 | (codepoint >> 6));
          result += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint <= 0xFFFF) {
          result += static_cast<char>(0xE0 | (codepoint >> 12));
          result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
          result += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
          result += static_cast<char>(0xF0 | (codepoint >> 18));
          result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
          result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
          result += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        break;
      }
      default:
        setError("Invalid escape sequence: \\" + std::string(1, escaped));
        return "";
      }
    } else if (c < 0x20) {
      setError("Unescaped control character in string");
      return "";
    } else {
      result += c;
      advance();
    }
  }

  setError("Unterminated string");
  return "";
}

std::string JsonReader::parseNumber() {
  std::string result;

  // Optional minus
  if (peek() == '-') {
    result += advance();
  }

  // Integer part
  if (peek() == '0') {
    result += advance();
  } else if (isDigit(peek())) {
    while (isDigit(peek())) {
      result += advance();
    }
  } else {
    setError("Invalid number format");
    return "";
  }

  // Optional fractional part
  if (peek() == '.') {
    result += advance();
    if (!isDigit(peek())) {
      setError("Invalid number format: expected digit after decimal point");
      return "";
    }
    while (isDigit(peek())) {
      result += advance();
    }
  }

  // Optional exponent part
  if (peek() == 'e' || peek() == 'E') {
    result += advance();
    if (peek() == '+' || peek() == '-') {
      result += advance();
    }
    if (!isDigit(peek())) {
      setError("Invalid number format: expected digit in exponent");
      return "";
    }
    while (isDigit(peek())) {
      result += advance();
    }
  }

  return result;
}

bool JsonReader::isDigit(char c) const { return c >= '0' && c <= '9'; }

bool JsonReader::isHexDigit(char c) const {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
         (c >= 'A' && c <= 'F');
}

uint32_t JsonReader::parseUnicodeEscape() {
  uint32_t result = 0;
  for (int i = 0; i < 4; ++i) {
    if (m_position >= m_input.length() || !isHexDigit(peek())) {
      setError("Invalid Unicode escape sequence");
      return 0;
    }

    char c = advance();
    uint32_t digit;
    if (c >= '0' && c <= '9') {
      digit = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      digit = c - 'a' + 10;
    } else {
      digit = c - 'A' + 10;
    }

    result = (result << 4) | digit;
  }
  return result;
}

// Parser implementation
JsonValue JsonReader::Parser::parse() {
  if (m_tokens.empty()) {
    setError("Empty JSON input");
    return JsonValue();
  }

  JsonValue result = parseValue();
  if (!m_error.empty()) {
    return JsonValue();
  }

  if (!isAtEnd()) {
    setError("Unexpected token after JSON value");
    return JsonValue();
  }

  return result;
}

JsonValue JsonReader::Parser::parseValue() {
  const JsonToken &token = peek();

  switch (token.type) {
  case JsonTokenType::Null:
    advance();
    return JsonValue();
  case JsonTokenType::True:
    advance();
    return JsonValue(true);
  case JsonTokenType::False:
    advance();
    return JsonValue(false);
  case JsonTokenType::Number: {
    std::string numStr = advance().value;
    try {
      double num = std::stod(numStr);
      return JsonValue(num);
    } catch (const std::exception &) {
      setError("Invalid number format: " + numStr);
      return JsonValue();
    }
  }
  case JsonTokenType::String:
    return JsonValue(advance().value);
  case JsonTokenType::LeftBrace:
    return JsonValue(parseObject());
  case JsonTokenType::LeftBracket:
    return JsonValue(parseArray());
  default:
    setError("Expected value, got " + token.value);
    return JsonValue();
  }
}

JsonObject JsonReader::Parser::parseObject() {
  JsonObject result;

  if (!match(JsonTokenType::LeftBrace)) {
    setError("Expected '{'");
    return result;
  }

  if (check(JsonTokenType::RightBrace)) {
    advance(); // Consume '}'
    return result;
  }

  do {
    if (!check(JsonTokenType::String)) {
      setError("Expected string key in object");
      return result;
    }

    std::string key = advance().value;

    if (!match(JsonTokenType::Colon)) {
      setError("Expected ':' after object key");
      return result;
    }

    JsonValue value = parseValue();
    if (!m_error.empty()) {
      return result;
    }

    result[key] = std::move(value);

  } while (match(JsonTokenType::Comma));

  if (!match(JsonTokenType::RightBrace)) {
    setError("Expected '}' or ',' in object");
    return result;
  }

  return result;
}

JsonArray JsonReader::Parser::parseArray() {
  JsonArray result;

  if (!match(JsonTokenType::LeftBracket)) {
    setError("Expected '['");
    return result;
  }

  if (check(JsonTokenType::RightBracket)) {
    advance(); // Consume ']'
    return result;
  }

  do {
    JsonValue value = parseValue();
    if (!m_error.empty()) {
      return result;
    }

    result.push_back(std::move(value));

  } while (match(JsonTokenType::Comma));

  if (!match(JsonTokenType::RightBracket)) {
    setError("Expected ']' or ',' in array");
    return result;
  }

  return result;
}

bool JsonReader::Parser::match(JsonTokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

bool JsonReader::Parser::check(JsonTokenType type) const {
  if (isAtEnd())
    return false;
  return peek().type == type;
}

const JsonToken &JsonReader::Parser::advance() {
  if (!isAtEnd())
    m_current++;
  return previous();
}

const JsonToken &JsonReader::Parser::peek() const {
  return m_tokens[m_current];
}

const JsonToken &JsonReader::Parser::previous() const {
  return m_tokens[m_current - 1];
}

bool JsonReader::Parser::isAtEnd() const {
  return peek().type == JsonTokenType::EndOfFile;
}

void JsonReader::Parser::setError(const std::string &message) {
  const JsonToken &token = peek();
  m_error = std::format("Line {}, Column {}: {}", token.line, token.column, message);
}

} // namespace VoidLight
