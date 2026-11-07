-- Ablauf von Prozeduren in T-SQL (Entscheide R1-R6): IF ohne BEGIN, ELSE IF, WHILE mit Cursor,
-- BEGIN TRY / BEGIN CATCH, RAISERROR, THROW, EXEC einer anderen Prozedur, ein Trigger mit Rumpf.
CREATE TABLE dbo.Artikel (
    ArtikelID int NOT NULL PRIMARY KEY,
    Bestand int NOT NULL,
    Mindest int NOT NULL
);
CREATE TABLE dbo.Bestellung (
    BestellungID int IDENTITY(1, 1) NOT NULL PRIMARY KEY,
    ArtikelID int NOT NULL REFERENCES dbo.Artikel (ArtikelID),
    Menge int NOT NULL
);
GO

CREATE PROCEDURE dbo.Nachbestellen @ArtikelID int, @Menge int = 10
AS
BEGIN
    SET NOCOUNT ON;
    IF @Menge <= 0
        THROW 50001, 'Menge muss positiv sein', 1;
    INSERT INTO dbo.Bestellung (ArtikelID, Menge) VALUES (@ArtikelID, @Menge);
    RETURN 0;
END
GO

CREATE PROCEDURE dbo.PruefeBestand @Gesamt int OUTPUT
AS
BEGIN
    SET NOCOUNT ON;
    DECLARE @id int, @bestand int, @mindest int;
    SET @Gesamt = 0;
    BEGIN TRY
        DECLARE c CURSOR FOR SELECT ArtikelID, Bestand, Mindest FROM dbo.Artikel;
        OPEN c;
        FETCH NEXT FROM c INTO @id, @bestand, @mindest;
        WHILE @@FETCH_STATUS = 0
        BEGIN
            IF @bestand < @mindest AND NOT EXISTS (SELECT 1 FROM dbo.Bestellung WHERE ArtikelID = @id)
            BEGIN
                EXEC dbo.Nachbestellen @ArtikelID = @id;
                SET @Gesamt += 1;
            END
            ELSE IF @bestand < 0
                RAISERROR ('Bestand von %d ist negativ', 16, 1, @id);
            FETCH NEXT FROM c INTO @id, @bestand, @mindest;
        END
        CLOSE c;
        DEALLOCATE c;
    END TRY
    BEGIN CATCH
        IF CURSOR_STATUS('global', 'c') >= 0
        BEGIN
            CLOSE c;
            DEALLOCATE c;
        END;
        THROW;
    END CATCH
    RETURN @Gesamt;
END
GO

CREATE TRIGGER dbo.BestandGeaendert ON dbo.Artikel AFTER UPDATE
AS
BEGIN
    IF UPDATE(Bestand)
        PRINT 'Bestand geaendert';
END
GO
