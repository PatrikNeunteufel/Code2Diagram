/**
 * @file Projekt.cpp
 * @brief Ein Projekt einlesen: Ordner durchsuchen, zweimal lesen, Aufrufe aufloesen
 */
#include "c2d/projekt/Projekt.hpp"

#include <algorithm>
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

/// Die Stelle (Zeile, Spalte) eines Versatzes in einem Text.
class Zeilen
{
public:
    explicit Zeilen(std::string_view text)
    {
        m_anfang.push_back(0);
        for (std::size_t i = 0; i < text.size(); ++i)
            if (text[i] == '\n') m_anfang.push_back(i + 1);
    }

    [[nodiscard]] quelle::Position ort(std::size_t versatz) const
    {
        const auto z = std::upper_bound(m_anfang.begin(), m_anfang.end(), versatz) - 1;
        return {versatz, static_cast<int>(z - m_anfang.begin()) + 1, static_cast<int>(versatz - *z) + 1};
    }

    /// Wo Zeile z (ab 1) beginnt.
    [[nodiscard]] std::size_t anfang(int z) const
    {
        return m_anfang[std::clamp<std::size_t>(static_cast<std::size_t>(std::max(z, 1)) - 1, 0, m_anfang.size() - 1)];
    }

private:
    std::vector<std::size_t> m_anfang;
};

/// SQL in den Zeichenketten einer Datei (Entscheide S1, E1 a): jede, die mit einer Anweisung des Schemas beginnt
/// (sprache::beginntMitSchema), wird gelesen — alle einer Datei als ein Text, damit ein Fremdschluessel seine
/// Tabelle in einer anderen Zeichenkette findet; ihre Tabellen kommen in die Einheit der Datei, ihre
/// Stellen und Meldungen auf die Stellen der Quelle umgerechnet. Der Dialekt: der der Projektdatei, sonst
/// der, den der Host-Code zeigt. Eine zusammengesetzte Zeichenkette bleibt aussen vor (Hinweis).
void lieseingebettetesSql(sprache::Ergebnis& e, const std::string& text, const std::string& name,
                          const std::string& dialekt)
{
    std::string sql;
    std::vector<std::size_t> herkunft;  // je Byte von `sql` der Versatz in `text`
    const Zeilen quelle(text);
    for (const sprache::Zeichenkette& z : e.zeichenketten)
    {
        // heute nur das Schema (CREATE, ALTER, DROP): aus Abfragen leitet noch nichts ab (Entscheid S2)
        // eine zusammengesetzte traegt nur ihren Anfang (`"CREATE TABLE " + name`)
        if (z.inhalt.empty() || !sprache::beginntMitSchema(z.inhalt, z.zusammengesetzt)) continue;
        if (z.zusammengesetzt)
        {
            e.meldungen.push_back({Schwere::Hinweis, quelle.ort(z.versatz.front()),
                                   "SQL in a composed string (+ or embedded expressions) is not read"});
            continue;
        }
        for (std::size_t j = 0; j < z.inhalt.size(); ++j)
        {
            // zwischen zwei Literalen auf verschiedenen Zeilen (`"CREATE TABLE a ("` / `"  x INT)"`) ein
            // Zeilenwechsel — fuer SQL Leerraum, und jede Zeile des SQL liegt auf einer der Quelle
            if (j > 0 && z.versatz[j] > z.versatz[j - 1] + 1 && z.inhalt[j - 1] != '\n' &&
                text.find('\n', z.versatz[j - 1] + 1) < z.versatz[j])
            {
                sql += '\n';
                herkunft.push_back(z.versatz[j - 1]);
            }
            sql += z.inhalt[j];
            herkunft.push_back(z.versatz[j]);
        }
        sql += ";\n";  // trennt, auch wenn die Zeichenkette kein `;` traegt
        herkunft.insert(herkunft.end(), 2, z.versatz.back());
    }
    if (sql.empty()) return;

    sprache::LeseOptionen lesen;
    lesen.dialekt = dialekt;
    std::string bibliotheken;
    if (lesen.dialekt.empty())
    {
        const auto host = dialekteVonHost(e);
        if (host.size() == 1) lesen.dialekt = std::string(host.front().first);
        for (const auto& [d, woran] : host) bibliotheken += (bibliotheken.empty() ? "" : ", ") + woran;
        if (host.size() > 1)
            e.meldungen.push_back({Schwere::Hinweis, quelle.ort(herkunft.front()),
                                   "several SQL libraries (" + bibliotheken + ") - SQL read as common SQL"});
    }
    sprache::Ergebnis s = sprache::sql().lies(sql, name, lesen);

    const Zeilen inSql(sql);
    const auto ort = [&](std::size_t versatz) { return quelle.ort(herkunft[std::min(versatz, herkunft.size() - 1)]); };
    const auto zeile = [&](int z) { return ort(inSql.anfang(z)).zeile; };
    for (modell::Typ& t : s.einheit.typen)
    {
        t.zeile = zeile(t.zeile);
        for (modell::Feld& f : t.felder) f.zeile = zeile(f.zeile);
        for (modell::Verweis& v : t.verweise) v.zeile = zeile(v.zeile);
        e.einheit.typen.push_back(std::move(t));
    }
    for (quelle::Meldung& m : s.meldungen)
    {
        m.ort = ort(m.ort.versatz);
        e.meldungen.push_back(std::move(m));
    }
}

sprache::Ergebnis lies(const sprache::Sprache& sprache, const std::string& text, const std::string& name,
                       const std::vector<sprache::Definition>& definitionen, const ProjektOptionen& optionen,
                       const std::string& sqlDialekt = {})
{
    sprache::LeseOptionen lesen;
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
                durchsuche(e.path(), weg);
                continue;
            }
            if (!e.is_regular_file(art) || sprache::spracheFuer(name) == nullptr) continue;
            Datei d;
            d.pfad = weg;
            if (!liesDatei(e.path(), d.text))
            {
                projektMeldung(weg, Schwere::Fehler, "cannot read the file");
                continue;
            }
            d.sprache = sprache::spracheFuer(name, d.text);
            m_p.dateien.push_back(std::move(d));
        }
    }

    void liesAlle()
    {
        std::sort(m_p.dateien.begin(), m_p.dateien.end(), [](const Datei& a, const Datei& b) { return a.pfad < b.pfad; });
        for (std::size_t i = 0; i < m_p.dateien.size(); ++i)
        {
            m_index[klein(m_p.dateien[i].pfad)] = i;
            m_namen[klein(nameVon(m_p.dateien[i].pfad))].push_back(i);
        }

        // 1. jede Datei mit den Definitionen von Projekt und Abschnitten
        for (Datei& d : m_p.dateien)
        {
            d.definitionen = definitionenFuer(m_p.einstellungen, d.pfad);
            d.ergebnis = lies(*d.sprache, d.text, nameVon(d.pfad), d.definitionen, m_optionen, m_p.einstellungen.sql);
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
            d.ergebnis = lies(*d.sprache, d.text, nameVon(d.pfad), d.definitionen, m_optionen, m_p.einstellungen.sql);
        }
    }

private:
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
    if (d.sprache == nullptr) d.sprache = &sprache::c();
    d.ergebnis = lies(*d.sprache, d.text, d.pfad, {}, optionen);
    p.dateien.push_back(std::move(d));
    return p;
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
