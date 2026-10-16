/* Beispiel: Links auf Datentypen - ein Typ, der einen aus einem anderen Kopf enthaelt. */
#ifndef MOTOR_H
#define MOTOR_H

#include "typen.h"

struct Motor {
    int drehzahl;
    t_PID pid;
};

#endif
