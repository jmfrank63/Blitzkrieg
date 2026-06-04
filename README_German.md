[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



Dieses Repository ist ein persönliches Projekt von Johannes Maria Frank. Ich nutze es zum Spaß und zum Lernen, besonders um agentisches Programmieren in Brownfield-Anwendungen zu üben. Das ursprüngliche README ist in `readme_original` erhalten.

Warnung: Dieses Projekt befindet sich noch in Arbeit.



# Was dieses Repository heute kann

- Enthält den vollständigen Blitzkrieg Singleplayer-Quellcode und Spieldaten.

- Lässt sich sauber aus einer frischen `A7.sln`-Lösung mit modernen MSVC-Tools kompilieren.

- Startet standardmäßig in einem echten Fenster; `-fullscreen` bleibt als Kommandozeilenoption verfügbar.

- Das Tutorial funktioniert jetzt im Build `Debug | Win32`; Einheiten können ausgewählt und bewegt werden, während das Laufzeit-Debugging weitergeht.

- Enthält native Ausnahmeberichterstattung und moderne Debug-Unterstützung.

- Verwendet Git-Submodule für fehlende Bibliotheken.

- Unterstützt sowohl nativen C++- als auch WinDbg-Debugging in VS Code Insiders.

- Hat das veraltete BugSlay-Crash-Handling entfernt und durch standardmäßige C++-Asserts ersetzt.



# Spiel mit Visual Studio 2026 Insiders ausführen

1. Klone das Repository mit Submodulen, oder führe in einem bestehenden Checkout `git submodule update --init --recursive` aus.

2. Installiere Visual Studio 2026 Insiders / Visual Studio 18 mit dem Workload Desktopentwicklung mit C++, der VS-2026-MSVC-Toolchain und einem Windows 10 oder Windows 11 SDK.

3. Öffne `Sources/src/A7.sln`.

4. Wähle `Debug | Win32`.

5. Baue das Projekt `Game`, oder baue die gesamte Lösung.

6. Starte das Projekt `Game` mit F5, oder führe `Sources/src/Game/Debug/Game.exe` direkt aus.

7. Das Spiel startet standardmäßig im Fenstermodus. Füge `-fullscreen` zu den Befehlsargumenten des Game-Projekts hinzu, wenn du das alte Vollbildverhalten möchtest.

Wenn ein Build keine DLL nach `Sources/src/Game/Debug` kopieren kann, schließe alle laufenden `Game.exe`-Prozesse und baue erneut.



# Verlauf bisher

- Fehlende Bibliotheken wurden gefunden und als Git-Submodule hinzugefügt.

- Es wurde eine Windows XP SP3-VM mit Visual Studio 6 eingerichtet, um die ursprüngliche Umgebung zu prüfen.

- VS6 war instabil und stürzte oft ab, daher wurde der Fokus auf moderne Werkzeuge verlagert.

- Visual Studio 2010 wurde installiert und das alte `.dwr`-Projekt in `.sln` konvertiert.

- Die Lösung wurde in VS 2026 Insiders geladen und alle Abhängigkeiten aktualisiert.

- Zwei Wochen agentengesteuertes Codieren führten dazu, dass die Lösung aus einem sauberen Zustand ohne Fehler und Warnungen kompilierte.

- Das Laufzeit-Debugging ist nun über das Anfangsmenü hinausgekommen: Das Tutorial lädt und ist spielbar, einschließlich Auswahl und Bewegung eines Panzers.

- Die Entwicklung wurde wegen besserer Werkzeuge nach VS Code Insiders verlagert.

- Zwei Debug-Pfade wurden konfiguriert: nativer C++ und WinDbg.

- BugSlay wurde entfernt, weil es das Debugging erschwerte und in einem Fall nach einem Absturz selbst ausfiel.

- BugSlay wurde durch einfache standardmäßige C++-Asserts ersetzt.

- Mit entferntem BugSlay ist das Projekt nun bereit, die gezielte Laufzeit-Debugging-Phase bis zum vollständigen Start fortzusetzen.



# Roadmap

1. Weitere verbleibende Laufzeitausnahmen jenseits des nun funktionierenden Tutorial-Pfads beseitigen.

2. Die Kompilierung auf Zig umstellen.

3. FMOD, Stingray und Bink durch Open-Source-Alternativen ersetzen.

4. C++-Projekte nach und nach durch Zig-Code ersetzen.
