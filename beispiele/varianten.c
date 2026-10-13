/* Beispiel: bedingte Uebersetzung - je nach Definitionen eine andere Variante. */
#include <stdint.h>

#define PUFFER 64

#if PUFFER > 32
typedef uint16_t index_t;
#else
typedef uint8_t index_t;
#endif

#ifdef BOARD_A
static void sende(uint8_t byte) { uart0_schreibe(byte); }
#elif defined(BOARD_B)
static void sende(uint8_t byte) { spi_schreibe(byte); }
#else
static void sende(uint8_t byte) { (void)byte; }
#endif

void melde(const uint8_t *daten, index_t laenge)
{
#if LOG_STUFE >= 2
    protokolliere("melde", laenge);
#endif
    for (index_t i = 0; i < laenge; i++) {
#ifdef MIT_PRUEFSUMME
        if (!pruefe(daten[i]))
            break;
#endif
        sende(daten[i]);
    }
}
