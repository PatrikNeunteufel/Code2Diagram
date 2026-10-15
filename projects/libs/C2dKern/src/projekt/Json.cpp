/**
 * @file Json.cpp
 * @brief Der JSON-Leser: rekursiver Abstieg ueber den Text
 */
#include "Json.hpp"

#include <cstdio>

namespace c2d::projekt::json
{

namespace
{

class Leser
{
public:
    explicit Leser(std::string_view text) : m_text(text) {}

    bool liesAlles(Wert& aus, quelle::Meldung& fehler)
    {
        m_fehler = &fehler;
        leer();
        if (!liesWert(aus, 0)) return false;
        leer();
        if (m_i < m_text.size()) return scheitere("text after the end of the value");
        return true;
    }

private:
    static constexpr int kTiefe = 64;  // mehr Verschachtelung braucht keine Projektdatei

    [[nodiscard]] quelle::Position hier() const { return {m_i, m_zeile, static_cast<int>(m_i - m_zeilenAnfang) + 1}; }

    bool scheitere(std::string_view grund)
    {
        *m_fehler = {quelle::Schwere::Fehler, hier(), "not valid JSON: " + std::string(grund)};
        return false;
    }

    void weiter()
    {
        if (m_text[m_i] == '\n')
        {
            ++m_zeile;
            m_zeilenAnfang = m_i + 1;
        }
        ++m_i;
    }

    void leer()
    {
        while (m_i < m_text.size() &&
               (m_text[m_i] == ' ' || m_text[m_i] == '\t' || m_text[m_i] == '\r' || m_text[m_i] == '\n'))
            weiter();
    }

    bool wort(std::string_view w)
    {
        if (m_text.substr(m_i, w.size()) != w) return false;
        for (std::size_t k = 0; k < w.size(); ++k) weiter();
        return true;
    }

    bool liesWert(Wert& aus, int tiefe)
    {
        if (tiefe > kTiefe) return scheitere("nested too deeply");
        aus = Wert{};
        aus.ort = hier();
        if (m_i >= m_text.size()) return scheitere("a value is missing");
        const char c = m_text[m_i];
        if (c == '{') return liesObjekt(aus, tiefe);
        if (c == '[') return liesListe(aus, tiefe);
        if (c == '"')
        {
            aus.art = Art::Text;
            return liesText(aus.text);
        }
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            aus.art = Art::Zahl;
            return liesZahl(aus.text);
        }
        if (wort("true")) aus.art = Art::Wahr;
        else if (wort("false")) aus.art = Art::Falsch;
        else if (wort("null")) aus.art = Art::Nichts;
        else return scheitere("unexpected character");
        return true;
    }

    bool liesObjekt(Wert& aus, int tiefe)
    {
        aus.art = Art::Objekt;
        weiter();  // {
        leer();
        if (m_i < m_text.size() && m_text[m_i] == '}')
        {
            weiter();
            return true;
        }
        while (true)
        {
            leer();
            if (m_i >= m_text.size() || m_text[m_i] != '"') return scheitere("a key in quotes is expected");
            std::string name;
            if (!liesText(name)) return false;
            leer();
            if (m_i >= m_text.size() || m_text[m_i] != ':') return scheitere("':' is expected after a key");
            weiter();
            leer();
            Wert wert;
            if (!liesWert(wert, tiefe + 1)) return false;
            aus.namen.push_back(std::move(name));
            aus.elemente.push_back(std::move(wert));
            leer();
            if (m_i < m_text.size() && m_text[m_i] == ',')
            {
                weiter();
                continue;
            }
            if (m_i < m_text.size() && m_text[m_i] == '}')
            {
                weiter();
                return true;
            }
            return scheitere("',' or '}' is expected");
        }
    }

    bool liesListe(Wert& aus, int tiefe)
    {
        aus.art = Art::Liste;
        weiter();  // [
        leer();
        if (m_i < m_text.size() && m_text[m_i] == ']')
        {
            weiter();
            return true;
        }
        while (true)
        {
            leer();
            Wert wert;
            if (!liesWert(wert, tiefe + 1)) return false;
            aus.elemente.push_back(std::move(wert));
            leer();
            if (m_i < m_text.size() && m_text[m_i] == ',')
            {
                weiter();
                continue;
            }
            if (m_i < m_text.size() && m_text[m_i] == ']')
            {
                weiter();
                return true;
            }
            return scheitere("',' or ']' is expected");
        }
    }

    static void alsUtf8(unsigned long zeichen, std::string& aus)
    {
        if (zeichen < 0x80)
            aus += static_cast<char>(zeichen);
        else if (zeichen < 0x800)
        {
            aus += static_cast<char>(0xC0 | (zeichen >> 6));
            aus += static_cast<char>(0x80 | (zeichen & 0x3F));
        }
        else if (zeichen < 0x10000)
        {
            aus += static_cast<char>(0xE0 | (zeichen >> 12));
            aus += static_cast<char>(0x80 | ((zeichen >> 6) & 0x3F));
            aus += static_cast<char>(0x80 | (zeichen & 0x3F));
        }
        else
        {
            aus += static_cast<char>(0xF0 | (zeichen >> 18));
            aus += static_cast<char>(0x80 | ((zeichen >> 12) & 0x3F));
            aus += static_cast<char>(0x80 | ((zeichen >> 6) & 0x3F));
            aus += static_cast<char>(0x80 | (zeichen & 0x3F));
        }
    }

    bool vierHex(unsigned long& wert)
    {
        wert = 0;
        for (int k = 0; k < 4; ++k)
        {
            if (m_i >= m_text.size()) return scheitere("four hex digits are expected after \\u");
            const char c = m_text[m_i];
            unsigned long ziffer = 0;
            if (c >= '0' && c <= '9') ziffer = static_cast<unsigned long>(c - '0');
            else if (c >= 'a' && c <= 'f') ziffer = static_cast<unsigned long>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') ziffer = static_cast<unsigned long>(c - 'A' + 10);
            else return scheitere("four hex digits are expected after \\u");
            wert = wert * 16 + ziffer;
            weiter();
        }
        return true;
    }

    bool liesText(std::string& aus)
    {
        weiter();  // "
        while (true)
        {
            if (m_i >= m_text.size()) return scheitere("the closing quote is missing");
            const char c = m_text[m_i];
            if (c == '"')
            {
                weiter();
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) return scheitere("a control character in a string");
            if (c != '\\')
            {
                aus += c;
                weiter();
                continue;
            }
            weiter();
            if (m_i >= m_text.size()) return scheitere("the closing quote is missing");
            const char e = m_text[m_i];
            weiter();
            switch (e)
            {
                case '"': aus += '"'; break;
                case '\\': aus += '\\'; break;
                case '/': aus += '/'; break;
                case 'b': aus += '\b'; break;
                case 'f': aus += '\f'; break;
                case 'n': aus += '\n'; break;
                case 'r': aus += '\r'; break;
                case 't': aus += '\t'; break;
                case 'u':
                {
                    unsigned long zeichen = 0;
                    if (!vierHex(zeichen)) return false;
                    // ein Paar fuer ein Zeichen ausserhalb der Grundebene
                    if (zeichen >= 0xD800 && zeichen < 0xDC00 && m_text.substr(m_i, 2) == "\\u")
                    {
                        weiter();
                        weiter();
                        unsigned long zweites = 0;
                        if (!vierHex(zweites)) return false;
                        if (zweites >= 0xDC00 && zweites < 0xE000)
                            zeichen = 0x10000 + ((zeichen - 0xD800) << 10) + (zweites - 0xDC00);
                    }
                    alsUtf8(zeichen, aus);
                    break;
                }
                default: return scheitere("unknown escape sequence");
            }
        }
    }

    bool liesZahl(std::string& aus)
    {
        const std::size_t anfang = m_i;
        const auto ziffern = [&] {
            const std::size_t vorher = m_i;
            while (m_i < m_text.size() && m_text[m_i] >= '0' && m_text[m_i] <= '9') weiter();
            return m_i > vorher;
        };
        if (m_text[m_i] == '-') weiter();
        if (!ziffern()) return scheitere("a digit is expected");
        if (m_i < m_text.size() && m_text[m_i] == '.')
        {
            weiter();
            if (!ziffern()) return scheitere("a digit is expected after '.'");
        }
        if (m_i < m_text.size() && (m_text[m_i] == 'e' || m_text[m_i] == 'E'))
        {
            weiter();
            if (m_i < m_text.size() && (m_text[m_i] == '+' || m_text[m_i] == '-')) weiter();
            if (!ziffern()) return scheitere("a digit is expected in the exponent");
        }
        aus = std::string(m_text.substr(anfang, m_i - anfang));
        return true;
    }

    std::string_view m_text;
    std::size_t m_i = 0;
    int m_zeile = 1;
    std::size_t m_zeilenAnfang = 0;
    quelle::Meldung* m_fehler = nullptr;
};

}  // namespace

bool lies(std::string_view text, Wert& aus, quelle::Meldung& fehler)
{
    return Leser(text).liesAlles(aus, fehler);
}

std::string zitat(std::string_view text)
{
    std::string aus = "\"";
    for (const char c : text)
    {
        switch (c)
        {
            case '"': aus += "\\\""; break;
            case '\\': aus += "\\\\"; break;
            case '\n': aus += "\\n"; break;
            case '\r': aus += "\\r"; break;
            case '\t': aus += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    char puffer[8];
                    std::snprintf(puffer, sizeof puffer, "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                    aus += puffer;
                }
                else
                    aus += c;
                break;
        }
    }
    return aus + "\"";
}

std::string_view name(Art art)
{
    switch (art)
    {
        case Art::Nichts: return "null";
        case Art::Wahr:
        case Art::Falsch: return "a boolean";
        case Art::Zahl: return "a number";
        case Art::Text: return "a string";
        case Art::Liste: return "a list";
        case Art::Objekt: return "an object";
    }
    return "a value";
}

}  // namespace c2d::projekt::json
