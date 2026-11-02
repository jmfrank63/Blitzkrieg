[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)


[![Blitzkrieg Trailer](Blitzkrieg.png)](https://www.youtube.com/watch?v=zNxMvTcsJbk)

Dieses Repository ist ein persönliches Projekt von Johannes Maria Frank. Ich nutze es zum Spaß und zum Lernen, besonders um agentisches Programmieren an bestehenden Anwendungen (Brownfield) zu üben. Das ursprüngliche README ist in `readme_original` erhalten.

Achtung: Dieses Projekt ist in Arbeit.


# Wo das Projekt steht (November 2026)

Die Portierung selbst ist größtenteils fertig: Das Spiel läuft nativ unter Windows, macOS und Linux, es spricht zehn Sprachen, und alle Editoren sind portiert. Aktuell hängt das Projekt an der Grafik, genauer an den gedrehten Ansichten von Gebäuden und Requisiten. Siehe unten „Die Wand: die Grafik“.


# Was bisher geschafft ist

**Das Spiel**

- Das gesamte Einzelspielerspiel und seine Daten sind hier, gebaut mit `zig build` (Zig + clang) als natives 64-Bit-Programm. Kampagnen, Tutorials und Zufallsmissionen sind spielbar, einschließlich Speichern/Laden, Video und Ton.

- Die Engine ist von Win32 und DirectX auf eine portable Schicht umgezogen: SDL3 für Fenster, Eingabe und Audio sowie ein SDL-GPU-Renderer.

- Die geschlossenen Bibliotheken sind weg: FMOD, Bink und Stingray wurden entfernt. Ton und Filme nutzen jetzt Ogg Vorbis und Ogg Theora.

- StreamIO.dll, die Persistenzschicht der Engine, ist durch eine Zig-Implementierung ersetzt (`Sources/src/StreamIOZig`).

- Der alte C++-Code wird mit clang und UBSan übersetzt, was Dutzende Fehler aufgedeckt und behoben hat, die der Build von 2003 stillschweigend duldete.

- Die ursprünglichen Zufallsmissionen sind wieder da, und jedes Kampagnenkapitel schaltet seine historische Mission frei, so wie es sein Skript vorsieht.

- Spielerprofile können einem Spieler per Cloud-Synchronisierung zwischen Rechnern folgen.

- Der LAN-Mehrspielermodus funktioniert. Der GameSpy-Code ist entfernt, und das Spielen über das Internet ist noch nicht angebunden.

**Zehn Sprachen**

- Das Spiel, alle Editoren, die Kommandozeilenwerkzeuge samt ihren Anleitungen und dieses README gibt es auf Englisch, Russisch, Chinesisch (vereinfacht), Hindi, Spanisch, Französisch, Deutsch, Portugiesisch (Brasilien), Japanisch und Indonesisch.

- Die neun Übersetzungen werden als Sprach-Mods installiert, mit einer neuen TrueType-Textengine, die jede dieser Schriften darstellt.

- Die Übersetzungen sind maschinell erstellt und müssen noch von Muttersprachlern geprüft werden. Korrekturen sind willkommen.

**Alle Editoren**

- Der Map Editor, der Resource Editor mit allen seinen Unter-Editoren und ELK, das Lokalisierungs-Kit, sind als Zig-Apps mit Dear ImGui neu geschrieben (`Sources/editor`). Sie sprechen über eine C-ABI mit der C++-Engine (`Sources/src/EditorBridge`).

- Die alten MFC-Editoren, die nur unter Windows liefen, sind gelöscht. Das Spiel liest, was die neuen Editoren schreiben, und Tests prüfen das mit den eigenen Lesern des Spiels.

- Die alten Kommandozeilenwerkzeuge sind durch sieben portable zum Erstellen von Mods ersetzt: `bk-pak`, `bk-table`, `bk-image`, `bk-video`, `bk-font`, `bk-map` und `bk-sprite`. Siehe `tools/guide/README.md`.

**Die Option „Remastered“**

- Optionen > Einstellungen > Gameplay > Remastered ist standardmäßig aus, und aus bedeutet: Es gelten die Originalregeln. An fügt Funktionen hinzu, die das Spiel von 2003 nicht hatte, etwa das Drehen der Kamera in vier Schritten, einen Wegfinder für die schnellste Route und das direkte Umsetzen von Infanterie von einem Container in einen anderen.

- Missions-Wiederholungen sind im Bau; ihre Grundlagen sind eingepflegt.


# Die Wand: die Grafik

Eine drehbare Kamera heißt, dass jedes Gebäude und jede Requisite von vier Seiten gezeigt werden muss, aber die Grafik von 2003 besteht aus je einem Bild pro Objekt, nur von einer Seite. Die anderen Seiten wurden nie gezeichnet.

Bisher habe ich für alle 412 Objekte zwei Ansätze ausprobiert:

- Jedes gedrehte Bild aus Pixeln von 2003 zusammenbauen (aufgegeben am 2026-10-29: Es schuf für jedes gelöste Problem ein neues).

- Einfache 3D-Modelle rendern, die mit denselben Bildern bemalt sind. Das macht das Projekt heute. Die Seite, die die Grafik von 2003 zeigt, sieht richtig aus, aber jeder Teil, den sie nie zeigte, muss erraten werden. Auch Details scheitern: Ein Dachfenster, das auf ein flaches Dach gemalt ist, kann von einer anderen Seite nicht richtig aussehen.

Beides ist nicht gut genug. Der richtige Weg ist ein echtes 3D-Modell jedes Objekts, mit Details wie Dachfenstern als echter Geometrie und einer Textur auf jeder ebenen Fläche, sodass sich jede Seite aus der Form ergibt.

Die Frage ist, woher diese Modelle und Texturen kommen:

- Sie von Hand oder mit Agenten nach den Entwürfen von 2003 modellieren.

- Die Modelle von Blitzkrieg 2, die ebenfalls Nival veröffentlicht hat. Ein Testrender zeigt, dass sie so aufgebaut sind und bei allen vier Drehungen sauber aussehen, aber viele sind andere Gebäude als die gleichnamigen von 2003, und die Lizenz deckt ihre Grafik nicht eindeutig ab.

- Bild-zu-3D-Generatoren, die ich an dieser Grafik noch nicht ausprobiert habe.

Eine Entscheidung ist nicht gefallen. Wenn du 3D-Künstler bist oder dich mit solcher Arbeit auskennst, würde ich gern von dir hören.


# Blender-Asset-Konvertierung und -Prüfung

Die Ersatz-Pipeline für Maya40 ist in `docs/blender-replacement.md` dokumentiert.

Kurzbefehle:

- Konvertieren: `bk-map obj2mod <input.obj> <output.mod> [skeleton.txt] [animation.txt]`

- Nur prüfen: `bk-map validate-obj2mod <input.obj> [skeleton.txt] [animation.txt]`


# Unterstützte Plattformen

Ein Ziel wird mit `-Dtarget=` übergeben. Der erzeugte Baum ist als `zig-out/game/<os>/<arch>/<config>` aufgebaut, sodass mehrere Ziele und beide Optimierungsstufen nebeneinander liegen.

| Plattform | Target-Triple | Status |
| --- | --- | --- |
| Windows (MSVC) | `x86_64-windows-msvc` | Spiel und Editoren laufen; in der CI getestet |
| macOS (Apple Silicon) | `aarch64-macos` | Spiel und Editoren laufen; in der CI getestet |
| Linux (x86_64) | `x86_64-linux-gnu` | Spiel und Editoren laufen; in der CI auf einem Software-Renderer getestet |
| macOS (Intel) | `x86_64-macos` | Wird gebaut; der CI-Runner hat keine GPU für das Spiel, daher wird das Spiel auf einem echten Intel-Mac geprüft |
| Linux (arm64, Raspberry Pi) | `aarch64-linux-gnu` | Nur Plattformtests; Spiel und Editoren noch nicht getestet |
| Windows (MinGW) | `x86_64-windows-gnu` | Nur Plattformtests; die Engine lässt sich hier noch nicht übersetzen |

Die CI führt jedes Ziel auf einem Runner der eigenen Architektur aus, damit die Suiten tatsächlich laufen und nicht bloß gelinkt werden. Einige Jobs unter Linux und macOS schlagen derzeit fehl.

Unter Linux wird das Fenstersymbol im Programm gesetzt, was X11 und KDE beachten. GNOME unter Wayland ignoriert es und nimmt das Symbol für Dock und Alt-Tab aus einem Desktop-Eintrag, der nach der App-ID des Programms benannt ist (`org.blitzkrieg.game`, und `org.blitzkrieg.mapeditor`, `org.blitzkrieg.resourceeditor`, `org.blitzkrieg.elk` für die Editoren). Die Installation enthält ein Skript zum freiwilligen Ausführen, das diese Einträge und das Symbol nach `~/.local/share` schreibt; nichts führt es für dich aus. Führe aus dem Installationsordner `Tools/install_desktop_entry.sh` aus (aus einem Checkout: `tools/install_desktop_entry.sh [install-dir]`), und `Tools/install_desktop_entry.sh --remove` macht es rückgängig.

Nur `x86_64-windows-msvc` braucht eine Visual-Studio-Installation. Zig liefert für jedes andere Ziel, auch MinGW, eigene Toolchain, Header und Import-Bibliotheken mit.


# Bauen und starten

1. Das Repository mit Submodulen klonen oder in einem vorhandenen Checkout `git submodule update --init --recursive` ausführen.

2. Zig 0.17 installieren. Für `x86_64-windows-msvc` werden außerdem die MSVC-Toolchain und ein Windows SDK benötigt (die Pfade lassen sich mit `-Dmsvc-include`/`-Dwindows-sdk-include` und den zugehörigen `lib`-Optionen ändern, falls sie von den Standardwerten in `build.zig` abweichen).

3. `zig build install-all -Dtarget=<triple> --release=fast` ausführen für das Spiel, die neun Sprach-Mods und alle Editoren, oder `zig build install-game` nur für das Spiel. Ohne `-Dtarget` wird für den Host gebaut, und ohne `--release=fast` entsteht ein Debug-Build.

4. Die erzeugte Programmdatei starten, zum Beispiel `zig-out/game/windows/x86_64/release/Game.exe` unter Windows oder `zig-out/game/macos/arm64/release/Game` auf Apple Silicon (`.../debug/...` ohne `--release=fast`). Das Spiel läuft in einem Fenster; `-fullscreen` gibt es als Kommandozeilenoption.

5. `zig build package` erstellt verteilbare Zip-Pakete, und `zig build --help` listet jeden Schritt und Test auf.


# Bisheriger Verlauf

- Die fehlenden Bibliotheken gefunden und die Solution von 2003 unter modernem Visual Studio aus sauberem Zustand zum Kompilieren gebracht, dann das erste Tutorial gespielt.

- Den Build auf Zig umgestellt: `build.zig` kompiliert jedes Projekt mit clang und legt ein lauffähiges Spiel-Layout ab.

- StreamIO.dll durch Zig ersetzt und das Spiel unter dem Zig-Build von Anfang bis Ende spielbar gemacht.

- Das Spiel auf 64 Bit portiert und es mit UBSan Absturz für Absturz durch die Tutorials spielgetestet.

- Von Win32 auf SDL3 und einen SDL-GPU-Renderer umgestellt und das Spiel auf macOS und Linux in Betrieb genommen.

- Den Map Editor, den Resource Editor und ELK portiert und die MFC-Versionen gelöscht. Die alten Kommandozeilenwerkzeuge ersetzt.

- FMOD, Bink und Stingray entfernt.

- Die Textengine und die zehn Sprachen in Spiel, Editoren und Werkzeugen ergänzt.

- Die Option „Remastered“ und die Kameradrehung ergänzt, was zur oben beschriebenen Wand führte.


# Fahrplan

1. Die Wand der Grafik überwinden: entscheiden, woher die 3D-Modelle und Texturen kommen, und für alles, was sie braucht, eine Erlaubnis einholen.

2. Missions-Wiederholungen fertigstellen.

3. Wieder alle CI-Jobs zum Bestehen bringen.

4. Spiel und Editoren auf Linux arm64 und MinGW in Betrieb nehmen und Windows arm64 hinzufügen.

5. Das Mehrspielen über das Internet durch einen OpenSpy-Ersatz für GameSpy anbinden.

6. Ein Werkzeug bauen, das 32-Bit-Spielstände in das 64-Bit-Format umwandelt.

7. C++-Projekte weiter durch Zig-Code ersetzen, mit dem StreamIO-Port als Vorlage.
