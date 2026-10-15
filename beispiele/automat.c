/* Beispiel: ein Zustandsautomat - ein switch ueber den Zustand, der sich selbst weiterschaltet. */
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
