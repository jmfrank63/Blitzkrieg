# Blitzkrieg ELK

ELK, das Blitzkrieg-Lokalisierungswerkzeug, ist der Ort, an dem Sie die Texte des Spiels übersetzen. Es zeigt jeden Text des Spiels mit seinem Original, Sie tippen die Übersetzung, markieren, wie weit jeder Text ist, und übernehmen Ihre Übersetzung anschließend als Sprachmod ins Spiel.

## Starten von ELK

- Allein gestartet öffnet ELK die Datenbank, die Sie zuletzt benutzt haben.
- ELK path/to/elk.xml öffnet diese Datenbank.
- ELK --short ist das Übersetzer-Layout: Es öffnet die elk.xml neben dem Programm, bringt sie zuerst mit der neuesten dortigen .upd-Updatedatei auf den neuesten Stand und zeigt eine Symbolleiste mit den fünf festen Filtern. Es startet jedes Mal auf dieselbe Weise.
- ELK --developer, oder gar keine Option, ist das erweiterte Layout mit allen Menüs, den benannten Filtern, die Sie selbst anlegen, und der Liste der zuletzt geöffneten Datenbanken.
- Die Sprache der ELK-Menüs, -Meldungen und dieser Hilfe wählen Sie unter Extras > Sprache. ELK merkt sie sich; System folgt der Sprache Ihres Betriebssystems.

## Öffnen und Speichern

Eine Datenbank ist ein Ordner mit einer Datei elk.xml und je einem Ordner pro Teil des Spiels, zum Beispiel game_data_base. Öffnen Sie eine mit Datei > Öffnen (Ctrl+O) oder wählen Sie sie unter Datei > Zuletzt geöffnet.

Sie verlieren keine Arbeit: ELK speichert den gerade bearbeiteten Text, wenn Sie zu einem anderen Text wechseln, bei Datei > Speichern (Ctrl+S), bei Datei > Schließen, beim Beenden und alle 30 Sekunden, solange etwas ungespeichert ist. Die Statusleiste unten zeigt an, wenn der Text ungespeicherte Änderungen hat.

## Der Textbaum

Links steht der Baum aller Spieltexte, in Ordnern, wie das Spiel sie ablegt. Klicken Sie auf einen Text, um ihn anzuzeigen: oben sein Original, darunter seine Beschreibung (eine Notiz für Übersetzer, falls der Text eine hat) und darunter seine Übersetzung. Original und Beschreibung sind schreibgeschützt, Sie können daraus aber auswählen und kopieren. Öffnen Sie einen Ordner mit seinem Pfeil oder per Doppelklick.

Jede Zeile hat die Farbe ihres Status, und ein Ordner hat die Farbe des am wenigsten fertigen Textes darin; ein roter Ordner enthält also noch Arbeit.

## Status

Jeder Text hat einen von vier Status, den Sie mit den Schaltflächen zwischen Original und Übersetzung wählen:

- Nicht übersetzt (rot): noch nichts getan.
- Veraltet (gelb): Das Original hat sich nach der Übersetzung durch ein Update geändert. Prüfen Sie die Übersetzung und setzen Sie sie erneut. Nur ein Update oder ein Tabellenimport setzt diesen Status.
- Übersetzt (grün): übersetzt, noch nicht geprüft. Das Tippen in eine Übersetzung setzt diesen Status.
- Freigegeben (blau): übersetzt und geprüft.

## Filter und Durchsuchen

Der Filter bestimmt, welche Texte die Befehle unter Durchsuchen ablaufen. Der Baum blendet die Texte, die der Filter ausschließt, aus, zeigt sie aber weiterhin an.

- Die fünf festen Filter sind Alle, Nicht übersetzt, Veraltet, Übersetzt und Freigegeben.
- Durchsuchen > Nächster (Ctrl+N) und Vorheriger (Ctrl+P) springen zum nächsten oder vorherigen Text, den der Filter behält, und der Cursor steht sofort in der Übersetzung. Im erweiterten Layout springen Erster und Letzter an die Enden.
- Eintrag einklappen schließt die Ordner, die Sie beim Durchsuchen verlassen, sodass nur der Weg zum aktuellen Text offen bleibt.
- Im erweiterten Layout erstellt Durchsuchen > Filter erstellen... eigene Filter: einen Namen, die Status, die sie behalten, Nur geänderte und Ordnerbedingungen wie medals&german (jeder Teil muss im Pfad des Ordners vorkommen).

## Suchen

Bearbeiten > Suchen... (Ctrl+F, im kurzen Layout unter Extras) durchsucht die Originale, Beschreibungen und Übersetzungen aller Texte in der Reihenfolge des Baums. Weitersuchen springt zur nächsten Stelle; Groß-/Kleinschreibung, Nur ganze Wörter und Aufwärts oder Abwärts funktionieren wie in anderen Programmen. Die Statusleiste nennt das Feld und die Stelle, an der der Text gefunden wurde.

## Statistik

Ansicht > ELK-Statistik zählt Texte, Wörter und Zeichen der Originale und Übersetzungen, insgesamt und pro Status, damit Sie sehen, wie viel noch offen ist. Daneben stehen die Zahlen vom letzten Schließen der Datenbank nach dem Zählen, sodass Sie sehen, wie viel sich seitdem geändert hat.

Ansicht > ELK-Baum blendet die Liste der Texte aus oder ein, und Ansicht > Statusleiste (erweitertes Layout) die Statuszeile; ELK merkt sich beides.

## Rechtschreibprüfung

Extras > Rechtschreibung (F7) prüft die Übersetzung, die Sie bearbeiten, von Anfang an. Sie hält an jedem falsch geschriebenen Wort an und setzt ihren besten Vorschlag in das Feld Ändern in; klicken Sie auf einen anderen Vorschlag oder tippen Sie einen eigenen. Ignorieren überspringt das Wort diesmal, Alle ignorieren überspringt es, bis Sie ELK schließen, Ändern ersetzt es durch den Text in Ändern in, und Alle ändern ersetzt es überall in der Übersetzung. Erneut prüfen beginnt von vorn. Eine Änderung ist eine Bearbeitung wie getippter Text und wird wie diese gespeichert. Darunter ist jedes falsch geschriebene Wort mit seinen Vorschlägen aufgelistet; Zum Wörterbuch hinzufügen bewahrt ein Wort, zum Beispiel einen Namen, in Ihrer eigenen Wörterbuchdatei custom.dic auf, sodass es nie wieder markiert wird.

Die Rechtschreibprüfung benötigt ein Hunspell-Wörterbuch, eine .dic- und eine .aff-Datei Ihrer Sprache, zum Beispiel aus LibreOffice. Legen Sie sie in den Ordner dictionaries neben ELK oder wählen Sie unter Extras > Rechtschreiboptionen... einen anderen Ordner und die Sprache. Solange kein Wörterbuch gefunden wird, ist die Rechtschreibung aus und nennt den Grund.

## Tabellen

Sie können in einem Tabellenprogramm übersetzen und das Ergebnis zurückbringen. Im kurzen Layout stehen diese Befehle, der Export und Spiel starten unter Extras und in der Symbolleiste.

- Datei > In Tabelle exportieren... schreibt jeden Text mit Pfad, Original, Übersetzung, Status und Beschreibung in eine .xlsx-Arbeitsmappe (Standard) oder eine UTF-8-.csv-Datei.
- Datei > Aus Tabelle importieren... liest eine solche Tabelle oder eine alte .xls-Tabelle des ursprünglichen ELK. Jede Zeile mit einer Übersetzung setzt die Übersetzung dieses Textes. Eine Zeile, deren Original nicht mehr zur Datenbank passt, markiert den Text als Veraltet. Zeilen ohne Übersetzung werden übersprungen. Die Statusleiste nennt, wie viele Zeilen gelesen und geändert wurden.
- Lassen Sie die Spalte Path unverändert: Sie sagt ELK, zu welchem Text eine Zeile gehört.

## Originale aktualisieren

Ändern sich die Texte des Spiels, übernehmen Sie die neuen Originale in die geöffnete Datenbank. Ein Text, dessen Original sich geändert hat, wird Veraltet, wenn er übersetzt oder freigegeben war, sonst Nicht übersetzt; Ihre Übersetzungen bleiben erhalten.

- Datei > Aus dem Spiel importieren... liest die Texte des Sprachordners eines Spiels (des Ordners mit Textes). Neue Texte werden hinzugefügt; nichts wird gelöscht.
- Datei > Aus PAK/UPD importieren... liest eine .pak- oder .upd-Datei. Ihr Name bestimmt den Teil der Datenbank, den sie aktualisiert: game_12.upd aktualisiert game. Eine .upd ist ein vollständiges Update und entfernt auch die Texte, die sie nicht mehr enthält.
- Die Statusleiste nennt, wie viele Texte importiert wurden. Dieselbe Datei erneut zu importieren ändert nichts.

## Die Übersetzung ins Spiel bringen

- Datei > In Sprachmod exportieren... schreibt Ihre Übersetzung als Mod: Geben Sie einen Namen ein, und ELK schreibt mods/<Name>/data/texts.pak in den Spielordner. Das ist der sichere Weg: Das Spiel liest einen Mod vor seinen eigenen Textdateien, und das Originalspiel bleibt unberührt.
- Lässt der aktuelle Filter im erweiterten Layout Texte aus, bietet der Exportdialog an, nur die Texte zu schreiben, die der Filter behält, zum Beispiel nur die freigegebenen. Die ausgelassenen Texte sind in diesem Mod gar nicht enthalten.
- Im Spiel wählen Sie den Mod unter Optionen > Einstellungen > Spielverlauf > Sprache. Das Spiel wechselt sofort die Sprache und merkt sich die Wahl. Wählen Sie dort die Originalsprache, um zurückzuwechseln.
- Datei > Im Spiel installieren... schreibt Data/Texts.pak ins Spiel selbst, wie es das ursprüngliche ELK tat. Hat das Spiel lose Textdateien, die sie verdecken würden, warnt ELK Sie und bietet stattdessen den Mod an.
- Datei > Spiel starten exportiert den Mod mit Ihren neuesten Änderungen erneut und startet das Spiel damit, in einem separaten Testprofil, sodass Ihre eigenen Spielstände und Einstellungen unberührt bleiben.
- ELK sucht das Spiel in dem Ordner, aus dem es läuft.

## Lange Vorgänge

Die Exporte, die Importe und eine Suche, die jeden Text lesen muss, laufen im Hintergrund. Ein Fenster Arbeitet zeigt, was ELK tut und wie weit es ist, und der Rest von ELK wartet, bis es fertig ist. Abbrechen stoppt den Vorgang sicher: Ein abgebrochener Export schreibt nichts, und ein abgebrochener Import behält die bereits gelesenen Zeilen, jede Datei ist intakt, sodass Sie die Tabelle später erneut importieren können.

## Tasten

- Ctrl+O öffnen, Ctrl+S speichern
- Ctrl+N oder Alt+N nächster Text, Ctrl+P oder Alt+P vorheriger Text (Alt nur, solange der Cursor nicht in der Übersetzung steht)
- Ctrl+F suchen
- F7 Rechtschreibung
- F1 diese Hilfe
