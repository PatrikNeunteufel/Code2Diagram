/* Beispiel: Links auf Datentypen - Typen in einem Kopf. */
#ifndef TYPEN_H
#define TYPEN_H

typedef struct {
    float kp;
    float ki;
} t_PID;

typedef enum { AUS, AN, FEHLER } t_MODUS;

struct Regler {
    t_PID pid;
    t_MODUS modus;
    struct Motor *motor;
};

struct Fern {
    int x;
};

#endif
