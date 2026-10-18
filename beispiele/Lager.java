// Ein kleines Lager: zeigt, was Code2Diagram an Java liest.
package beispiel.lagerhaltung;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

enum Zustand {
    LEER("leer"),
    TEILWEISE("teilweise"),
    VOLL("voll") {
        @Override
        boolean nimmtAuf() { return false; }
    };

    private final String text;

    Zustand(String text) { this.text = text; }

    boolean nimmtAuf() { return true; }
}

interface Ablage {
    boolean lege(String name, int menge);

    default int anzahl() { return 0; }
}

record Posten(String name, int menge) {
    Posten {
        if (menge < 0) throw new IllegalArgumentException("menge");
    }
}

public class Lager implements Ablage, AutoCloseable {
    private static final Map<String, Integer> GRENZEN = new HashMap<>();
    private final Map<String, Integer> posten = new HashMap<>();
    private final List<Runnable> beobachter = new java.util.ArrayList<>();
    private int grenze = 100;

    static {
        GRENZEN.put("klein", 10);
        GRENZEN.put("gross", 1000);
    }

    public Lager(int grenze) { this.grenze = grenze; }

    @Override
    public boolean lege(String name, int menge) {
        if (menge <= 0 || name == null || name.isEmpty()) {
            return false;
        }
        int alt = posten.getOrDefault(name, 0);
        if (alt + menge > grenze)
            throw new IllegalArgumentException(name);
        posten.put(name, alt + menge);
        synchronized (beobachter) {
            for (Runnable r : beobachter) r.run();
        }
        return true;
    }

    public Zustand zustand() {
        int gesamt = 0;
        for (int menge : posten.values()) gesamt += menge;
        return switch (gesamt) {
            case 0 -> Zustand.LEER;
            case 1, 2, 3 -> {
                if (gesamt < grenze) yield Zustand.TEILWEISE;
                yield Zustand.VOLL;
            }
            default -> Zustand.VOLL;
        };
    }

    public String suche(String[][] regale, String name) {
        String fund = null;
        aussen:
        for (String[] regal : regale) {
            for (String fach : regal) {
                if (fach == null) continue aussen;
                if (fach.equals(name)) {
                    fund = fach;
                    break aussen;
                }
            }
        }
        return fund;
    }

    public static Lager lade(String pfad) throws IOException {
        Lager lager = new Lager(100);
        try (BufferedReader leser = new BufferedReader(new FileReader(pfad))) {
            String zeile;
            while ((zeile = leser.readLine()) != null) {
                String[] teile = zeile.split(";");
                lager.lege(teile[0], Integer.parseInt(teile[1]));
            }
        } catch (NumberFormatException fehler) {
            System.err.println("kaputt: " + fehler.getMessage());
            throw fehler;
        } finally {
            System.out.println("geladen: " + pfad);
        }
        lager.beobachter.add(new Runnable() {
            @Override
            public void run() {
                System.out.println("geaendert");
            }
        });
        lager.beobachter.add(() -> {
            System.out.println("noch einmal");
        });
        return lager;
    }

    @Override
    public void close() { posten.clear(); }
}
