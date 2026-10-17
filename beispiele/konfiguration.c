/* Beispiel: bedingte Uebersetzung - welcher Zweig bei welchen Definitionen gilt. */
#define TAKT_MHZ 100

#if defined(BOARD_A) || defined(BOARD_B)
#define LEDS 4
#elif VERSION >= 2
#define LEDS 2
#else
#define LEDS 1
#endif

#ifdef DEBUG
#if LOG_STUFE > 1
void protokolliere(const char *text);
#endif
#endif

#if TAKT_MHZ > 50
void schnell(void) {}
#else
void langsam(void) {}
#endif

int leds(void)
{
    return LEDS;
}
