/* Beispiel: Zuweisungen, if/else, while. */
int summe_bis(int grenze)
{
    int summe = 0;
    int i = 1;
    if (grenze < 0) {
        return -1;
    } else {
        while (i <= grenze) {
            summe += i;
            i++;
        }
    }
    return summe;
}
