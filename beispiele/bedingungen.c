/* Beispiel: Bedingungen mit mehreren Teilen - je eine Wahrheitstabelle samt MC/DC. */
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t spannung_mv;
    uint16_t strom_ma;
    bool freigabe;
    bool not_aus;
} Kanal;

bool pruefe_sensor(const Kanal *k);

/* darf der Kanal einschalten? */
bool darf_einschalten(const Kanal *k)
{
    if (k->not_aus || !k->freigabe)
        return false;
    return k->spannung_mv > 11000 && k->spannung_mv < 13000 && pruefe_sensor(k);
}

/* Ueberstrom oder Unterspannung, ausser im Hochlauf */
void ueberwache(Kanal *k, bool hochlauf)
{
    while ((k->strom_ma > 2000 || k->spannung_mv < 9000) && !hochlauf) {
        k->freigabe = false;
    }
    const bool warnung = k->strom_ma > 1500 || k->spannung_mv < 10000;
    k->not_aus = warnung ? k->not_aus : false;
}
