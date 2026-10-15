/* Beispielprojekt: der echte Regler. */
static int ist;

void regle(int soll)
{
    if (ist < soll)
        ist++;
    else if (ist > soll)
        ist--;
}
