/**
 * @file Sprachen.cpp
 * @brief C# und Java: gelesen mit dem Leser von csharp/, je mit ihrer Tabelle
 */
#include "CSharp.hpp"

namespace c2d::sprache
{

namespace
{

constexpr std::string_view kEndungenCs[] = {"cs"};
constexpr std::string_view kEndungenJava[] = {"java"};

class CSharp final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "C#"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungenCs; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return csjava::tabelle({.java = false}); }
    [[nodiscard]] bool hatPraeprozessor() const override { return true; }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                                const LeseOptionen& optionen) const override
    {
        return csjava::lies(text, einheitName, optionen, {.java = false});
    }
};

class Java final : public Sprache
{
public:
    [[nodiscard]] std::string_view name() const override { return "Java"; }
    [[nodiscard]] std::span<const std::string_view> endungen() const override { return kEndungenJava; }
    [[nodiscard]] const lexer::Sprachtabelle& tabelle() const override { return csjava::tabelle({.java = true}); }

    [[nodiscard]] Ergebnis lies(std::string_view text, std::string_view einheitName,
                                const LeseOptionen& optionen) const override
    {
        return csjava::lies(text, einheitName, optionen, {.java = true});
    }
};

}  // namespace

const Sprache& csharp()
{
    static const CSharp sprache;
    return sprache;
}

const Sprache& java()
{
    static const Java sprache;
    return sprache;
}

}  // namespace c2d::sprache
