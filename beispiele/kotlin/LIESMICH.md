# Kotlin-Musterprojekte

Zwei kleine Android-Apps, von Claude geschrieben (Entscheid K5, 2026-10-18): Es gab keinen fremden
Kotlin-Code als Probe. Sie sind die Probe für den Kotlin-Leser, bis es echten gibt — und prüfen damit
nur, woran beim Schreiben gedacht wurde.

**Nicht übersetzt:** auf dieser Maschine gibt es weder `kotlinc` noch Gradle oder das Android-SDK.
Die Dateien sind nach der Sprache geschrieben, aber von keinem Compiler geprüft.

| Projekt | Form | Was darin vorkommt |
|---|---|---|
| `notizen/` | heute: Compose, ViewModel, Coroutinen, Flow | `data class`, `sealed interface`/`sealed class` mit `object`, `enum class` mit Rumpf je Konstante, `typealias`; `companion object` (auch mit `@Volatile`, `synchronized`); `object NotizFormat`; Erweiterungsfunktionen und -eigenschaften (`String.kuerze`, `String.istSchlagwort`), `inline … reified`, `infix`; `when` mit und ohne Gegenstand, als Wert und mit `is`/`in`; `if` als Wert; Marken (`notizen@ for`, `continue@notizen`, `return@forEach`, `return@withContext`); `try`/`catch`/`finally` als Wert; Lambdas: einzeilige (`{ it.id }`), mehrzeilige, nachgestellte, verschachtelte (Compose); anonymes `object : ViewModelProvider.Factory`; Eigenschaft mit `get()` und `set(wert)`; Vorlagen `"${…}"`, `"""…"""` |
| `wetter/` | früher: Activity mit Views, `RecyclerView`-Adapter, Threads, Rückrufe | `lateinit`; `setOnClickListener { … return@setOnClickListener }`; anonymes `object : TextWatcher` mit drei Methoden; `inner class` mit `init`; `interface` mit Standard-Methode; `enum class` mit `abstract fun` je Konstante; zweiter `constructor(…) : this(…)`, `operator fun plus`; `for (i in tage.indices.reversed())`, `0 until n step 7`; `Thread { … }.apply { … }`; mehrere `catch`; ein JSON-Leser in rekursivem Abstieg mit einem Zustandsautomaten (`when (zustand)`, `zeichen@ while`, `continue@zeichen`, `do … while`) |

Die Bau-Skripte (`*.gradle.kts`) liest Code2Diagram noch nicht (Entscheid K6); sie stehen da, damit
die Ordner aussehen wie ein echtes Projekt.
