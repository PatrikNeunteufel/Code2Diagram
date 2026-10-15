/**
 * @file Beispiele.hpp
 * @brief Die Quelltexte, an denen mehrere Testdateien pruefen
 *
 * kSummeBis steht wortgleich in beispiele/verzweigung.c, kDatentypen in
 * beispiele/datentypen.c, kMotor in beispiele/motor.cpp, kAutomat in beispiele/automat.c.
 */
#pragma once

#include <string_view>

namespace beispiele
{

inline constexpr std::string_view kSummeBis =
    R"c(/* Beispiel: Zuweisungen, if/else, while. */
int summe_bis(int grenze)
{
    int summe = 0;
    int i = 1;
    if (grenze < 0) {
        return -1;
    } else {
        while (i <= grenze) {
            summe += i;
            i++;
        }
    }
    return summe;
}
)c";

inline constexpr std::string_view kDatentypen =
    R"c(/* Beispiel: enum, struct, union, typedef und wie sie zusammenhaengen. */
typedef unsigned char u8;

enum Farbe { ROT, GRUEN = 4, BLAU };

struct Punkt {
    int x, y;
};

typedef struct Knoten {
    struct Punkt ort;
    enum Farbe farbe;
    struct Knoten *naechster;
    u8 daten[16];
    unsigned bereit : 1;
    union {
        int ganz;
        float bruch;
    } wert;
    void (*rueckruf)(struct Knoten *k, int grund);
} Knoten_t;

typedef struct Punkt Punkt_t;

typedef struct {
    Knoten_t *kopf;
    Punkt_t mitte;
} Liste;

struct Punkt ursprung = { 0, 0 };
)c";

/// Steht wortgleich in beispiele/motor.cpp.
inline constexpr std::string_view kMotor =
    R"cpp(// Beispiel: Klassen, Vererbung, Namensraum, Methoden - C++.
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
)cpp";

inline constexpr std::string_view kAutomat =
    R"c(/* Beispiel: ein Zustandsautomat - ein switch ueber den Zustand, der sich selbst weiterschaltet. */
typedef enum { AUS, STARTE, LAEUFT, FEHLER } Zustand;

typedef struct {
    Zustand zustand;
    int versuche;
} Motor;

static Motor motor;

void motor_takt(int bereit, int stoerung)
{
    switch (motor.zustand) {
    case AUS:
        if (bereit)
            motor.zustand = STARTE;
        break;
    case STARTE:
        if (stoerung) {
            motor.versuche++;
            if (motor.versuche > 3)
                motor.zustand = FEHLER;
        } else {
            motor.zustand = LAEUFT;
        }
        break;
    case LAEUFT:
        if (stoerung)
            motor.zustand = FEHLER;
        else if (!bereit)
            motor.zustand = AUS;
        break;
    case FEHLER:
        break;
    }
}

/* von aussen: das Quittieren setzt den Automaten zurueck */
void motor_quittiere(void)
{
    motor.versuche = 0;
    motor.zustand = AUS;
}
)c";

}  // namespace beispiele
