-- Ablauf von Routinen in MySQL (Entscheide R1-R6): IF … ELSEIF … END IF, LOOP mit Marke und LEAVE,
-- REPEAT … UNTIL, CASE als Anweisung, EXIT- und CONTINUE-Handler, SIGNAL.
CREATE TABLE artikel (id INT PRIMARY KEY, bestand INT NOT NULL, mindest INT NOT NULL);

DELIMITER //
CREATE FUNCTION fehlmenge(p_id INT) RETURNS INT DETERMINISTIC READS SQL DATA
BEGIN
  DECLARE v_bestand INT;
  DECLARE v_mindest INT;
  DECLARE EXIT HANDLER FOR NOT FOUND RETURN NULL;
  SELECT bestand, mindest INTO v_bestand, v_mindest FROM artikel WHERE id = p_id;
  IF v_bestand >= v_mindest THEN
    RETURN 0;
  ELSEIF v_bestand < 0 THEN
    SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'negativer Bestand';
  END IF;
  RETURN v_mindest - v_bestand;
END //

CREATE PROCEDURE zaehle_fehlende(OUT p_anzahl INT)
BEGIN
  DECLARE fertig INT DEFAULT 0;
  DECLARE v_id INT;
  DECLARE c CURSOR FOR SELECT id FROM artikel;
  DECLARE CONTINUE HANDLER FOR NOT FOUND SET fertig = 1;
  SET p_anzahl = 0;
  OPEN c;
  lesen: LOOP
    FETCH c INTO v_id;
    IF fertig = 1 THEN
      LEAVE lesen;
    END IF;
    IF fehlmenge(v_id) > 0 THEN
      SET p_anzahl = p_anzahl + 1;
    END IF;
  END LOOP lesen;
  CLOSE c;
END //

CREATE PROCEDURE warte_auf(IN p_id INT)
BEGIN
  DECLARE versuche INT DEFAULT 0;
  warten: REPEAT
    SET versuche = versuche + 1;
    CASE fehlmenge(p_id)
      WHEN 0 THEN LEAVE warten;
      WHEN 1, 2 THEN DO SLEEP(1);
      ELSE DO SLEEP(5);
    END CASE;
  UNTIL versuche >= 10 END REPEAT warten;
END //
DELIMITER ;
