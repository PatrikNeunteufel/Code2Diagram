/**
 * @file Sprachen.cpp
 * @brief JavaScript und TypeScript: dieselbe Tabelle, gelesen mit dem Leser von ecma/
 */
#include "Ecma.hpp"

namespace c2d::sprache
{

namespace
{

// JSX/TSX (`.jsx`, `.tsx`) noch nicht: braucht eine eigene Betriebsart des Lexers fuer Tags.
constexpr std::string_view kEndungenJs[] = {"js", "mjs", "cjs"};
constexpr std::string_view kEndungenTs[] = {"ts", "mts", "cts"};

class JavaScript final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "JavaScript"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungenJs; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return ecma::tabelle(); }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                                const LeseOptionen& /*optionen*/) const override
    {
        return ecma::lies(text, einheitName, {.ts = false});
    }
};

class TypeScript final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "TypeScript"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungenTs; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return ecma::tabelle(); }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                                const LeseOptionen& /*optionen*/) const override
    {
        return ecma::lies(text, einheitName, {.ts = true});
    }
};

}  // namespace

const Sprache& javascript()
{
    static const JavaScript sprache;
    return sprache;
}

const Sprache& typescript()
{
    static const TypeScript sprache;
    return sprache;
}

}  // namespace c2d::sprache
