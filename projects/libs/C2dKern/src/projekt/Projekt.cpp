/**
 * @file Projekt.cpp
 * @brief Ein Projekt einlesen: Ordner durchsuchen, zweimal lesen, Aufrufe aufloesen
 */
#include "c2d/projekt/Projekt.hpp"

#include "Notebook.hpp"
#include "Zeilen.hpp"

#include <algorithm>
#include <iterator>
#include <optional>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <system_error>
#include <unordered_map>

namespace c2d::projekt
{

namespace fs = std::filesystem;

namespace
{

using quelle::Meldung;
using quelle::Schwere;

/// Eine Meldung ueber einen Ordner oder eine ganze Datei: Zeile 0.
constexpr quelle::Position kOhneZeile{0, 0, 0};

std::string alsText(const fs::path& weg)
{
    const std::u8string u8 = weg.generic_u8string();
    return {reinterpret_cast<const char*>(u8.data()), u8.size()};
}

fs::path alsWeg(std::string_view text)
{
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

/// Fuer Vergleiche von Wegen: Windows unterscheidet Gross und Klein nicht.
std::string klein(std::string_view text)
{
    std::string aus(text);
    for (char& c : aus)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return aus;
}

std::string ordnerVon(std::string_view pfad)
{
    const std::size_t strich = pfad.rfind('/');
    return strich == std::string_view::npos ? std::string() : std::string(pfad.substr(0, strich));
}

std::string nameVon(std::string_view pfad)
{
    const std::size_t strich = pfad.find_last_of("/\\");
    return std::string(strich == std::string_view::npos ? pfad : pfad.substr(strich + 1));
}

/// Windows oeffnet einen Weg ab 260 Zeichen nur mit dem Vorsatz `\\?\` (absolut, mit `\`) — eine
/// Datei wie einen Ordner (Sourcetrail-Testdaten: 18 Dateien „cannot read the file“).
fs::path oeffenbar(const fs::path& weg)
{
#ifdef _WIN32
    constexpr std::size_t kHoechstens = 259;
    if (weg.native().size() <= kHoechstens || weg.native().starts_with(L"\\\\?\\")) return weg;
    std::error_code fehler;
    const fs::path absolut = fs::absolute(weg, fehler).lexically_normal().make_preferred();
    if (fehler || absolut.native().starts_with(L"\\\\")) return weg;  // ein UNC-Weg hiesse \\?\UNC\…
    return fs::path(L"\\\\?\\" + absolut.native());
#else
    return weg;
#endif
}

/// Die ersten `n` Bytes einer Datei; leer = sie liess sich nicht oeffnen.
std::string anfangVon(const fs::path& weg, std::size_t n)
{
    std::ifstream strom(oeffenbar(weg), std::ios::binary);
    if (!strom) return {};
    std::string aus(n, '\0');
    strom.read(aus.data(), static_cast<std::streamsize>(n));
    aus.resize(static_cast<std::size_t>(strom.gcount()));
    return aus;
}

/// Ein Name ohne Endung (`app-20170708141026-0013`, `local-1794057617951`) — so heisst ein Event-Log (L1).
bool ohneEndung(std::string_view name)
{
    return name.find('.') == std::string_view::npos;
}

/// So benennt Spark seine Event-Logs (L1-N): `app-…` (Standalone), `local-…`, `application_…` (YARN), `spark-…`
/// (Kubernetes), `events_…` (rolling). Nur eine Datei ohne Endung mit so einem Namen wird im Projekt geoeffnet.
constexpr std::string_view kLogNamen[] = {"app-", "local-", "application_", "spark-", "events_"};

bool logName(std::string_view name)
{
    return std::any_of(std::begin(kLogNamen), std::end(kLogNamen), [&](std::string_view v) { return name.starts_with(v); });
}

/// Die Endungen eines komprimierten Event-Logs (Spark 4 komprimiert vorgegeben, L1).
constexpr std::string_view kKomprimiert[] = {"zstd", "lz4", "snappy", "lzf"};

/// Ein Name wie ein komprimierter Event-Log: `app-…`, `local-…`, `application_…`, `events_…` mit einer Endung aus
/// kKomprimiert.
bool komprimierterLog(std::string_view name)
{
    const std::size_t punkt = name.rfind('.');
    if (punkt == std::string_view::npos) return false;
    const std::string_view endung = name.substr(punkt + 1);
    if (std::find(std::begin(kKomprimiert), std::end(kKomprimiert), endung) == std::end(kKomprimiert)) return false;
    return logName(name);
}


/// Gleichnamiges ersetzt: das Spaetere gilt, an der Stelle des Frueheren (auch innerhalb von `dazu`).
/// Ueber einen Index nach Namen: die Koepfe eines Projekts bringen Tausende `#define` je Datei.
void fuegeHinzu(std::vector<sprache::Definition>& ziel, const std::vector<sprache::Definition>& dazu)
{
    if (dazu.empty()) return;
    std::unordered_map<std::string, std::size_t> stelle;  // Name -> Index in `ziel`
    stelle.reserve(ziel.size() + dazu.size());
    for (std::size_t i = 0; i < ziel.size(); ++i) stelle.try_emplace(ziel[i].name, i);  // der erste gilt als Ort
    for (const sprache::Definition& d : dazu)
    {
        const auto [wo, neu] = stelle.try_emplace(d.name, ziel.size());
        if (neu)
            ziel.push_back(d);
        else
            ziel[wo->second] = d;  // auch eine Aufhebung `!NAME` oder ihr Gegenteil
    }
}

/// Was ein `#define` fuer die Auswertung beitraegt: ohne Text oder mit Parametern gilt 1.
sprache::Definition alsDefinition(const modell::Makro& m)
{
    std::string wert = m.text;
    while (!wert.empty() && (wert.front() == ' ' || wert.front() == '\t')) wert.erase(wert.begin());
    while (!wert.empty() && (wert.back() == ' ' || wert.back() == '\t')) wert.pop_back();
    if (wert.empty() || wert.front() == '(') wert = "1";
    return {m.name, wert};
}

/// Woran der Host-Code den Dialekt seines SQL zeigt (Entscheid S3): eine Einbindung, die so heisst oder
/// so beginnt (`import duckdb`, `from pyspark.sql import …`, `using Npgsql;`), oder — Qt — der Name des
/// Treibers als Zeichenkette (`QSqlDatabase::addDatabase("QSQLITE")`).
struct SqlBibliothek
{
    std::string_view name;
    std::string_view dialekt;
};

constexpr SqlBibliothek kSqlBibliotheken[] = {
    {"duckdb", "duckdb"},
    {"sqlite3", "sqlite"},
    {"aiosqlite", "sqlite"},
    {"psycopg", "postgres"},
    {"psycopg2", "postgres"},
    {"asyncpg", "postgres"},
    {"pyspark", "spark"},
    {"databricks", "spark"},
    {"pymysql", "mysql"},
    {"MySQLdb", "mysql"},
    {"mysql", "mysql"},  // auch mysql.connector
    {"Npgsql", "postgres"},
    {"Microsoft.Data.Sqlite", "sqlite"},
    {"System.Data.SQLite", "sqlite"},
    {"Microsoft.Data.SqlClient", "tsql"},
    {"System.Data.SqlClient", "tsql"},
    {"MySql.Data", "mysql"},
    {"MySqlConnector", "mysql"},
    {"better-sqlite3", "sqlite"},
    {"pg", "postgres"},
    {"mysql2", "mysql"},
    {"android.database.sqlite", "sqlite"},
    {"androidx.room", "sqlite"},
    {"androidx.sqlite", "sqlite"},
    {"org.sqlite", "sqlite"},
    {"org.postgresql", "postgres"},
};

constexpr SqlBibliothek kQtTreiber[] = {{"QSQLITE", "sqlite"}, {"QPSQL", "postgres"}, {"QMYSQL", "mysql"}};

/// Die Dialekte, die der Host-Code zeigt, jeder einmal, mit dem, woran.
std::vector<std::pair<std::string_view, std::string>> dialekteVonHost(const sprache::Ergebnis& e)
{
    std::vector<std::pair<std::string_view, std::string>> aus;
    const auto nimm = [&](std::string_view dialekt, std::string woran) {
        if (std::none_of(aus.begin(), aus.end(), [&](const auto& x) { return x.first == dialekt; }))
            aus.emplace_back(dialekt, std::move(woran));
    };
    for (const modell::Einbindung& b : e.einheit.einbindungen)
    {
        std::string_view ziel = b.ziel;
        while (!ziel.empty() && (ziel.front() == '"' || ziel.front() == '\'')) ziel.remove_prefix(1);
        while (!ziel.empty() && (ziel.back() == '"' || ziel.back() == '\'' || ziel.back() == ';')) ziel.remove_suffix(1);
        for (const SqlBibliothek& s : kSqlBibliotheken)
            if (ziel == s.name || (ziel.starts_with(s.name) && ziel.size() > s.name.size() &&
                                   std::string_view(".:/").find(ziel[s.name.size()]) != std::string_view::npos))
                nimm(s.dialekt, std::string(s.name));
    }
    for (const sprache::Zeichenkette& z : e.zeichenketten)
        for (const SqlBibliothek& s : kQtTreiber)
            if (z.inhalt == s.name) nimm(s.dialekt, std::string(s.name));
    return aus;
}

/// Wie der Name eines Platzhalters im SQL beginnt (LeseOptionen::platzhalter).
constexpr std::string_view kPlatzhalter = "c2d_platzhalter_";

/// Aufrufe, die SQL ausfuehren: eine Zeichenkette als ihr Argument steht in klarer Form (Entscheid A4).
constexpr std::string_view kSqlAufrufe[] = {
    "execute",        "executemany",  "executescript", "exec",          "execSQL",      "rawQuery",
    "query",          "prepare",      "sql",           "read_sql",      "read_sql_query", "SqlCommand",
    "QSqlQuery",      "compileStatement", "prepareStatement", "executeQuery", "executeUpdate",
    "ExecuteSqlRaw",  "FromSqlRaw",   "ExecuteNonQuery", "ExecuteScalar"};

/// Eine Zeichenkette, wie der SQL-Leser sie sieht.
struct SqlStueck
{
    std::string text;
    std::vector<std::size_t> woher;      ///< je Byte von `text` der Versatz in der Quelle
    std::vector<std::string> originale;  ///< je Platzhalter (ab der Nummer `erster`) der Text der Quelle
};

/// Je Platzhalter ein Name `kPlatzhalter<n>_` (n ab `erster`), zwischen Literalen auf verschiedenen Zeilen
/// (`"CREATE TABLE a ("` / `"  x INT)"`) ein Zeilenwechsel — fuer SQL Leerraum, und jede Zeile des SQL
/// liegt auf einer der Quelle.
SqlStueck alsSql(const sprache::Zeichenkette& z, const std::string& quelle, std::size_t erster)
{
    SqlStueck s;
    std::size_t naechster = 0;  // der naechste Platzhalter von z
    for (std::size_t j = 0; j < z.inhalt.size(); ++j)
    {
        if (naechster < z.platzhalter.size() && z.platzhalter[naechster].anfang == j)
        {
            const auto [anfang, laenge] = z.platzhalter[naechster++];
            const std::string ersatz = std::string(kPlatzhalter) + std::to_string(erster + s.originale.size()) + "_";
            s.originale.push_back(z.inhalt.substr(anfang, laenge));
            s.text += ersatz;
            s.woher.insert(s.woher.end(), ersatz.size(), z.versatz[anfang]);
            j += laenge - 1;
            continue;
        }
        if (j > 0 && z.versatz[j] > z.versatz[j - 1] + 1 && z.inhalt[j - 1] != '\n' &&
            quelle.find('\n', z.versatz[j - 1] + 1) < z.versatz[j])
        {
            s.text += '\n';
            s.woher.push_back(z.versatz[j - 1]);
        }
        s.text += z.inhalt[j];
        s.woher.push_back(z.versatz[j]);
    }
    return s;
}

/// Die Funktion, in deren Rumpf die Zeile liegt — die innerste; nullptr = keine.
modell::Funktion* funktionAn(modell::Einheit& einheit, int zeile)
{
    modell::Funktion* beste = nullptr;
    for (modell::Funktion& f : einheit.funktionen)
        if (f.bisZeile > 0 && f.zeile <= zeile && zeile <= f.bisZeile &&
            (beste == nullptr || f.bisZeile - f.zeile < beste->bisZeile - beste->zeile))
            beste = &f;
    return beste;
}

/// SQL in den Zeichenketten einer Datei (Entscheide S1, E1 a): jede, die mit einer Anweisung des Schemas beginnt
/// (sprache::beginntMitSchema), wird gelesen — alle einer Datei als ein Text, damit ein Fremdschluessel seine
/// Tabelle in einer anderen Zeichenkette findet; ihre Tabellen und Objekte kommen in die Einheit der Datei,
/// ihre Stellen und Meldungen auf die Stellen der Quelle umgerechnet. Der Dialekt: der der Projektdatei, sonst
/// der, den der Host-Code zeigt. Eine mit `+` zusammengesetzte Zeichenkette bleibt aussen vor (Hinweis).
/// Eine Vorlage (Entscheid F1 a) wird gelesen: jeder Platzhalter steht im SQL als Name `kPlatzhalter<n>_`,
/// danach wieder als Text der Quelle (`projekt_id: %1`); steht er in einem Namen, laesst der SQL-Leser die
/// Anweisung aus (Hinweis). Dazu (Entscheid A4) jede Zeichenkette mit SQL im Rumpf einer Funktion: was sie
/// liest, schreibt, ruft, kommt an die Funktion — `unsicher`, wenn sie nicht Argument eines Aufrufs aus
/// kSqlAufrufe ist.
void lieseingebettetesSql(sprache::Ergebnis& e, const std::string& text, const std::string& name,
                          const std::string& dialekt)
{
    std::string sql;
    std::vector<std::size_t> herkunft;  // je Byte von `sql` der Versatz in `text`
    std::vector<std::string> platzhalter;  // je Nummer der Text der Quelle
    const Zeilen quelle(text);

    // der Dialekt: der der Projektdatei, sonst der, den der Host-Code zeigt
    sprache::LeseOptionen lesen;
    lesen.dialekt = dialekt;
    std::string bibliotheken;
    std::size_t mehrereBibliotheken = 0;
    if (lesen.dialekt.empty())
    {
        const auto host = dialekteVonHost(e);
        if (host.size() == 1) lesen.dialekt = std::string(host.front().first);
        for (const auto& [d, woran] : host) bibliotheken += (bibliotheken.empty() ? "" : ", ") + woran;
        mehrereBibliotheken = host.size() > 1 ? host.size() : 0;
    }

    // die Platzhalter wieder, wie sie in der Quelle stehen (`alle`: je Nummer der Text)
    const auto zurueckIn = [](std::string& s, const std::vector<std::string>& alle) {
        for (std::size_t p = s.find(kPlatzhalter); p != std::string::npos; p = s.find(kPlatzhalter, p))
        {
            std::size_t q = p + kPlatzhalter.size();
            std::size_t n = 0;
            while (q < s.size() && s[q] >= '0' && s[q] <= '9') n = n * 10 + static_cast<std::size_t>(s[q++] - '0');
            if (q >= s.size() || s[q] != '_' || n >= alle.size())
            {
                p = q;
                continue;
            }
            s.replace(p, q + 1 - p, alle[n]);
            p += alle[n].size();
        }
    };

    for (const sprache::Zeichenkette& z : e.zeichenketten)
    {
        if (z.inhalt.empty()) continue;
        SqlStueck stueck = alsSql(z, text, platzhalter.size());
        // jede Anweisung, nicht nur das Schema: was eine Funktion des Hosts liest, schreibt, ruft (A4), und
        // ihre Abfragen (Q1) — je Zeichenkette fuer sich
        if (sprache::beginntMitSql(stueck.text, z.zusammengesetzt))
        {
            modell::Funktion* f = funktionAn(e.einheit, quelle.ort(z.versatz.front()).zeile);
            if (z.zusammengesetzt)
            {
                if (f != nullptr)
                    e.meldungen.push_back({Schwere::Hinweis, quelle.ort(z.versatz.front()),
                                           "SQL in a string composed with + is not read"});
            }
            else
            {
                sprache::LeseOptionen o = lesen;
                o.platzhalter = std::string(kPlatzhalter);
                sprache::Ergebnis s = sprache::sql().lies(stueck.text, name, o);
                const Zeilen inSql(stueck.text);
                const auto hostZeile = [&](int zeileImSql) {
                    return quelle.ort(stueck.woher[std::min(inSql.anfang(zeileImSql), stueck.woher.size() - 1)]).zeile;
                };
                const bool sicher = std::find(std::begin(kSqlAufrufe), std::end(kSqlAufrufe),
                                              std::string_view(z.aufruf)) != std::end(kSqlAufrufe);
                for (modell::Zugriff zugriff : s.einheit.zugriffe)
                {
                    if (f == nullptr) break;
                    zugriff.zeile = hostZeile(zugriff.zeile);
                    zugriff.unsicher = !sicher;
                    const bool schon = std::any_of(f->zugriffe.begin(), f->zugriffe.end(), [&](const modell::Zugriff& x) {
                        return x.art == zugriff.art && x.ziel == zugriff.ziel && x.imAusdruck == zugriff.imAusdruck;
                    });
                    if (!schon) f->zugriffe.push_back(std::move(zugriff));
                }
                // die Abfragen: Zeilen der Quelle, Name nach der Funktion, Platzhalter als Text der Quelle
                std::vector<std::string> alle = platzhalter;
                alle.insert(alle.end(), stueck.originale.begin(), stueck.originale.end());
                for (modell::Abfrage& q : s.einheit.abfragen)
                {
                    q.zeile = hostZeile(q.zeile);
                    q.bisZeile = hostZeile(q.bisZeile);
                    q.name = (f != nullptr ? f->vollerName() : std::string("query")) + "@" + std::to_string(q.zeile);
                    q.zugriffe.clear();  // sie traegt die Funktion (A4), die Anweisung ist kein eigener Knoten
                    zurueckIn(q.ziel, alle);
                    for (modell::AbfrageBlock& b : q.bloecke)
                    {
                        b.zeile = hostZeile(b.zeile);
                        for (std::string* t : {&b.auswahl, &b.gruppe, &b.ordnung, &b.grenze, &b.fenster, &b.titel})
                            zurueckIn(*t, alle);
                        for (modell::AbfrageQuelle& x : b.quellen)
                        {
                            x.zeile = hostZeile(x.zeile);
                            zurueckIn(x.name, alle);
                            for (modell::Bedingungsteil& t : x.bedingung) zurueckIn(t.text, alle);
                        }
                        for (modell::Bedingungsteil& t : b.wo) zurueckIn(t.text, alle);
                        for (modell::Bedingungsteil& t : b.haben) zurueckIn(t.text, alle);
                    }
                    e.einheit.abfragen.push_back(std::move(q));
                }
            }
        }
        // das Schema (CREATE, ALTER, DROP) aller Zeichenketten als ein Text; eine zusammengesetzte traegt nur
        // ihren Anfang (`"CREATE TABLE " + name`)
        if (!sprache::beginntMitSchema(stueck.text, z.zusammengesetzt)) continue;
        if (z.zusammengesetzt)
        {
            if (funktionAn(e.einheit, quelle.ort(z.versatz.front()).zeile) == nullptr)  // sonst schon gemeldet
                e.meldungen.push_back({Schwere::Hinweis, quelle.ort(z.versatz.front()),
                                       "SQL in a string composed with + is not read"});
            continue;
        }
        sql += stueck.text + ";\n";  // `;` trennt, auch wenn die Zeichenkette keins traegt
        herkunft.insert(herkunft.end(), stueck.woher.begin(), stueck.woher.end());
        herkunft.insert(herkunft.end(), 2, z.versatz.back());
        platzhalter.insert(platzhalter.end(), stueck.originale.begin(), stueck.originale.end());
    }
    if (sql.empty()) return;

    if (mehrereBibliotheken > 0)
        e.meldungen.push_back({Schwere::Hinweis, quelle.ort(herkunft.front()),
                               "several SQL libraries (" + bibliotheken + ") - SQL read as common SQL"});
    if (!platzhalter.empty()) lesen.platzhalter = std::string(kPlatzhalter);
    sprache::Ergebnis s = sprache::sql().lies(sql, name, lesen);

    const Zeilen inSql(sql);
    const auto ort = [&](std::size_t versatz) { return quelle.ort(herkunft[std::min(versatz, herkunft.size() - 1)]); };
    const auto zeile = [&](int z) { return ort(inSql.anfang(z)).zeile; };
    // die Platzhalter wieder, wie sie in der Quelle stehen (in Typen, Zusaetzen, Meldungen)
    const auto zurueck = [&](std::string& s) {
        for (std::size_t p = s.find(kPlatzhalter); p != std::string::npos; p = s.find(kPlatzhalter, p))
        {
            std::size_t q = p + kPlatzhalter.size();
            std::size_t n = 0;
            while (q < s.size() && s[q] >= '0' && s[q] <= '9') n = n * 10 + static_cast<std::size_t>(s[q++] - '0');
            if (q >= s.size() || s[q] != '_' || n >= platzhalter.size())
            {
                p = q;
                continue;
            }
            s.replace(p, q + 1 - p, platzhalter[n]);
            p += platzhalter[n].size();
        }
    };
    for (modell::Typ& t : s.einheit.typen)
    {
        t.zeile = zeile(t.zeile);
        for (modell::Feld& f : t.felder)
        {
            f.zeile = zeile(f.zeile);
            zurueck(f.typ);
            zurueck(f.zusatz);
        }
        for (modell::Verweis& v : t.verweise) v.zeile = zeile(v.zeile);
        e.einheit.typen.push_back(std::move(t));
    }
    for (modell::Datenbankobjekt& o : s.einheit.objekte)
    {
        o.zeile = zeile(o.zeile);
        o.bisZeile = zeile(o.bisZeile);
        for (modell::Zugriff& z : o.zugriffe) z.zeile = zeile(z.zeile);
        e.einheit.objekte.push_back(std::move(o));
    }
    for (quelle::Meldung& m : s.meldungen)
    {
        m.ort = ort(m.ort.versatz);
        zurueck(m.text);
        e.meldungen.push_back(std::move(m));
    }
}

sprache::Ergebnis lies(const sprache::Sprache& sprache, const std::string& text, const std::string& name,
                       const std::vector<sprache::Definition>& definitionen, const ProjektOptionen& optionen,
                       const std::string& sqlDialekt = {}, const std::vector<sprache::Abschnitt>& abschnitte = {})
{
    sprache::LeseOptionen lesen;
    lesen.abschnitte = abschnitte;
    lesen.alleZweige = optionen.alleZweige;
    lesen.varianten = optionen.varianten;
    lesen.bereiche = optionen.bereiche;
    lesen.definitionen = definitionen;
    lesen.dialekt = sqlDialekt;
    fuegeHinzu(lesen.definitionen, optionen.definitionen);
    sprache::Ergebnis e = sprache.lies(text, name, lesen);
    if (&sprache != &sprache::sql()) lieseingebettetesSql(e, text, name, sqlDialekt);
    return e;
}

/// Die Zelle, in der die Zeile liegt: die letzte, deren Kopf (sonst erste Zeile) nicht dahinter liegt.
std::size_t zelleIn(const std::vector<Zelle>& zellen, int zeile)
{
    if (zellen.empty()) return kKeine;
    std::size_t aus = 0;
    for (std::size_t i = 0; i < zellen.size(); ++i)
        if ((zellen[i].kopf > 0 ? zellen[i].kopf : zellen[i].zeile) <= zeile) aus = i;
    return aus;
}

/// Eine Stelle im Text einer fremden Zelle als Stelle im Skript: dieselbe Zeile, die Spalte um das
/// verschoben, was dort im Skript davor steht (`-- MAGIC `).
quelle::Position imSkript(const quelle::Position& p, const FremdeZelle& f, const Zeilen& fremd, const Zeilen& skript)
{
    const std::size_t spalte = p.versatz - std::min(p.versatz, fremd.anfang(p.zeile));
    const auto k = static_cast<std::size_t>(std::max(p.zeile, 1) - 1);
    const std::size_t abzug = k < f.abzug.size() ? f.abzug[k] : 0;
    return skript.ort(skript.anfang(p.zeile) + abzug + spalte);
}

/// Haengt an, was eine fremde Zelle gibt (Entscheid N3, wie E1): ihre Funktionen, Typen, Objekte …
void fuegeEin(modell::Einheit& in, modell::Einheit&& von)
{
    const auto haenge = [](auto& ziel, auto& quelle) {
        ziel.insert(ziel.end(), std::make_move_iterator(quelle.begin()), std::make_move_iterator(quelle.end()));
    };
    haenge(in.einbindungen, von.einbindungen);
    haenge(in.makros, von.makros);
    haenge(in.aufhebungen, von.aufhebungen);
    haenge(in.typen, von.typen);
    haenge(in.aliase, von.aliase);
    haenge(in.variablen, von.variablen);
    haenge(in.funktionen, von.funktionen);
    haenge(in.objekte, von.objekte);
    haenge(in.zugriffe, von.zugriffe);
    haenge(in.abfragen, von.abfragen);
}

/// In einem Notebook gehoert, was SQL ausserhalb einer Funktion liest, schreibt, ruft, der Funktion seiner
/// Zelle (Entscheide N3, A4) — `<cell n>`, ohne Rumpf, wenn die Zelle keinen Code einer Sprache mit Ablauf
/// traegt (ihr SQL zeigt der Ablauf erst mit S2); eine Abfrage auf oberster Ebene heisst nach ihr.
/// Die Funktionen stehen danach in der Reihenfolge des Texts.
void ordneZellenZu(modell::Einheit& e, const std::vector<Zelle>& zellen)
{
    const auto funktionDer = [&](std::size_t zelle) -> modell::Funktion& {
        const std::string name = zellenName(zelle + 1);
        for (modell::Funktion& f : e.funktionen)
            if (f.name == name && f.bereich.empty()) return f;
        modell::Funktion f;
        f.name = name;
        f.zeile = zellen[zelle].zeile;
        f.bisZeile = 0;  // ohne Rumpf
        e.funktionen.push_back(std::move(f));
        return e.funktionen.back();
    };
    for (modell::Zugriff& z : e.zugriffe)
    {
        const std::size_t zelle = zelleIn(zellen, z.zeile);
        modell::Funktion& f = funktionDer(zelle);
        const bool schon = std::any_of(f.zugriffe.begin(), f.zugriffe.end(), [&](const modell::Zugriff& x) {
            return x.art == z.art && x.ziel == z.ziel && x.imAusdruck == z.imAusdruck;
        });
        if (!schon) f.zugriffe.push_back(std::move(z));
    }
    e.zugriffe.clear();
    constexpr std::string_view kOben = "query@";
    for (modell::Abfrage& q : e.abfragen)
    {
        q.zugriffe.clear();  // sie traegt die Zelle (N3), die Anweisung ist kein eigener Knoten
        if (q.name.starts_with(kOben))
            q.name = zellenName(zelleIn(zellen, q.zeile) + 1) + "@" + q.name.substr(kOben.size());
    }
    std::stable_sort(e.funktionen.begin(), e.funktionen.end(),
                     [](const modell::Funktion& a, const modell::Funktion& b) { return a.zeile < b.zeile; });
    std::stable_sort(e.abfragen.begin(), e.abfragen.end(),
                     [](const modell::Abfrage& a, const modell::Abfrage& b) { return a.zeile < b.zeile; });
    std::stable_sort(e.typen.begin(), e.typen.end(),
                     [](const modell::Typ& a, const modell::Typ& b) { return a.zeile < b.zeile; });
}

/// Die `explain()` in einer Anweisung und allem darunter: je eins seine Zeile.
void sammleExplain(const modell::Anweisung& a, std::vector<int>& zeilen)
{
    for (const modell::Aufrufkette& k : a.ketten)
        for (const modell::Kettenglied& g : k.glieder)
            if (g.aufruf && g.name == "explain")
            {
                zeilen.push_back(k.zeile > 0 ? k.zeile : a.zeile);
                break;
            }
    for (const modell::AnweisungPtr& k : a.kinder)
        if (k) sammleExplain(*k, zeilen);
    if (a.sonst) sammleExplain(*a.sonst, zeilen);
}

/// Die Plaene in den Ausgaben eines Notebooks (Entscheid P2-3): je Zelle der Reihe nach zu ihren `explain()`,
/// benannt `funktion@zeile` nach dem explain (P2-4). Ungleiche Zahl: ein Hinweis; ein Plan ohne explain heisst
/// `<cell n>@output:m` (m: seine Zeile in der Ausgabe). Meldungen ueber den Text einer Ausgabe stehen am Kopf der
/// Zelle.
void ordnePlaeneZu(sprache::Ergebnis& e, const Notebook& notebook, std::string_view text)
{
    if (notebook.ausgaben.empty()) return;
    struct Explain
    {
        int zeile;
        std::string funktion;
    };
    std::vector<Explain> explains;
    for (const modell::Funktion& f : e.einheit.funktionen)
    {
        if (!f.rumpf) continue;
        std::vector<int> zeilen;
        sammleExplain(*f.rumpf, zeilen);
        for (const int z : zeilen) explains.push_back({z, f.vollerName()});
    }
    std::sort(explains.begin(), explains.end(), [](const Explain& a, const Explain& b) { return a.zeile < b.zeile; });
    const Zeilen zeilen(text);
    for (const ZellAusgabe& a : notebook.ausgaben)
    {
        const Zelle& z = notebook.zellen[a.zelle];
        const quelle::Position kopf = zeilen.ort(zeilen.anfang(z.kopf > 0 ? z.kopf : z.zeile));
        std::vector<Meldung> meldungen;
        std::vector<modell::Plan> plaene = sprache::liesPlaene(a.text, meldungen);
        for (Meldung& m : meldungen)
            e.meldungen.push_back({m.schwere, kopf, "output, line " + std::to_string(m.ort.zeile) + ": " + m.text});
        if (plaene.empty()) continue;
        std::vector<const Explain*> hier;
        for (const Explain& x : explains)
            if (x.zeile >= z.zeile && x.zeile <= z.bisZeile) hier.push_back(&x);
        if (hier.size() != plaene.size())
            e.meldungen.push_back({Schwere::Hinweis, kopf,
                                   std::to_string(plaene.size()) + " plan(s) in the output, " + std::to_string(hier.size()) +
                                       " explain() in the cell - matched in order"});
        for (std::size_t i = 0; i < plaene.size(); ++i)
        {
            modell::Plan& p = plaene[i];
            if (i < hier.size())
            {
                p.name = hier[i]->funktion + "@" + std::to_string(hier[i]->zeile);
                p.aufruf = hier[i]->zeile;
            }
            else
            {
                p.name = zellenName(a.zelle + 1) + "@output:" + std::to_string(p.zeile);
                p.aufruf = z.zeile;
            }
            e.einheit.plaene.push_back(std::move(p));
        }
    }
}

/// Liest eine Datei: ein Notebook (`notebook`) in seiner Sprache und je fremde Zelle in deren (N3).
sprache::Ergebnis liesEinheit(const Datei& d, const Notebook* notebook, const std::string& name,
                              const std::vector<sprache::Definition>& definitionen, const ProjektOptionen& optionen,
                              const std::string& sqlDialekt)
{
    if (notebook == nullptr) return lies(*d.sprache, d.text, name, definitionen, optionen, sqlDialekt);
    // der Dialekt des SQL (E2, S3): der der Projektdatei, sonst der Kopf (Databricks: spark), sonst fuer die
    // Zellen in SQL der, den der Code des Notebooks zeigt (`import sqlite3`)
    std::string dialekt = sqlDialekt.empty() && notebook->databricks ? std::string("spark") : sqlDialekt;
    sprache::Ergebnis e =
        lies(*d.sprache, notebook->lesetext, name, definitionen, optionen, dialekt, notebook->abschnitte);
    if (dialekt.empty())
        if (const auto host = dialekteVonHost(e); host.size() == 1) dialekt = std::string(host.front().first);
    const Zeilen skript(d.text);
    for (const FremdeZelle& f : notebook->fremde)
    {
        const Zelle& z = notebook->zellen[f.zelle];
        sprache::Ergebnis x = lies(*f.sprache, f.text, name, definitionen, optionen, dialekt,
                                   {{zellenName(f.zelle + 1), z.zeile, z.bisZeile}});
        const Zeilen fremd(f.text);
        for (Meldung& m : x.meldungen)
        {
            m.ort = imSkript(m.ort, f, fremd, skript);
            e.meldungen.push_back(std::move(m));
        }
        fuegeEin(e.einheit, std::move(x.einheit));
    }
    e.meldungen.insert(e.meldungen.end(), notebook->meldungen.begin(), notebook->meldungen.end());
    std::stable_sort(e.meldungen.begin(), e.meldungen.end(),
                     [](const Meldung& a, const Meldung& b) { return a.ort.versatz < b.ort.versatz; });
    ordneZellenZu(e.einheit, notebook->zellen);
    ordnePlaeneZu(e, *notebook, d.text);
    std::stable_sort(e.meldungen.begin(), e.meldungen.end(),
                     [](const Meldung& a, const Meldung& b) { return a.ort.versatz < b.ort.versatz; });
    return e;
}

/// Macht aus der Datei ein Notebook, wenn sie eins ist: ein `.ipynb` wird sein Skript (N1) und bekommt
/// seine Sprache; ein Databricks-Notebook bleibt, wie es ist. false = ein `.ipynb`, das sich nicht lesen
/// laesst (`fehler`).
bool bereiteNotebook(Datei& d, std::optional<Notebook>& notebook, Meldung& fehler)
{
    if (istNotebook(d.pfad))
    {
        notebook = ausIpynb(d.text, fehler);
        if (!notebook) return false;
        d.text = notebook->skript;
        d.sprache = notebook->sprache;
    }
    else if (d.sprache != nullptr)
        notebook = ausDatabricks(d.text, *d.sprache);
    if (notebook) d.zellen = notebook->zellen;
    return true;
}

/// Die Include-Pfade fuer eine Datei: die des Projekts, dann die der passenden Abschnitte.
std::vector<std::string> includePfadeFuer(const Projektdatei& einstellungen, std::string_view pfad)
{
    std::vector<std::string> aus = einstellungen.includePfade;
    const std::string ordner = ordnerVon(pfad);
    for (const Abschnitt& a : einstellungen.abschnitte)
        if (liegtIn(a.ordner, ordner)) aus.insert(aus.end(), a.includePfade.begin(), a.includePfade.end());
    return aus;
}

/// Liest ein Projekt; haelt, was beim Suchen der Koepfe gebraucht wird.
class Einleser
{
public:
    Einleser(Projekt& projekt, const ProjektOptionen& optionen) : m_p(projekt), m_optionen(optionen) {}

    void durchsuche(const fs::path& ordner, const std::string& relativ)
    {
        std::error_code fehler;
        std::vector<fs::directory_entry> eintraege;
        for (fs::directory_iterator it(oeffenbar(ordner), fs::directory_options::skip_permission_denied, fehler), ende;
             !fehler && it != ende; it.increment(fehler))
            eintraege.push_back(*it);
        if (fehler)
        {
            projektMeldung(relativ, Schwere::Warnung, "cannot read the folder: " + fehler.message());
            return;
        }
        std::sort(eintraege.begin(), eintraege.end(),
                  [](const fs::directory_entry& a, const fs::directory_entry& b) { return a.path() < b.path(); });
        for (const fs::directory_entry& e : eintraege)
        {
            const std::string name = alsText(e.path().filename());
            const std::string weg = relativ.empty() ? name : relativ + "/" + name;
            std::error_code art;
            if (e.is_symlink(art))
            {
                if (e.is_directory(art)) projektMeldung(weg, Schwere::Hinweis, "folder skipped: a link, not followed");
                continue;
            }
            if (e.is_directory(art))
            {
                if (wirdImmerUebergangen(name))
                {
                    uebergehe(weg, "always skipped");
                    continue;
                }
                const auto erzeugt = std::find_if(erzeugteOrdner().begin(), erzeugteOrdner().end(), [&](const ErzeugterOrdner& o) {
                    std::error_code fehler;
                    return o.name == name && fs::is_regular_file(oeffenbar(ordner / fs::path(std::string(o.kennzeichen))), fehler);
                });
                if (erzeugt != erzeugteOrdner().end())
                {
                    uebergehe(weg, "made by " + std::string(erzeugt->werkzeug));
                    continue;
                }
                const auto muster = std::find_if(m_p.einstellungen.uebergehen.begin(), m_p.einstellungen.uebergehen.end(),
                                                 [&](const std::string& m) { return passt(m, weg); });
                if (muster != m_p.einstellungen.uebergehen.end())
                {
                    uebergehe(weg, "\"uebergehen\": \"" + *muster + "\"");
                    continue;
                }
                // ein Event-Log mit `spark.eventLog.rolling.enabled`: ein Ordner, ein Lauf (L1)
                if (name.starts_with("eventlog_v2_"))
                {
                    liesRollendenLog(e.path(), weg);
                    continue;
                }
                durchsuche(e.path(), weg);
                continue;
            }
            if (!e.is_regular_file(art)) continue;
            if (komprimierterLog(name))
            {
                projektMeldung(weg, Schwere::Hinweis, "compressed event log - not read (spark.eventLog.compress=false)");
                continue;
            }
            // ohne Endung: ein Event-Log, wenn er heisst, wie Spark Logs benennt, und so beginnt (L1, L1-N) — sonst nichts,
            // was das Projekt liest (ein Maildir hat Hunderttausende solcher Dateien)
            if (ohneEndung(name))
            {
                if (!logName(name) || !sprache::eventLogText(anfangVon(e.path(), 256))) continue;
                Datei d;
                d.pfad = weg;
                if (!liesDatei(e.path(), d.text))
                {
                    projektMeldung(weg, Schwere::Fehler, "cannot read the file");
                    continue;
                }
                d.sprache = &sprache::sparkEventLog();
                m_p.dateien.push_back(std::move(d));
                continue;
            }
            if (!kannLesen(name)) continue;
            Datei d;
            d.pfad = weg;
            if (!liesDatei(e.path(), d.text))
            {
                projektMeldung(weg, Schwere::Fehler, "cannot read the file");
                continue;
            }
            d.sprache = sprache::spracheFuer(name, d.text);  // ein Notebook bekommt sie in liesAlle
            // eine Textdatei ist nur ein Plan von Spark, wenn einer darin steht (Entscheid P2-3); ein `.inprogress` nur
            // ein Event-Log, wenn er so beginnt (L1)
            if (d.sprache == &sprache::sparkPlan() && !sprache::plantext(d.text)) continue;
            if (d.sprache == &sprache::sparkEventLog() && !sprache::eventLogText(d.text)) continue;
            m_p.dateien.push_back(std::move(d));
        }
    }

    /// Ein rollender Event-Log: seine `events_<n>_…` der Nummer nach aneinander, als eine Datei (L1). Ein
    /// `appstatus_….inprogress` daneben heisst: der Lauf schreibt noch.
    void liesRollendenLog(const fs::path& ordner, const std::string& weg)
    {
        std::error_code fehler;
        std::vector<std::pair<long long, fs::path>> teile;
        bool laeuft = false;
        for (fs::directory_iterator it(oeffenbar(ordner), fs::directory_options::skip_permission_denied, fehler), ende;
             !fehler && it != ende; it.increment(fehler))
        {
            const std::string n = alsText(it->path().filename());
            if (n.starts_with("appstatus_") && n.ends_with(".inprogress")) laeuft = true;
            if (!n.starts_with("events_")) continue;
            if (komprimierterLog(n))
            {
                projektMeldung(weg + "/" + n, Schwere::Hinweis, "compressed event log - not read (spark.eventLog.compress=false)");
                continue;
            }
            const std::size_t strich = n.find('_', 7);
            long long nummer = 0;
            try
            {
                nummer = std::stoll(n.substr(7, strich == std::string::npos ? std::string::npos : strich - 7));
            }
            catch (...)
            {
            }
            teile.emplace_back(nummer, it->path());
        }
        if (teile.empty()) return;
        std::sort(teile.begin(), teile.end());
        Datei d;
        d.pfad = laeuft ? weg + ".inprogress" : weg;
        for (const auto& [nummer, teil] : teile)
        {
            std::string text;
            if (!liesDatei(teil, text))
            {
                projektMeldung(weg, Schwere::Fehler, "cannot read " + alsText(teil.filename()));
                continue;
            }
            if (!d.text.empty() && d.text.back() != '\n') d.text.push_back('\n');
            d.text += text;
        }
        if (!sprache::eventLogText(d.text)) return;
        d.sprache = &sprache::sparkEventLog();
        m_p.dateien.push_back(std::move(d));
    }

    void liesAlle()
    {
        std::sort(m_p.dateien.begin(), m_p.dateien.end(), [](const Datei& a, const Datei& b) { return a.pfad < b.pfad; });
        // 0. Notebooks (Entscheide N1–N3); ein `.ipynb`, das sich nicht lesen laesst, faellt mit Meldung heraus
        std::vector<Datei> dateien;
        for (Datei& d : m_p.dateien)
        {
            std::optional<Notebook> notebook;
            Meldung fehler;
            if (!bereiteNotebook(d, notebook, fehler))
            {
                m_p.meldungen.push_back({d.pfad, std::move(fehler)});
                continue;
            }
            dateien.push_back(std::move(d));
            m_notebooks.push_back(std::move(notebook));
        }
        m_p.dateien = std::move(dateien);
        for (std::size_t i = 0; i < m_p.dateien.size(); ++i)
        {
            m_index[klein(m_p.dateien[i].pfad)] = i;
            m_namen[klein(nameVon(m_p.dateien[i].pfad))].push_back(i);
        }

        // 1. jede Datei mit den Definitionen von Projekt und Abschnitten
        for (std::size_t i = 0; i < m_p.dateien.size(); ++i)
        {
            Datei& d = m_p.dateien[i];
            d.definitionen = definitionenFuer(m_p.einstellungen, d.pfad);
            d.ergebnis = liesEinheit(d, notebook(i), nameVon(d.pfad), d.definitionen, m_optionen, m_p.einstellungen.sql);
        }
        // 2. dazu die #define der eingebundenen Koepfe — aus dem ersten Durchgang
        std::vector<std::vector<sprache::Definition>> ausKoepfen(m_p.dateien.size());
        // (Koepfe bindet nur `#include` ein; import und using loest das Projekt noch nicht auf)
        for (std::size_t i = 0; i < m_p.dateien.size(); ++i)
            if (m_p.dateien[i].sprache->bindetKoepfeEin()) ausKoepfen[i] = sammleKoepfe(i, m_p.dateien[i].koepfe);
        for (std::size_t i = 0; i < m_p.dateien.size(); ++i)
        {
            if (ausKoepfen[i].empty()) continue;
            Datei& d = m_p.dateien[i];
            fuegeHinzu(d.definitionen, ausKoepfen[i]);
            d.ergebnis = liesEinheit(d, notebook(i), nameVon(d.pfad), d.definitionen, m_optionen, m_p.einstellungen.sql);
        }
    }

private:
    [[nodiscard]] const Notebook* notebook(std::size_t datei) const
    {
        return datei < m_notebooks.size() && m_notebooks[datei] ? &*m_notebooks[datei] : nullptr;
    }

    /// Ein Kopf: eine Datei des Projekts oder eine draussen (ueber einen Include-Pfad gefunden).
    struct Kopf
    {
        std::size_t datei = kKeine;
        std::string draussen;  ///< absoluter Weg, wenn er nicht zum Projekt gehoert
        [[nodiscard]] bool gefunden() const { return datei != kKeine || !draussen.empty(); }
        [[nodiscard]] std::string schluessel() const { return datei != kKeine ? std::to_string(datei) : draussen; }
    };

    /// Was ein Kopf draussen beitraegt: seine Einbindungen und Makros, einmal gelesen.
    struct Draussen
    {
        std::vector<modell::Einbindung> einbindungen;
        std::vector<modell::Makro> makros;
    };

    void projektMeldung(const std::string& pfad, Schwere schwere, std::string text)
    {
        m_p.meldungen.push_back({pfad, {schwere, kOhneZeile, std::move(text)}});
    }

    void uebergehe(const std::string& weg, const std::string& grund)
    {
        m_p.uebergangen.push_back(weg);
        projektMeldung(weg, Schwere::Hinweis, "folder skipped (" + grund + ")");
    }

    /// Ein Weg auf der Platte als Kopf: im Projekt, wenn er unter der Wurzel liegt und eingelesen ist.
    Kopf alsKopf(const fs::path& weg)
    {
        std::error_code fehler;
        if (!fs::is_regular_file(weg, fehler)) return {};
        const fs::path normal = weg.lexically_normal();
        const fs::path relativ = normal.lexically_relative(m_p.wurzel);
        const std::string rel = alsText(relativ);
        if (!relativ.empty() && !rel.starts_with(".."))
            if (const auto it = m_index.find(klein(rel)); it != m_index.end()) return {it->second, {}};
        return {kKeine, alsText(normal)};
    }

    /// Wo ein `#include` hinfuehrt; `ordner` ist der Ordner der einbindenden Datei (absolut).
    Kopf suche(const modell::Einbindung& e, const fs::path& ordner, const std::vector<std::string>& includePfade)
    {
        if (!e.system)
            if (Kopf k = alsKopf(ordner / alsWeg(e.ziel)); k.gefunden()) return k;
        for (const std::string& ip : includePfade)
        {
            const fs::path basis = alsWeg(ip).is_absolute() ? alsWeg(ip) : m_p.wurzel / alsWeg(ip);
            if (Kopf k = alsKopf(basis / alsWeg(e.ziel)); k.gefunden()) return k;
        }
        if (e.system) return {};
        // zuletzt: eine Datei dieses Namens, wenn es im Projekt genau eine gibt
        if (const auto it = m_namen.find(klein(nameVon(e.ziel))); it != m_namen.end() && it->second.size() == 1)
            return {it->second.front(), {}};
        return {};
    }

    const Draussen& draussen(const std::string& weg, const std::vector<sprache::Definition>& definitionen)
    {
        if (const auto it = m_draussen.find(weg); it != m_draussen.end()) return it->second;
        Draussen& aus = m_draussen[weg];
        std::string text;
        if (!liesDatei(alsWeg(weg), text)) return aus;
        const sprache::Sprache* s = sprache::spracheFuer(nameVon(weg), text);
        if (s == nullptr) s = &sprache::c();
        sprache::Ergebnis e = lies(*s, text, nameVon(weg), definitionen, m_optionen);
        aus.einbindungen = std::move(e.einheit.einbindungen);
        aus.makros = std::move(e.einheit.makros);
        return aus;
    }

    /// Die #define aller Koepfe, die Datei `i` einbindet, auch mittelbar: zuerst
    /// die eines Kopfs, den ein Kopf einbindet, dann dessen eigene. `koepfe`: die Koepfe
    /// im Projekt, in der Reihenfolge, in der sie gefunden werden.
    std::vector<sprache::Definition> sammleKoepfe(std::size_t i, std::vector<std::size_t>& koepfe)
    {
        const Datei& d = m_p.dateien[i];
        const std::vector<std::string> includePfade = includePfadeFuer(m_p.einstellungen, d.pfad);
        std::vector<sprache::Definition> gefunden;  // alle #define in der Reihenfolge, gleichnamige mehrfach
        std::set<std::string> besucht{std::to_string(i)};
        const auto folge = [&](auto& selbst, const std::vector<modell::Einbindung>& einbindungen, const fs::path& ordner,
                               const std::string& wer) -> void {
            for (const modell::Einbindung& e : einbindungen)
            {
                const Kopf k = suche(e, ordner, includePfade);
                if (!k.gefunden())
                {
                    if (!e.system && m_gemeldet.insert(wer + "\n" + e.ziel).second)
                        m_p.meldungen.push_back({wer,
                                                 {Schwere::Hinweis, {0, e.zeile, 1},
                                                  "header \"" + e.ziel +
                                                      "\" not found (next to the file, in the include paths, by name in the project)"}});
                    continue;
                }
                if (!besucht.insert(k.schluessel()).second) continue;
                if (k.datei != kKeine)
                {
                    koepfe.push_back(k.datei);
                    const Datei& kopf = m_p.dateien[k.datei];
                    selbst(selbst, kopf.ergebnis.einheit.einbindungen, m_p.wurzel / alsWeg(ordnerVon(kopf.pfad)), kopf.pfad);
                    for (const modell::Makro& m : kopf.ergebnis.einheit.makros) gefunden.push_back(alsDefinition(m));
                }
                else
                {
                    const Draussen& kopf = draussen(k.draussen, m_p.einstellungen.definitionen);
                    selbst(selbst, kopf.einbindungen, alsWeg(k.draussen).parent_path(), k.draussen);
                    for (const modell::Makro& m : kopf.makros) gefunden.push_back(alsDefinition(m));
                }
            }
        };
        folge(folge, d.ergebnis.einheit.einbindungen, m_p.wurzel / alsWeg(ordnerVon(d.pfad)), d.pfad);
        std::vector<sprache::Definition> aus;
        fuegeHinzu(aus, gefunden);  // je Name einmal, der spaetere gilt
        return aus;
    }

    Projekt& m_p;
    const ProjektOptionen& m_optionen;
    std::map<std::string, std::size_t> m_index;                // klein(pfad) -> Datei
    std::map<std::string, std::vector<std::size_t>> m_namen;   // klein(Dateiname) -> Dateien
    std::map<std::string, Draussen> m_draussen;
    std::set<std::string> m_gemeldet;
    std::vector<std::optional<Notebook>> m_notebooks;  ///< je Datei, nach liesAlle Schritt 0
};

}  // namespace

bool liesDatei(const fs::path& weg, std::string& text)
{
    std::ifstream strom(oeffenbar(weg), std::ios::binary);
    if (!strom) return false;
    std::ostringstream inhalt;
    inhalt << strom.rdbuf();
    text = inhalt.str();
    return true;
}

std::size_t Projekt::finde(std::string_view pfad) const
{
    for (std::size_t i = 0; i < dateien.size(); ++i)
        if (dateien[i].pfad == pfad) return i;
    return kKeine;
}

std::size_t Projekt::funktionen() const
{
    std::size_t n = 0;
    for (const Datei& d : dateien)
        for (const modell::Funktion& f : d.ergebnis.einheit.funktionen)
            if (f.rumpf) ++n;
    return n;
}

std::size_t Projekt::typen() const
{
    std::size_t n = 0;
    for (const Datei& d : dateien) n += d.ergebnis.einheit.typen.size();
    return n;
}

std::vector<sprache::Definition> definitionenFuer(const Projektdatei& einstellungen, std::string_view pfad)
{
    std::vector<sprache::Definition> aus;
    fuegeHinzu(aus, einstellungen.definitionen);  // je Name einmal, auch wo die Datei ihn zweimal nennt
    const std::string ordner = ordnerVon(pfad);
    for (const Abschnitt& a : einstellungen.abschnitte)
        if (liegtIn(a.ordner, ordner)) fuegeHinzu(aus, a.definitionen);
    return aus;
}

Herkunft herkunft(const Projektdatei& einstellungen, const Datei& datei, std::string_view name)
{
    const auto gleich = [&](const sprache::Definition& x) { return x.name == name; };
    const auto gelesen = std::find_if(datei.definitionen.begin(), datei.definitionen.end(), gleich);
    if (gelesen == datei.definitionen.end()) return Herkunft::keine;
    // ohne die Koepfe anders (oder gar nicht): ein Kopf hat ihn festgelegt
    const std::vector<sprache::Definition> eigene = definitionenFuer(einstellungen, datei.pfad);
    const auto ohneKoepfe = std::find_if(eigene.begin(), eigene.end(), gleich);
    if (ohneKoepfe == eigene.end() || sprache::alsText(*ohneKoepfe) != sprache::alsText(*gelesen)) return Herkunft::kopf;
    const std::string ordner = ordnerVon(datei.pfad);
    for (const Abschnitt& a : einstellungen.abschnitte)
        if (liegtIn(a.ordner, ordner) && std::any_of(a.definitionen.begin(), a.definitionen.end(), gleich))
            return Herkunft::abschnitt;
    return Herkunft::projekt;
}

Projekt liesOrdner(const fs::path& wurzel, const ProjektOptionen& optionen)
{
    Projekt p;
    std::error_code fehler;
    p.wurzel = fs::absolute(wurzel, fehler).lexically_normal();
    if (fehler || !fs::is_directory(p.wurzel, fehler))
    {
        p.meldungen.push_back({{}, {Schwere::Fehler, kOhneZeile, "not a folder: " + alsText(wurzel)}});
        return p;
    }
    std::string text;
    if (liesDatei(p.wurzel / alsWeg(kProjektdatei), text))
    {
        p.mitProjektdatei = true;
        std::vector<Meldung> meldungen;
        p.einstellungen = liesProjektdatei(text, meldungen);
        for (Meldung& m : meldungen) p.meldungen.push_back({std::string(kProjektdatei), std::move(m)});
    }
    Einleser einleser(p, optionen);
    einleser.durchsuche(p.wurzel, {});
    einleser.liesAlle();
    return p;
}

Projekt liesText(std::string_view text, std::string_view name, const ProjektOptionen& optionen)
{
    Projekt p;
    Datei d;
    d.pfad = std::string(name);
    d.text = std::string(text);
    d.sprache = sprache::spracheFuer(name, text);
    if (d.sprache == nullptr && sprache::eventLogText(text)) d.sprache = &sprache::sparkEventLog();  // ohne Endung (L1)
    if (d.sprache == nullptr && !istNotebook(name)) d.sprache = &sprache::c();
    std::optional<Notebook> notebook;
    Meldung fehler;
    if (!bereiteNotebook(d, notebook, fehler))
    {
        p.meldungen.push_back({d.pfad, std::move(fehler)});
        return p;
    }
    d.ergebnis = liesEinheit(d, notebook ? &*notebook : nullptr, d.pfad, {}, optionen, {});
    p.dateien.push_back(std::move(d));
    return p;
}

bool istNotebook(std::string_view dateiname)
{
    const std::size_t punkt = dateiname.rfind('.');
    return punkt != std::string_view::npos && klein(dateiname.substr(punkt + 1)) == "ipynb";
}

bool kannLesen(std::string_view dateiname)
{
    return istNotebook(dateiname) || sprache::spracheFuer(dateiname) != nullptr;
}

std::size_t zelleAn(const Datei& datei, int zeile)
{
    return zelleIn(datei.zellen, zeile);
}

std::string stelleIn(const Datei& datei, int zeile)
{
    const std::size_t i = zelleAn(datei, zeile);
    if (i == kKeine) return "line " + std::to_string(zeile);
    const Zelle& z = datei.zellen[i];
    const std::string zelle = "cell " + std::to_string(i + 1);
    if (zeile < z.zeile) return zelle;
    return zelle + ", line " + std::to_string(zeile - z.zeile + 1);
}

std::vector<Fundstelle> kandidaten(const Projekt& projekt, std::size_t ausDatei, std::string_view aufruf)
{
    std::vector<Fundstelle> aus;
    const auto sammle = [&](std::size_t d, bool nurOeffentliche) {
        const std::vector<modell::Funktion>& fs = projekt.dateien[d].ergebnis.einheit.funktionen;
        for (std::size_t f = 0; f < fs.size(); ++f)
            if (fs[f].rumpf && fs[f].heisst(aufruf) && !(nurOeffentliche && fs[f].intern)) aus.push_back({d, f});
    };
    if (ausDatei < projekt.dateien.size())
    {
        sammle(ausDatei, false);
        if (!aus.empty()) return aus;
    }
    for (std::size_t d = 0; d < projekt.dateien.size(); ++d)
        if (d != ausDatei) sammle(d, true);
    return aus;
}

std::vector<Fundstelle> aufrufer(const Projekt& projekt, const Fundstelle& ziel)
{
    std::vector<Fundstelle> aus;
    if (ziel.datei >= projekt.dateien.size()) return aus;
    const modell::Funktion& gerufen = funktion(projekt, ziel);
    for (std::size_t d = 0; d < projekt.dateien.size(); ++d)
    {
        const std::vector<modell::Funktion>& fs = projekt.dateien[d].ergebnis.einheit.funktionen;
        for (std::size_t f = 0; f < fs.size(); ++f)
        {
            if (!fs[f].rumpf) continue;
            for (const std::string& aufruf : modell::aufrufeVon(fs[f]))
            {
                // erst der billige Vergleich am Namen, dann die Regeln des Projekts
                if (!gerufen.heisst(aufruf)) continue;
                const std::vector<Fundstelle> gemeint = kandidaten(projekt, d, aufruf);
                if (std::any_of(gemeint.begin(), gemeint.end(), [&](const Fundstelle& g) { return g == ziel; }))
                {
                    aus.push_back({d, f});
                    break;
                }
            }
        }
    }
    return aus;
}

std::vector<Fundstelle> funktionenNamens(const Projekt& projekt, std::string_view name)
{
    std::vector<Fundstelle> aus;
    for (std::size_t d = 0; d < projekt.dateien.size(); ++d)
    {
        const std::vector<modell::Funktion>& fs = projekt.dateien[d].ergebnis.einheit.funktionen;
        for (std::size_t f = 0; f < fs.size(); ++f)
            if (fs[f].rumpf && (fs[f].name == name || fs[f].vollerName() == name)) aus.push_back({d, f});
    }
    return aus;
}

const modell::Funktion& funktion(const Projekt& projekt, const Fundstelle& stelle)
{
    return projekt.dateien[stelle.datei].ergebnis.einheit.funktionen[stelle.funktion];
}

}  // namespace c2d::projekt
