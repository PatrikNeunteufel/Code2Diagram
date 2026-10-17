/* Beispiel: Zuordnungen - ein switch, der nur Werte liefert, und konstante Tabellen. */
typedef enum { ROT, ROT_GELB, GRUEN, GELB } Phase;

typedef struct {
    unsigned short dauer_ms;
    unsigned char blinkt;
} PhasenDaten;

static const unsigned short dauer[4] = {5000, 1000, 4000, 1000};
static const PhasenDaten daten[] = {
    [ROT] = {5000, 0},
    [GRUEN] = {.blinkt = 0, .dauer_ms = 4000},
    {1000, 1},
};

unsigned short dauer_ms(Phase phase)
{
    switch (phase) {
    case ROT:
        return 5000;
    case GRUEN:
        return 4000;
    case ROT_GELB:
    case GELB:
        return 1000;
    }
    return 0;
}

void setze(Phase phase, int *rot, int *gruen)
{
    int r, g;
    switch (phase) {
    case ROT:
        r = 1;
        g = 0;
        break;
    case GRUEN:
        r = 0;
        g = 1;
        break;
    default:
        r = 0;
        g = 0;
        break;
    }
    *rot = r;
    *gruen = g;
}
