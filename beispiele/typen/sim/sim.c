/* Beispiel: Links auf Datentypen - ein gleichnamiger Typ, den main.c nicht einbindet. */
typedef struct {
    int schritt;
} t_PID;

void simuliere(t_PID *p)
{
    p->schritt++;
}
