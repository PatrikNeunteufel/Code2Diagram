/* Beispiel: Links auf Datentypen - der Ablauf nennt Typen aus den Koepfen. */
#include "inc/motor.h"

typedef struct Motor Motor_t;

static float regle(struct Regler *r, float fehler)
{
    return r->pid.kp * fehler;
}

int main(void)
{
    Motor_t motor = {0};
    struct Regler regler = {{1.0f, 0.1f}, AN, &motor};
    float stell = regle(&regler, 2.0f);
    return (int)stell + (int)sizeof(t_PID);
}
