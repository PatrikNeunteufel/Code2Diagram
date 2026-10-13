/* Beispiel: Funktionen, die einander rufen - zum Springen und Einbetten in der Vorschau. */
static int quadrat(int x)
{
    return x * x;
}

static int summe(int n)
{
    int s = 0;
    for (int i = 0; i < n; i++)
        s += quadrat(i);
    return s;
}

int main(void)
{
    int n = lies();
    if (n < 0)
        return 1;
    drucke(summe(n));
    return 0;
}
