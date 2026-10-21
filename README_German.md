[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Dieses Repository ist ein persönliches Projekt von Johannes Maria Frank. Ich nutze es zum Spaß und zum Lernen, besonders um agentisches Programmieren an bestehenden Anwendungen (Brownfield) zu üben. Das ursprüngliche README ist in `readme_original` erhalten.

Achtung: Dieses Projekt ist in Arbeit.


# Was dieses Repository heute kann

- Enthält den vollständigen Quellcode des Einzelspielermodus von Blitzkrieg samt Spieldaten.

- Baut das ganze Spiel mit `zig build` (Zig + clang) für sechs Ziele — Windows (MSVC und MinGW), macOS (Intel und Apple Silicon) und Linux (x86_64 und arm64) — und erzeugt ein lauffähiges Installationsverzeichnis unter `zig-out/game/<os>/<arch>/<config>`. Nur das MSVC-Ziel braucht Visual Studio; Zig bringt für alle anderen seine eigene Toolchain mit.

- Wird als native **64-Bit**-Game.exe gebaut und gestartet (`zig build install-game -Dtarget=x86_64-windows-msvc`); die 32-Bit-Ziele sind eingestellt. Kampagnenmissionen und Tutorials sind in x64 spielbar, einschließlich Speichern/Laden, Video und Ton.

- Wird mit einer vollständigen Zig-Implementierung anstelle von StreamIO.dll ausgeliefert (`Sources/src/StreamIOZig`) — die gesamte Persistenzschicht der Engine: binäre Objektgraphen zum Speichern und Laden, XML-Datenbäume, die Objektfabrik, globale Variablen, Konsole und Optionen.

- Übersetzt den alten C++-Code mit clang und aktiviertem UBSan, was Dutzende latenter Fehler aufgedeckt und behoben hat, die der MSVC-Build von 2003 stillschweigend duldete — darunter ein 22 Jahre alter Absturz durch einen rein virtuellen Aufruf beim Abbau von Panzer und Turm.

- Prüft jedes unterstützte Ziel in der CI und führt die Testsuiten der Plattformen nativ auf einem eigenen Runner aus — Linux x86_64 und arm64, Windows MSVC und MinGW, macOS Intel und Apple Silicon — statt nur zu cross-kompilieren.

- Baut weiterhin sauber aus der ursprünglichen Solution `A7.sln` unter moderner MSVC-Toolchain (`Debug | Win32`) als Verhaltensreferenz.

- Läuft standardmäßig in einem echten Fenster; `-fullscreen` bleibt als Kommandozeilenoption erhalten.

- Nutzt Git-Submodule für fehlende Bibliotheken.

- Unterstützt natives C++- und WinDbg-Debugging in VS Code Insiders sowie Skripte zur Absturzerfassung ohne Oberfläche unter `tools/zig/`.

- Bietet einen Hilfs-Workflow zum Verpacken komprimierter Filmdateien unter `VideoAssets/`.

- Hat die alte BugSlay-Absturzbehandlung entfernt und durch Standard-C++-Asserts ersetzt.


# Blender-Asset-Konvertierung und -Prüfung

Die Ersatz-Pipeline für Maya40 ist in `docs/blender-replacement.md` dokumentiert.

Kurzbefehle:

- Konvertieren: `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Nur prüfen: `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Unterstützte Plattformen

`zig build` akzeptiert sechs Ziele. Eines wird mit `-Dtarget=` übergeben; der erzeugte Baum ist als `zig-out/game/<os>/<arch>/<config>` aufgebaut, sodass mehrere Ziele und beide Optimierungsstufen nebeneinander liegen, statt sich zu überschreiben.

| Plattform | Target-Triple | Status |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | Spielbar |
| macOS (Apple Silicon) | `aarch64-macos` | Spielbar |
| macOS (Intel) | `x86_64-macos` | Wird gebaut; nicht spielgetestet |
| Linux (x86_64) | `x86_64-linux-gnu` | In Arbeit |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Nur Plattformschicht |
| Windows (MinGW) | `x86_64-windows-gnu` | Nur Plattformschicht |

„Nur Plattformschicht“ bedeutet: Das Ziel wird gebaut und besteht die Plattform-Testsuiten, aber der vollständige Spiel-Build wurde dafür noch nicht in Betrieb genommen.

Nur `x86_64-windows-msvc` braucht eine Visual-Studio-Installation. Zig liefert für jedes andere Ziel, auch MinGW, eigene Toolchain, Header und Import-Bibliotheken mit.

Jedes Ziel wird vom Workflow `Cross-platform validation` auf einem Runner der eigenen Architektur geprüft, damit die Suiten tatsächlich laufen und nicht bloß gelinkt werden.


# Das Spiel mit Zig starten (empfohlen)

1. Das Repository mit Submodulen klonen oder in einem vorhandenen Checkout `git submodule update --init --recursive` ausführen.

2. Zig installieren (0.17 oder neuer). Für `x86_64-windows-msvc` werden außerdem die MSVC-Toolchain und ein Windows SDK benötigt (die Pfade lassen sich mit `-Dmsvc-include`/`-Dwindows-sdk-include` und den zugehörigen `lib`-Optionen ändern, falls sie von den Standardwerten in `build.zig` abweichen). Kein anderes Ziel braucht sie.

3. `zig build install-game -Dtarget=<triple> --release=fast` ausführen, um zu spielen, oder `--release=fast` weglassen für einen Debug-Build. Ohne `-Dtarget` wird für den Host gebaut.

4. Die erzeugte Programmdatei starten — `zig-out/game/windows/x86_64/release/Game.exe` unter Windows, `zig-out/game/macos/arm64/release/Game` auf Apple Silicon (`.../debug/...` ohne `--release=fast`). Der erzeugte Baum ist nach der enthaltenen Optimierung benannt, sodass sich beide nie überschreiben.

5. `zig build package` erstellt verteilbare Zip-Pakete; `zig build test` führt die Zig-Unit-Tests und den C++-ABI-Smoke-Test aus.


# Das Spiel mit Visual Studio 2026 Insiders starten

1. Das Repository mit Submodulen klonen oder in einem vorhandenen Checkout `git submodule update --init --recursive` ausführen.

2. Visual Studio 2026 Insiders / Visual Studio 18 mit der Workload „Desktopentwicklung mit C++“, der MSVC-Toolchain von VS 2026 und einem Windows-10- oder Windows-11-SDK installieren.

3. `Sources/src/A7.sln` öffnen.

4. `Debug | Win32` auswählen.

5. Das Projekt `Game` oder die gesamte Solution bauen.

6. Das Projekt `Game` mit F5 starten oder `Sources/src/Game/Debug/Game.exe` direkt ausführen.

7. Das Spiel startet standardmäßig im Fenster. Mit `-fullscreen` in den Befehlszeilenargumenten des Projekts `Game` kehrt das alte Vollbildverhalten zurück.

Kann ein Build eine DLL nicht nach `Sources/src/Game/Debug` kopieren, den laufenden `Game.exe`-Prozess beenden und erneut bauen.


# Bisheriger Verlauf

- Die fehlenden Bibliotheken gefunden und als Git-Submodule eingebunden.

- Eine Windows-XP-SP3-VM mit Visual Studio 6 aufgesetzt, um die Originalumgebung zu studieren.

- VS 6 war instabil und stürzte oft ab, daher wechselte die Arbeit zu moderner Werkzeugkette.

- Visual Studio 2010 installiert und das alte `.dwr`-Projekt in eine `.sln` umgewandelt.

- Die Solution in VS 2026 Insiders geladen und alle Abhängigkeiten aktualisiert.

- Zwei Wochen agentisches Programmieren, bis die Solution aus sauberem Zustand ohne Fehler und Warnungen kompilierte.

- Das Debuggen zur Laufzeit ist nun über das Startmenü hinaus: Das Tutorial lädt und ist spielbar, einschließlich Auswahl und Bewegung eines Panzers.

- Die Entwicklung zu VS Code Insiders verlegt, um bessere Werkzeuge zu haben.

- Zwei Debug-Wege eingerichtet: natives C++ und WinDbg.

- BugSlay entfernt, weil es das Debuggen erschwerte und in einem Fall nach einem Absturz selbst abstürzte.

- BugSlay durch einfache Standard-C++-Asserts ersetzt.

- Ohne BugSlay ist das Projekt bereit für gezieltes Laufzeit-Debugging bis zum vollständigen Durchlauf.

- Den Build auf Zig umgestellt: `build.zig` kompiliert jedes Projekt mit clang, linkt gegen die MSVC-Toolchain und legt ein lauffähiges Spiel-Layout ab.

- StreamIO.dll — die Persistenz- und Dienstschicht der Engine — durch eine Zig-Implementierung samt C++-ABI-Brücke ersetzt und dabei das Objektgraph-Format beim Speichern/Laden, XML-Lesen/-Schreiben, Optionen und Leistungsregressionen pro Frame repariert.

- Das Spiel unter dem Zig-Build von Anfang bis Ende spielbar gemacht: Kampagnen, Tutorials, Entern, Luftfahrt, Speichern/Laden, synchrones Video, korrekte Minimap bei modernen Auflösungen.

- Das Spiel auf 64 Bit portiert: Zeigerabschneidungen im Renderer, x87-`_control87`-Aufrufe, `#pragma pack(1)` auf STL-Typen, x86-Inline-Assembler (durch portable SSE-Intrinsics ersetzt), den Lua-Unit-Zeigerkanal und die Objekt-IDs im Speicherformat repariert.

- Den x64-Build durch die Tutorials Absturz für Absturz spielgetestet, wobei UBSan jedes 20 Jahre alte undefinierte Verhalten auf Datei und Zeile genau benannte: Minimap-Koordinaten, Formations-Hashing, Winkelrechnung beim Bombardement und mehr.

- Von Win32 auf eine portable Plattformschicht umgestellt: SDL3 für Fenster, Eingabe und Audio, ein SDL-GPU-Renderer anstelle des DirectX-Pfads und ein kleiner nativer Adapter je Betriebssystem.

- Das Spiel auf macOS in Betrieb genommen, wo es auf Apple Silicon spielbar ist; Linux folgt.

- Linux arm64 (Raspberry-Pi-Klasse) und die Windows-GNU-ABI (MinGW) als Ziele hinzugefügt, wofür im ganzen Build „Ist das Windows?“ von „Ist das Visual Studio?“ getrennt werden musste. Dabei zeigte sich, dass Intel-macOS ganz ohne seine Cocoa-Adapter kompiliert wurde, dass der Linux-Job mangels C-Laufzeit rot war und dass die Linux-C++-Pfade an das x86_64-Multiarch-Triple gebunden waren.

- Alle sechs Ziele unter CI gestellt, jedes auf einem Runner der eigenen Architektur.


# Fahrplan

1. Den 64-Bit-Build durch die restlichen Tutorials und Kampagnen stabilisieren.

2. Den vollständigen Spiel-Build auf Linux arm64 und MinGW in Betrieb nehmen — heute sind nur ihre Plattformschichten validiert — und Intel-macOS spielgetestet prüfen.

3. Ein Werkzeug bauen, das 32-Bit-Spielstände in das 64-Bit-Format umwandelt.

4. FMOD, Stingray und Bink durch Open-Source-Alternativen ersetzen.

5. C++-Projekte nach und nach durch Zig-Code ersetzen, mit dem StreamIO-Port als Vorlage.
