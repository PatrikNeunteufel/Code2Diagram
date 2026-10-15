/* Beispielprojekt: der Antrieb. MOTOREN kommt aus dem Abschnitt "antrieb" der Projektdatei. */
#include "motor.h"

static int bereit[4];

/* static: gleicher Name wie in main.c, aber eine andere Funktion */
static int pruefe(int nummer)
{
    return bereit[nummer] != 0;
}

void motor_starte(int anzahl)
{
    for (int i = 0; i < anzahl; i++) {
        bereit[i] = 1;
        if (!pruefe(i))
            bereit[i] = 0;
    }
}

#if MOTOREN > 1
void motor_zwei_gleich(void)
{
    bereit[1] = bereit[0];
}
#endif
