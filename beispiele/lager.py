"""Ein kleines Lager: zeigt, was Code2Diagram an Python liest."""
from dataclasses import dataclass
import enum
import json


class Zustand(enum.Enum):
    LEER = 0
    TEILWEISE = 1
    VOLL = 2


@dataclass
class Posten:
    name: str
    menge: int = 0


class Lager:
    """Posten nach Namen, mit einer Grenze."""

    def __init__(self, grenze: int):
        self.grenze = grenze
        self.posten: dict[str, Posten] = {}

    def lege_ab(self, name: str, menge: int) -> bool:
        if menge <= 0 or not name:
            return False
        posten = self.posten.get(name)
        if posten is None:
            posten = Posten(name)
            self.posten[name] = posten
        elif posten.menge + menge > self.grenze:
            raise ValueError(name)
        posten.menge += menge
        return True

    def zustand(self) -> Zustand:
        gesamt = sum(p.menge for p in self.posten.values())
        match gesamt:
            case 0:
                return Zustand.LEER
            case n if n < self.grenze:
                return Zustand.TEILWEISE
            case _:
                return Zustand.VOLL

    def suche(self, teil: str):
        for name in sorted(self.posten):
            if teil in name:
                break
        else:
            return None
        return self.posten[name]


def lade(pfad: str) -> Lager:
    lager = Lager(100)
    try:
        with open(pfad, encoding="utf-8") as datei:
            daten = json.load(datei)
    except FileNotFoundError:
        return lager
    except json.JSONDecodeError as fehler:
        print("kaputt:", fehler)
        raise
    else:
        for name, menge in daten.items():
            lager.lege_ab(name, menge)
    finally:
        print("geladen:", pfad)
    return lager


if __name__ == "__main__":
    print(lade("lager.json").zustand())
