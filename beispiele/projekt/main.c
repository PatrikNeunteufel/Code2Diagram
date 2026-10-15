/* Beispielprojekt: die Hauptschleife ruft Funktionen aus anderen Dateien. */
#include "config.h"
#include "antrieb/motor.h"

void regle(int soll);
void protokolliere(const char *text);

/* static: nur in dieser Datei - antrieb/motor.c hat eine eigene */
static int pruefe(void)
{
    return TAKT_MS > 0;
}

int main(void)
{
    if (!pruefe())
        return 1;
    motor_starte(2);
    while (1) {
        regle(100);
#ifdef MIT_DIAGNOSE
        protokolliere("Takt");
#endif
    }
    return 0;
}
