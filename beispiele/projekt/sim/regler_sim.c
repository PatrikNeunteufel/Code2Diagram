/* Beispielprojekt: ein Regler fuer die Simulation - gleicher Name wie in antrieb/regler.c.
   Welcher gemeint ist, entscheidet erst der Linker; Code2Diagram laesst die Wahl. */
static int ist;

void regle(int soll)
{
    ist = soll;
}
