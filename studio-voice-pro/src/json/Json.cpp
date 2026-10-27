#include "svp/json/Json.h"
#include <charconv>
#include <cmath>
#include <cstdio>
#include <system_error>

namespace svp::json {
namespace {

constexpr int kMaxDepth = 128;

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    bool run(Value& out, std::string& error)
    {
        skipWhitespace();
        if (!parseValue(out, 0)) {
            error = withPosition(error_);
            return false;
        }
        skipWhitespace();
        if (position_ != text_.size()) {
            error = withPosition("données inattendues après la fin de la valeur racine");
            return false;
        }
        return true;
    }

private:
    std::string withPosition(const std::string& message) const
    {
        std::size_t line = 1;
        std::size_t column = 1;
        for (std::size_t i = 0; i < position_ && i < text_.size(); ++i) {
            if (text_[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "ligne %zu, colonne %zu : ", line, column);
        return std::string(buffer) + message;
    }

    void fail(const std::string& message) { error_ = message; }

    void skipWhitespace()
    {
        while (position_ < text_.size()) {
            const char c = text_[position_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
                ++position_;
            else
                break;
        }
    }

    bool peek(char& c) const
    {
        if (position_ >= text_.size())
            return false;
        c = text_[position_];
        return true;
    }

    bool literal(std::string_view word)
    {
        if (text_.compare(position_, word.size(), word) != 0)
            return false;
        position_ += word.size();
        return true;
    }

    bool parseValue(Value& out, int depth)
    {
        if (depth > kMaxDepth) {
            fail("imbrication JSON trop profonde");
            return false;
        }
        char c = 0;
        if (!peek(c)) {
            fail("fin de fichier inattendue (valeur attendue)");
            return false;
        }
        switch (c) {
        case '{': return parseObject(out, depth);
        case '[': return parseArray(out, depth);
        case '"': {
            out = Value{};
            out.type = Type::String;
            return parseString(out.text);
        }
        case 't':
            if (literal("true")) {
                out = Value{};
                out.type = Type::Bool;
                out.boolean = true;
                return true;
            }
            fail("littéral invalide (attendu : true, false ou null)");
            return false;
        case 'f':
            if (literal("false")) {
                out = Value{};
                out.type = Type::Bool;
                return true;
            }
            fail("littéral invalide (attendu : true, false ou null)");
            return false;
        case 'n':
            if (literal("null")) {
                out = Value{};
                out.type = Type::Null;
                return true;
            }
            fail("littéral invalide (attendu : true, false ou null)");
            return false;
        default:
            if (c == '-' || (c >= '0' && c <= '9'))
                return parseNumber(out);
            fail("caractère inattendu là où une valeur est attendue");
            return false;
        }
    }

    bool parseObject(Value& out, int depth)
    {
        ++position_;  // '{'
        out = Value{};
        out.type = Type::Object;
        skipWhitespace();
        char c = 0;
        if (peek(c) && c == '}') {
            ++position_;
            return true;
        }
        while (true) {
            skipWhitespace();
            if (!peek(c) || c != '"') {
                fail("nom de champ attendu (chaîne entre guillemets)");
                return false;
            }
            std::string name;
            if (!parseString(name))
                return false;
            skipWhitespace();
            if (!peek(c) || c != ':') {
                fail("« : » attendu après le nom de champ « " + name + " »");
                return false;
            }
            ++position_;
            skipWhitespace();
            Value member;
            if (!parseValue(member, depth + 1))
                return false;
            out.members.emplace_back(std::move(name), std::move(member));
            skipWhitespace();
            if (!peek(c))
                break;
            if (c == ',') {
                ++position_;
                continue;
            }
            if (c == '}') {
                ++position_;
                return true;
            }
            fail("« , » ou « } » attendu dans l'objet");
            return false;
        }
        fail("objet non terminé (} manquant)");
        return false;
    }

    bool parseArray(Value& out, int depth)
    {
        ++position_;  // '['
        out = Value{};
        out.type = Type::Array;
        skipWhitespace();
        char c = 0;
        if (peek(c) && c == ']') {
            ++position_;
            return true;
        }
        while (true) {
            skipWhitespace();
            Value item;
            if (!parseValue(item, depth + 1))
                return false;
            out.items.push_back(std::move(item));
            skipWhitespace();
            if (!peek(c))
                break;
            if (c == ',') {
                ++position_;
                continue;
            }
            if (c == ']') {
                ++position_;
                return true;
            }
            fail("« , » ou « ] » attendu dans le tableau");
            return false;
        }
        fail("tableau non terminé (] manquant)");
        return false;
    }

    void appendUtf8(std::string& out, uint32_t code)
    {
        if (code <= 0x7Fu) {
            out.push_back(static_cast<char>(code));
        } else if (code <= 0x7FFu) {
            out.push_back(static_cast<char>(0xC0u | (code >> 6)));
            out.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
        } else if (code <= 0xFFFFu) {
            out.push_back(static_cast<char>(0xE0u | (code >> 12)));
            out.push_back(static_cast<char>(0x80u | ((code >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
        } else {
            out.push_back(static_cast<char>(0xF0u | (code >> 18)));
            out.push_back(static_cast<char>(0x80u | ((code >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((code >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
        }
    }

    bool parseHex4(uint32_t& value)
    {
        if (position_ + 4 > text_.size()) {
            fail("séquence \\u incomplète");
            return false;
        }
        value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[position_ + static_cast<std::size_t>(i)];
            uint32_t digit = 0;
            if (c >= '0' && c <= '9')
                digit = static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f')
                digit = static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                digit = static_cast<uint32_t>(c - 'A' + 10);
            else {
                fail("chiffre hexadécimal attendu après \\u");
                return false;
            }
            value = value * 16u + digit;
        }
        position_ += 4;
        return true;
    }

    bool parseString(std::string& out)
    {
        ++position_;  // guillemet ouvrant
        out.clear();
        while (position_ < text_.size()) {
            const char c = text_[position_++];
            if (c == '"')
                return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (position_ >= text_.size()) {
                fail("échappement incomplet en fin de fichier");
                return false;
            }
            const char escapeChar = text_[position_++];
            switch (escapeChar) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                uint32_t code = 0;
                if (!parseHex4(code))
                    return false;
                if (code >= 0xD800u && code <= 0xDBFFu) {  // paire de substitution
                    if (position_ + 1 < text_.size() && text_[position_] == '\\'
                        && text_[position_ + 1] == 'u') {
                        position_ += 2;
                        uint32_t low = 0;
                        if (!parseHex4(low))
                            return false;
                        if (low >= 0xDC00u && low <= 0xDFFFu)
                            code = 0x10000u + ((code - 0xD800u) << 10) + (low - 0xDC00u);
                    }
                }
                appendUtf8(out, code);
                break;
            }
            default:
                fail("échappement JSON inconnu : \\" + std::string(1, escapeChar));
                return false;
            }
        }
        fail("chaîne non terminée (guillemet fermant manquant)");
        return false;
    }

    bool parseNumber(Value& out)
    {
        const std::size_t start = position_;
        bool integral = true;
        if (position_ < text_.size() && text_[position_] == '-')
            ++position_;
        const std::size_t digitsStart = position_;
        while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
            ++position_;
        if (position_ == digitsStart) {
            fail("nombre mal formé (aucun chiffre)");
            return false;
        }
        if (text_[digitsStart] == '0' && position_ - digitsStart > 1) {
            fail("nombre mal formé (zéro de tête interdit en JSON)");
            return false;
        }
        if (position_ < text_.size() && text_[position_] == '.') {
            integral = false;
            ++position_;
            const std::size_t fracStart = position_;
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                ++position_;
            if (position_ == fracStart) {
                fail("nombre mal formé (aucun chiffre après le point)");
                return false;
            }
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            integral = false;
            ++position_;
            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-'))
                ++position_;
            const std::size_t expStart = position_;
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9')
                ++position_;
            if (position_ == expStart) {
                fail("nombre mal formé (aucun chiffre dans l'exposant)");
                return false;
            }
        }
        const std::string_view token = text_.substr(start, position_ - start);
        out = Value{};
        out.type = Type::Number;
        if (integral) {
            int64_t value = 0;
            const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
            if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) {
                fail("entier hors de la plage int64 : " + std::string(token)
                     + " (refusé : un double perdrait des unités)");
                return false;
            }
            out.integer = value;
            // « -0 » est un zéro NÉGATIF au sens IEEE-754 : sans ce cas
            // particulier, from_chars<int64_t> le ramène à +0,0 et le signe
            // est perdu à la relecture (donc réécrit « 0 » — échec
            // d'idempotence byte pour byte). Voir docs/PROJECT-FORMAT.md §7.
            out.number = (token.size() > 1 && token.front() == '-' && value == 0)
                             ? -0.0
                             : static_cast<double>(value);
            out.integral = true;
            return true;
        }
        double value = 0.0;
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) {
            fail("nombre flottant illisible : " + std::string(token));
            return false;
        }
        out.number = value;
        return true;
    }

    std::string_view text_;
    std::size_t position_ = 0;
    std::string error_;
};

/** Écriture d'un double : représentation la plus courte qui se relit à
    l'identique (std::to_chars, sans locale). Repli sur %.17g si la
    bibliothèque standard ne fournit pas to_chars pour les flottants. */
void writeDouble(std::string& out, double value)
{
    if (!std::isfinite(value)) {
        out += "null";
        return;
    }
    // Zéro négatif : `std::to_chars` produit « -0 », que le lecteur JSON range
    // parmi les entiers et relit donc en +0,0. On écrit la seule forme qui se
    // relit bit à bit (« -0.0 »), pour que réenregistrer un projet donne
    // exactement le même fichier. Voir docs/PROJECT-FORMAT.md §7.
    if (value == 0.0 && std::signbit(value)) {
        out += "-0.0";
        return;
    }
    char buffer[64];
#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    if (result.ec == std::errc{}) {
        out.append(buffer, result.ptr);
        return;
    }
#endif
    std::snprintf(buffer, sizeof(buffer), "%.17g", value);
    out += buffer;
}

} // namespace

const Value* Value::find(std::string_view name) const
{
    if (type != Type::Object)
        return nullptr;
    for (const auto& member : members) {
        if (member.first == name)
            return &member.second;
    }
    return nullptr;
}

const Value* Value::item(std::size_t index) const
{
    if (type != Type::Array || index >= items.size())
        return nullptr;
    return &items[index];
}

std::size_t Value::size() const noexcept
{
    if (type == Type::Array)
        return items.size();
    if (type == Type::Object)
        return members.size();
    return 0;
}

bool Value::boolOr(bool fallback) const noexcept
{
    return (type == Type::Bool) ? boolean : fallback;
}

double Value::doubleOr(double fallback) const noexcept
{
    return (type == Type::Number) ? number : fallback;
}

int64_t Value::intOr(int64_t fallback) const noexcept
{
    if (type != Type::Number)
        return fallback;
    return integral ? integer : static_cast<int64_t>(number);
}

std::string Value::stringOr(std::string fallback) const
{
    return (type == Type::String) ? text : std::move(fallback);
}

Value Value::makeObject()
{
    Value value;
    value.type = Type::Object;
    return value;
}

Value Value::makeArray()
{
    Value value;
    value.type = Type::Array;
    return value;
}

Value Value::makeString(std::string text)
{
    Value value;
    value.type = Type::String;
    value.text = std::move(text);
    return value;
}

Value Value::makeInt(int64_t integer)
{
    Value value;
    value.type = Type::Number;
    value.integral = true;
    value.integer = integer;
    value.number = static_cast<double>(integer);
    return value;
}

Value Value::makeDouble(double number)
{
    Value value;
    value.type = Type::Number;
    value.number = number;
    return value;
}

Value Value::makeBool(bool boolean)
{
    Value value;
    value.type = Type::Bool;
    value.boolean = boolean;
    return value;
}

void Value::set(std::string key, Value member)
{
    type = Type::Object;
    for (auto& existing : members) {
        if (existing.first == key) {
            existing.second = std::move(member);
            return;
        }
    }
    members.emplace_back(std::move(key), std::move(member));
}

ParseResult parse(std::string_view text)
{
    ParseResult result;
    Parser parser(text);
    if (!parser.run(result.value, result.error)) {
        result.ok = false;
        result.value = Value{};
        return result;
    }
    result.ok = true;
    return result;
}

std::string escape(std::string_view text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20u) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(c));
                out += buffer;
            } else {
                out.push_back(c);  // octets UTF-8 valides recopiés tels quels
            }
            break;
        }
    }
    return out;
}

void Writer::newlineIndent()
{
    out_ += '\n';
    out_.append(static_cast<std::size_t>(indentWidth_) * stack_.size(), ' ');
}

void Writer::startValue()
{
    if (stack_.empty())
        return;
    Frame& frame = stack_.back();
    if (frame.isObject) {
        if (frame.hasMembers)
            out_ += ',';
        newlineIndent();
        out_ += '"';
        out_ += escape(pendingKey_);
        out_ += "\": ";
    } else {
        if (frame.hasMembers)
            out_ += ',';
        newlineIndent();
    }
    frame.hasMembers = true;
    hasPendingKey_ = false;
}

void Writer::beginObject()
{
    startValue();
    out_ += '{';
    stack_.push_back(Frame{true, false});
}

void Writer::endObject()
{
    if (stack_.empty() || !stack_.back().isObject)
        return;  // usage incorrect du Writer : on ne produit pas de JSON invalide
    const bool empty = !stack_.back().hasMembers;
    stack_.pop_back();
    if (!empty)
        newlineIndent();
    out_ += '}';
}

void Writer::beginArray()
{
    startValue();
    out_ += '[';
    stack_.push_back(Frame{false, false});
}

void Writer::endArray()
{
    if (stack_.empty() || stack_.back().isObject)
        return;
    const bool empty = !stack_.back().hasMembers;
    stack_.pop_back();
    if (!empty)
        newlineIndent();
    out_ += ']';
}

void Writer::key(std::string_view name)
{
    pendingKey_.assign(name);
    hasPendingKey_ = true;
}

void Writer::valueString(std::string_view value)
{
    startValue();
    out_ += '"';
    out_ += escape(value);
    out_ += '"';
}

void Writer::valueInt(int64_t value)
{
    startValue();
    char buffer[32];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    out_.append(buffer, result.ptr);
}

void Writer::valueDouble(double value)
{
    startValue();
    if (!std::isfinite(value))
        nonFiniteSeen_ = true;
    writeDouble(out_, value);
}

void Writer::valueBool(bool value)
{
    startValue();
    out_ += value ? "true" : "false";
}

void Writer::valueNull()
{
    startValue();
    out_ += "null";
}

void Writer::write(const Value& value)
{
    switch (value.type) {
    case Type::Null: valueNull(); break;
    case Type::Bool: valueBool(value.boolean); break;
    case Type::Number:
        if (value.integral)
            valueInt(value.integer);
        else
            valueDouble(value.number);
        break;
    case Type::String: valueString(value.text); break;
    case Type::Array:
        beginArray();
        for (const Value& item : value.items)
            write(item);
        endArray();
        break;
    case Type::Object:
        beginObject();
        for (const auto& member : value.members) {
            key(member.first);
            write(member.second);
        }
        endObject();
        break;
    }
}

void Writer::value(const Value& value)
{
    write(value);
}

std::string Writer::str() const
{
    return out_ + "\n";
}

} // namespace svp::json
