-- PostgreSQL database dump
-- (die Zeile oben belegt den Dialekt, Entscheid E2)
-- Ablauf von Funktionen in PL/pgSQL (Entscheide R1-R6): DECLARE-Teil, IF … ELSIF, RAISE EXCEPTION und
-- RAISE NOTICE, EXCEPTION WHEN, FOR … IN SELECT mit Marke, EXIT … WHEN, CONTINUE WHEN, RETURN NEXT,
-- CASE ohne ELSE, ein Trigger ohne eigenen Rumpf, ein DO-Block.
CREATE TABLE artikel (id integer PRIMARY KEY, bestand integer NOT NULL, mindest integer NOT NULL);
CREATE TABLE bestellung (id serial PRIMARY KEY, artikel_id integer NOT NULL REFERENCES artikel (id), menge integer NOT NULL);

CREATE FUNCTION nachbestellen(p_id integer, p_menge integer DEFAULT 10) RETURNS integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    v_bestand integer;
    v_neu integer;
BEGIN
    SELECT bestand INTO v_bestand FROM artikel WHERE id = p_id;
    IF NOT FOUND THEN
        RAISE EXCEPTION 'Artikel % unbekannt', p_id;
    ELSIF v_bestand > 100 AND p_menge > 0 THEN
        RAISE NOTICE 'Artikel % hat genug', p_id;
        RETURN NULL;
    END IF;
    INSERT INTO bestellung (artikel_id, menge) VALUES (p_id, p_menge) RETURNING id INTO v_neu;
    RETURN v_neu;
EXCEPTION
    WHEN foreign_key_violation THEN
        RAISE WARNING 'Bestellung fuer % abgelehnt', p_id;
        RETURN NULL;
END;
$$;

CREATE FUNCTION nachbestellen_alle() RETURNS SETOF integer
    LANGUAGE plpgsql
    AS $$
DECLARE
    r record;
    n integer := 0;
BEGIN
    <<artikel_schleife>>
    FOR r IN SELECT id, bestand, mindest FROM artikel ORDER BY id LOOP
        CONTINUE WHEN r.bestand >= r.mindest;
        n := n + 1;
        EXIT artikel_schleife WHEN n > 50;
        RETURN NEXT nachbestellen(r.id);
    END LOOP;
    CASE
        WHEN n = 0 THEN RAISE NOTICE 'nichts nachbestellt';
        WHEN n > 50 THEN RAISE NOTICE 'Grenze erreicht';
    END CASE;
    RETURN;
END;
$$;

CREATE FUNCTION artikel_geaendert() RETURNS trigger
    LANGUAGE plpgsql
    AS $$
BEGIN
    IF NEW.bestand < NEW.mindest THEN
        PERFORM nachbestellen(NEW.id);
    END IF;
    RETURN NEW;
END;
$$;

CREATE TRIGGER artikel_nach_update AFTER UPDATE ON artikel FOR EACH ROW EXECUTE FUNCTION artikel_geaendert();

DO $$
BEGIN
    IF (SELECT count(*) FROM artikel) = 0 THEN
        INSERT INTO artikel VALUES (1, 0, 5);
    END IF;
END
$$;
