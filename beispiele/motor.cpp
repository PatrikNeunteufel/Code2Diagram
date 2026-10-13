// Beispiel: Klassen, Vererbung, Namensraum, Methoden - C++.
#include <string>
#include <vector>

namespace antrieb {

enum class Zustand : int { Aus, Anlauf, Betrieb, Fehler };

class Geraet {
public:
    virtual ~Geraet() = default;
    virtual bool starte() = 0;
    virtual void stoppe() = 0;
    const std::string& name() const { return m_name; }

protected:
    explicit Geraet(std::string name) : m_name(std::move(name)) {}

private:
    std::string m_name;
};

struct Messwert {
    double strom = 0.0;
    double drehzahl{0.0};
};

class Motor final : public Geraet {
public:
    Motor(std::string name, int polpaare);
    bool starte() override;
    void stoppe() override;
    static int anzahl();
    Motor& operator+=(const Messwert& wert);

private:
    Zustand m_zustand = Zustand::Aus;
    int m_polpaare;
    std::vector<Messwert> m_verlauf;
    Geraet* m_partner = nullptr;
    static int s_anzahl;
};

int Motor::s_anzahl = 0;

Motor::Motor(std::string name, int polpaare)
    : Geraet(std::move(name)), m_polpaare(polpaare)
{
    ++s_anzahl;
}

bool Motor::starte()
{
    if (m_zustand == Zustand::Fehler)
        return false;
    try {
        pruefe(m_polpaare);
        m_zustand = Zustand::Anlauf;
    } catch (const std::exception& e) {
        melde(e.what());
        m_zustand = Zustand::Fehler;
        throw;
    }
    for (const auto& wert : m_verlauf) {
        if (wert.strom > 10.0)
            break;
        glaette(wert);
    }
    m_zustand = Zustand::Betrieb;
    return true;
}

void Motor::stoppe() { m_zustand = Zustand::Aus; }

int Motor::anzahl() { return s_anzahl; }

Motor& Motor::operator+=(const Messwert& wert)
{
    m_verlauf.push_back(wert);
    return *this;
}

}  // namespace antrieb

template <typename T>
T begrenze(T wert, T unten, T oben)
{
    return wert < unten ? unten : (wert > oben ? oben : wert);
}

using Verlauf = std::vector<antrieb::Messwert>;
