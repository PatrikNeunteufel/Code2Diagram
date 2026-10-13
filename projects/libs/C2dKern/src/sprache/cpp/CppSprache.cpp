/**
 * @file CppSprache.cpp
 * @brief C++: die Sprachtabelle; gelesen wird mit dem Leser der C-Familie
 *
 * Die Schluesselwoerter schalten im Leser frei, was C++ ueber C hinaus hat
 * (cfamilie/CFamilie.hpp). `final` und `override` fehlen mit Absicht: sie sind
 * nur an ihrer Stelle besonders und sonst gewoehnliche Namen.
 */
#include "../cfamilie/CFamilie.hpp"

namespace c2d::sprache
{

namespace
{

// C++20. Die Liste geht auf die der Vorgaenger zurueck (docs/Ernte_Vorgaenger.md),
// bereinigt: `NULL` und `defined` sind keine Schluesselwoerter.
constexpr std::string_view kSchluesselwoerter[] = {
    "alignas",     "alignof",      "and",       "and_eq",        "asm",          "auto",
    "bitand",      "bitor",        "bool",      "break",         "case",         "catch",
    "char",        "char8_t",      "char16_t",  "char32_t",      "class",        "compl",
    "concept",     "const",        "consteval", "constexpr",     "constinit",    "const_cast",
    "continue",    "co_await",     "co_return", "co_yield",      "decltype",     "default",
    "delete",      "do",           "double",    "dynamic_cast",  "else",         "enum",
    "explicit",    "export",       "extern",    "false",         "float",        "for",
    "friend",      "goto",         "if",        "inline",        "int",          "long",
    "mutable",     "namespace",    "new",       "noexcept",      "not",          "not_eq",
    "nullptr",     "operator",     "or",        "or_eq",         "private",      "protected",
    "public",      "register",     "reinterpret_cast",           "requires",     "return",
    "short",       "signed",       "sizeof",    "static",        "static_assert", "static_cast",
    "struct",      "switch",       "template",  "this",          "thread_local", "throw",
    "true",        "try",          "typedef",   "typeid",        "typename",     "union",
    "unsigned",    "using",        "virtual",   "void",          "volatile",     "wchar_t",
    "while",       "xor",          "xor_eq",
};

constexpr std::string_view kOperatoren[] = {
    "...", "<=>", "->*", "<<=", ">>=", "::", ".*", "->", "++", "--", "<<", ">>", "<=", ">=",
    "==",  "!=",  "&&",  "||",  "+=",  "-=", "*=", "/=", "%=", "&=", "^=", "|=", "##", "{",
    "}",   "[",   "]",   "(",   ")",   ";",  ",",  ".",  ":",  "?",  "~",  "!",  "+",  "-",
    "*",   "/",   "%",   "&",   "|",   "^",  "<",  ">",  "=",  "#",
};

// `h` teilt sich C++ mit C: wer sie bekommt, entscheidet erkennt().
constexpr std::string_view kEndungen[] = {"cpp", "cc", "cxx", "hpp", "hh", "hxx", "ipp", "inl", "tpp", "h"};

constexpr lexer::Sprachtabelle kTabelle{
    .schluesselwoerter = kSchluesselwoerter,
    .operatoren = kOperatoren,
    .zeilenKommentar = "//",
    .blockKommentarAuf = "/*",
    .blockKommentarZu = "*/",
    .direktive = '#',
    .rohZeichenketten = true,
    .ziffernTrenner = '\'',
};

/// Steht `wort` im Text als ganzes Wort?
bool hatWort(std::string_view text, std::string_view wort)
{
    const auto teil = [](char c) {
        return c == '_' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    };
    for (std::size_t i = text.find(wort); i != std::string_view::npos; i = text.find(wort, i + 1))
    {
        const bool davor = i == 0 || !teil(text[i - 1]);
        const bool danach = i + wort.size() >= text.size() || !teil(text[i + wort.size()]);
        if (davor && danach) return true;
    }
    return false;
}

class Cpp final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "C++"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungen; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return kTabelle; }

    /// Ein Kopf mit der Endung `h` ist C++, wenn er etwas nennt, das es in C
    /// nicht gibt. Grob, aber ohne Fehlgriff bei C: ein Treffer in einem
    /// Kommentar genuegt freilich auch.
    [[nodiscard]] bool erkennt(std::string_view text) const override
    {
        return hatWort(text, "class") || hatWort(text, "namespace") || hatWort(text, "template") ||
               hatWort(text, "public") || hatWort(text, "private") || text.find("::") != std::string_view::npos;
    }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                                const LeseOptionen& optionen) const override
    {
        return cfamilie::lies(text, einheitName, kTabelle, {.cpp = true}, optionen);
    }
};

}  // namespace

const Sprache& cpp()
{
    static const Cpp sprache;
    return sprache;
}

}  // namespace c2d::sprache
