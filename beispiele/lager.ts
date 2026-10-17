// Ein kleines Lager: zeigt, was Code2Diagram an TypeScript liest.
import { readFile } from "node:fs/promises";

export enum Zustand {
    Leer,
    Teilweise = 1,
    Voll = 2,
}

export interface Posten {
    readonly name: string;
    menge: number;
    notiz?: string;
}

interface Ablage {
    lege(name: string, menge: number): boolean;
}

type Bericht = { gesamt: number; zustand: Zustand };
type Filter = (p: Posten) => boolean;

export class Lager implements Ablage {
    #posten = new Map<string, Posten>();

    constructor(private readonly grenze: number) {}

    lege(name: string, menge: number): boolean {
        if (menge <= 0 || !name) {
            return false;
        }
        let posten = this.#posten.get(name);
        if (posten === undefined) {
            posten = { name, menge: 0 };
            this.#posten.set(name, posten);
        } else if (posten.menge + menge > this.grenze) {
            throw new RangeError(name);
        }
        posten.menge += menge;
        return true;
    }

    get zustand(): Zustand {
        const gesamt = [...this.#posten.values()].reduce((s, p) => s + p.menge, 0);
        switch (true) {
            case gesamt === 0:
                return Zustand.Leer;
            case gesamt < this.grenze:
                return Zustand.Teilweise;
            default:
                return Zustand.Voll;
        }
    }

    suche(filter: Filter): Posten[] {
        const treffer: Posten[] = [];
        for (const p of this.#posten.values()) {
            if (filter(p)) treffer.push(p);
        }
        return treffer;
    }
}

export async function lade(pfad: string): Promise<Lager> {
    const lager = new Lager(100);
    try {
        const daten = JSON.parse(await readFile(pfad, "utf-8"));
        Object.entries(daten).forEach(([name, menge]) => {
            if (typeof menge !== "number") {
                console.warn("keine Zahl:", name);
                return;
            }
            lager.lege(name, menge);
        });
    } catch (fehler) {
        console.error("kaputt:", fehler);
        throw fehler;
    } finally {
        console.log("geladen:", pfad);
    }
    return lager;
}

export const bericht = (lager: Lager): Bericht => {
    return { gesamt: lager.suche(() => true).length, zustand: lager.zustand };
};
