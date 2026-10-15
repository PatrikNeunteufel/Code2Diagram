/* Beispielprojekt: das Protokoll. MIT_LOG kommt aus der Projektdatei. */
void schreibe(const char *text);

void protokolliere(const char *text)
{
#ifdef MIT_LOG
    schreibe(text);
#endif
}
