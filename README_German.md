[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)



[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)



Dieses Repository ist ein persönliches Projekt von Johannes Maria Frank. Ich nutze es zum Spaß und zum Lernen, besonders um agentisches Programmieren in Brownfield-Anwendungen zu üben. Das ursprüngliche README ist in `readme_original` erhalten.

Warnung: Dieses Projekt befindet sich noch in Arbeit.



# Was dieses Repository heute kann

- Enthält den vollständigen Blitzkrieg Singleplayer-Quellcode und Spieldaten.

- Lässt sich sauber aus einer frischen `A7.sln`-Lösung mit modernen MSVC-Tools kompilieren.

- Enthält native Ausnahmeberichterstattung und moderne Debug-Unterstützung.

- Verwendet Git-Submodule für fehlende Bibliotheken.

- Unterstützt sowohl nativen C++- als auch WinDbg-Debugging in VS Code Insiders.

- Hat das veraltete BugSlay-Crash-Handling entfernt und durch standardmäßige C++-Asserts ersetzt.



# Verlauf bisher

- Fehlende Bibliotheken wurden gefunden und als Git-Submodule hinzugefügt.

- Es wurde eine Windows XP SP3-VM mit Visual Studio 6 eingerichtet, um die ursprüngliche Umgebung zu prüfen.

- VS6 war instabil und stürzte oft ab, daher wurde der Fokus auf moderne Werkzeuge verlagert.

- Visual Studio 2010 wurde installiert und das alte `.dwr`-Projekt in `.sln` konvertiert.

- Die Lösung wurde in VS 2026 Insiders geladen und alle Abhängigkeiten aktualisiert.

- Zwei Wochen agentengesteuertes Codieren führten dazu, dass die Lösung aus einem sauberen Zustand ohne Fehler und Warnungen kompilierte.

- Das bedeutete nicht, dass das Spiel vollständig lief, aber es erreichte einmal das Laden des Tutorials, und Video sowie das Anfangsmenü luden bereits.

- Die Entwicklung wurde wegen besserer Werkzeuge nach VS Code Insiders verlagert.

- Zwei Debug-Pfade wurden konfiguriert: nativer C++ und WinDbg.

- BugSlay wurde entfernt, weil es das Debugging erschwerte und in einem Fall nach einem Absturz selbst ausfiel.

- BugSlay wurde durch einfache standardmäßige C++-Asserts ersetzt.

- Mit entferntem BugSlay ist das Projekt nun bereit, die gezielte Laufzeit-Debugging-Phase bis zum vollständigen Start fortzusetzen.



# Roadmap

1. Das Spiel ohne Ausnahmen zum Laufen bringen.

2. Die Kompilierung auf Zig umstellen.

3. FMOD, Stingray und Bink durch Open-Source-Alternativen ersetzen.

4. C++-Projekte nach und nach durch Zig-Code ersetzen.
