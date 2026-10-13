/* Beispiel: eine Ampel als Zustandsautomat - enum, struct, switch. */
#include <stdint.h>

typedef enum { ROT, ROT_GELB, GRUEN, GELB } Phase;

typedef struct {
    Phase phase;
    uint16_t rest_ms;
    uint8_t stoerung : 1;
} Ampel;

static uint16_t dauer_ms(Phase phase)
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

void ampel_takt(Ampel *ampel, uint16_t vergangen_ms)
{
    if (ampel->stoerung) {
        ampel->phase = GELB;
        return;
    }
    if (ampel->rest_ms > vergangen_ms) {
        ampel->rest_ms -= vergangen_ms;
        return;
    }
    ampel->phase = (Phase)((ampel->phase + 1) % 4);
    ampel->rest_ms = dauer_ms(ampel->phase);
}
