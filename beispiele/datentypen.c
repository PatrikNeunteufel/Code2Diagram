/* Beispiel: enum, struct, union, typedef und wie sie zusammenhaengen. */
typedef unsigned char u8;

enum Farbe { ROT, GRUEN = 4, BLAU };

struct Punkt {
    int x, y;
};

typedef struct Knoten {
    struct Punkt ort;
    enum Farbe farbe;
    struct Knoten *naechster;
    u8 daten[16];
    unsigned bereit : 1;
    union {
        int ganz;
        float bruch;
    } wert;
    void (*rueckruf)(struct Knoten *k, int grund);
} Knoten_t;

typedef struct Punkt Punkt_t;

typedef struct {
    Knoten_t *kopf;
    Punkt_t mitte;
} Liste;

struct Punkt ursprung = { 0, 0 };
