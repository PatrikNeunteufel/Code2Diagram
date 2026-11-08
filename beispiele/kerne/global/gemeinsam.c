/* Laeuft auf beiden Kernen, je Kern anders uebersetzt (CPU1 bzw. CPU2 aus "kerne" in code2diagram.json). */
#include "gemeinsam.h"

void init_gemeinsam(void)
{
#if defined(CPU1)
    init_takt();
    init_gpio();
#else
    warte_auf_takt();
#endif
}

void hintergrund(void)
{
    pruefe_fehler();
}
