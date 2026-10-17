// Ein kleines Lager: zeigt, was Code2Diagram an C# liest.
#define PROTOKOLL
using System;
using System.Collections.Generic;
using System.IO;

namespace Beispiel.Lagerhaltung;

public enum Zustand { Leer, Teilweise = 1, Voll = 2 }

public interface IAblage
{
    bool Lege(string name, int menge);
    int Anzahl { get; }
}

public record Posten(string Name, int Menge);

public delegate bool Filter(Posten p);

public class Lager : IAblage, IDisposable
{
    private readonly Dictionary<string, int> _posten = new();
    public int Grenze { get; init; } = 100;
    public int Anzahl => _posten.Count;
    public event EventHandler? Geaendert;

    public Lager(int grenze) => Grenze = grenze;

    public bool Lege(string name, int menge)
    {
        if (menge <= 0 || string.IsNullOrEmpty(name))
        {
            return false;
        }
        _posten.TryGetValue(name, out var alt);
        if (alt + menge > Grenze)
            throw new ArgumentOutOfRangeException(nameof(menge));
        _posten[name] = alt + menge;
        Geaendert?.Invoke(this, EventArgs.Empty);
        return true;
    }

    public Zustand Zustand
    {
        get
        {
            var gesamt = 0;
            foreach (var menge in _posten.Values) gesamt += menge;
            return gesamt switch
            {
                0 => Zustand.Leer,
                < 100 when gesamt < Grenze => Zustand.Teilweise,
                _ => Zustand.Voll,
            };
        }
    }

    public IEnumerable<Posten> Suche(Filter filter)
    {
        foreach (var (name, menge) in _posten)
        {
            var p = new Posten(name, menge);
            if (!filter(p)) continue;
            yield return p;
        }
    }

    public static Lager Lade(string pfad)
    {
        var lager = new Lager(100);
        try
        {
            using (var leser = new StreamReader(pfad))
            {
                string? zeile;
                while ((zeile = leser.ReadLine()) != null)
                {
                    var teile = zeile.Split(';');
                    lager.Lege(teile[0], int.Parse(teile[1]));
                }
            }
        }
        catch (FileNotFoundException)
        {
            return lager;
        }
        catch (FormatException fehler) when (fehler.Message.Length > 0)
        {
#if PROTOKOLL
            Console.Error.WriteLine($"kaputt: {fehler.Message}");
#endif
            throw;
        }
        finally
        {
            Console.WriteLine($"geladen: {pfad}");
        }
        lager.Geaendert += (sender, e) =>
        {
            Console.WriteLine("geaendert");
        };
        return lager;
    }

    public void Dispose() => _posten.Clear();
}
