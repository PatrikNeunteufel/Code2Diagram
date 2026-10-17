/**
 * @file CSprache.cpp
 * @brief C: die Sprachtabelle; gelesen wird mit dem Leser der C-Familie
 */
#include "../cfamilie/CFamilie.hpp"

namespace c2d::sprache
{

namespace
{

// C11. Die Listen gehen auf die der Vorgaenger zurueck (docs/Ernte_Vorgaenger.md),
// bereinigt: nur, was der Standard ein Schluesselwort nennt.
constexpr std::string_view kSchluesselwoerter[] = {
    "auto",     "break",    "case",     "char",          "const",          "continue",  "default",
    "do",       "double",   "else",     "enum",          "extern",         "float",     "for",
    "goto",     "if",       "inline",   "int",           "long",           "register",  "restrict",
    "return",   "short",    "signed",   "sizeof",        "static",         "struct",    "switch",
    "typedef",  "union",    "unsigned", "void",          "volatile",       "while",     "_Alignas",
    "_Alignof", "_Atomic",  "_Bool",    "_Complex",      "_Generic",       "_Imaginary", "_Noreturn",
    "_Static_assert",       "_Thread_local",
};

constexpr std::string_view kOperatoren[] = {
    "...", "<<=", ">>=", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", "&&", "||",
    "+=",  "-=",  "*=",  "/=", "%=", "&=", "^=", "|=", "##", "{",  "}",  "[",  "]",  "(",
    ")",   ";",   ",",   ".",  ":",  "?",  "~",  "!",  "+",  "-",  "*",  "/",  "%",  "&",
    "|",   "^",   "<",   ">",  "=",  "#",
};

constexpr std::string_view kEndungen[] = {"c", "h"};

constexpr lexer::Sprachtabelle kTabelle{
    .schluesselwoerter = kSchluesselwoerter,
    .operatoren = kOperatoren,
    .zeilenKommentar = "//",
    .blockKommentarAuf = "/*",
    .blockKommentarZu = "*/",
    .direktive = '#',
};

class C final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "C"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungen; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return kTabelle; }
    [[nodiscard]] bool hatPraeprozessor() const override { return true; }
    [[nodiscard]] bool bindetKoepfeEin() const override { return true; }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                                const LeseOptionen& optionen) const override
    {
        return cfamilie::lies(text, einheitName, kTabelle, {.cpp = false}, optionen);
    }
};

}  // namespace

const Sprache& c()
{
    static const C sprache;
    return sprache;
}

}  // namespace c2d::sprache
