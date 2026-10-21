[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Modding-Werkzeuge

Sieben Kommandozeilenwerkzeuge zum Erstellen von Blitzkrieg-Mods. Sie ersetzen die Werkzeuge von 2003, die nur unter Windows liefen, und laufen unter Windows, macOS und Linux. Jedes schreibt Dateien, die das Spiel mit seinen eigenen Ladern liest; keines braucht eine Spielinstallation, es sei denn, es nimmt `--data` entgegen.

Wo sie liegen: im Verzeichnis `Tools/` des Spielordners. Unter Windows enthält dieser Ordner `bk-pak`, `bk-table`, `bk-image`, `bk-video` und `bk-font`; Linux und macOS haben zusätzlich `bk-map` und `bk-sprite`. Ein Werkzeug, das in Ihrem `Tools/` fehlt, wird mit `zig build bk-<name>` aus dem Quellcode gebaut und landet in `zig-out/bin`.

## Regeln, die jedes Werkzeug befolgt

- `bk-<tool> --help` gibt die Verwendung aus und beendet sich mit 0. Eine falsche Kommandozeile beendet sich mit 2 und der Verwendung. Ein Fehler beendet sich mit 1 und einer Zeile auf stderr, die mit `bk-<tool>: ` beginnt und die Datei nennt.
- Bei Erfolg gibt ein Werkzeug eine Zusammenfassungszeile aus und sonst nichts.
- Pfade akzeptieren `/` oder `\`; Ausgabepfade werden mit dem Trennzeichen Ihres Systems ausgegeben.
- Ein Werkzeug schreibt nur die Ausgaben, die Sie benennen (oder neben seine Eingabe, wenn es das sagt). Es ersetzt nie eine vorhandene Ausgabe, es sei denn, Sie übergeben `--force`.
- Die Ausgabe ist deterministisch: Dieselbe Eingabe ergibt auf jedem System dieselben Bytes.
- Werkzeuge, die Spieldaten lesen, nehmen `--data <dir>` entgegen, Vorgabe `./Data`. Pfade in einer Karte oder einem Mod sind die Data-Pfade des Spiels, nicht die Ihrer Festplatte.

Ein Mod liegt in `mods/<Name>/data/` neben dem eigenen `Data/` des Spiels, mit einer `mod.xml`, die ihn benennt (`MODName`, `MODVersion`), und das Spiel startet ihn mit `-mod=<Name>`. Das Spiel liest die Dateien in diesem Ordner und die Dateien in jeder dortigen `.pak` gleichermaßen.

## Sprache

Hilfe und Meldungen gibt es in zehn Sprachen. Wählen Sie eine mit `--lang <code>` (zum Beispiel `--lang=ja`), sonst über die Umgebungsvariable `BK_LANG`, sonst die Sprache Ihres Systems, sonst Englisch. Die Präfixe `usage: ` und `bk-<tool>: `, die Exit-Codes und die Zusammenfassungszeilen ändern sich nicht. Unter Windows fehlen dem alten Konsolenfenster möglicherweise CJK-Glyphen; Windows Terminal zeigt sie an.

## Altes Werkzeug, neuer Befehl

| Altes Werkzeug | Neuer Befehl |
|---|---|
| `zip.exe`, `unzip.exe` | `bk-pak pack`, `bk-pak unpack` |
| `zip2exe.exe` | keiner; liefern Sie die `.pak` oder ein normales Zip aus |
| `FontGen.exe` | `bk-font` |
| `ExcelExporter.exe` | `bk-table export`, `bk-table import` |
| `BZMConvertor.exe <map.xml>`, `-nobzm` | `bk-map to-bzm`, `bk-map to-xml` |
| `BZMConvertor.exe -obj2mod`, `-validateobj2mod` | `bk-map obj2mod`, `bk-map validate-obj2mod` |
| `ImageDefrag.exe` (MiniMap Creator) | `bk-map minimap` |
| `spcomp.exe` | `bk-sprite compose` |
| `OffsetRomb.exe` | `bk-image offset-romb` |
| `tools/video/convert_bik_to_ogv.ps1` | `bk-video convert` |
| `A7ExportModel.mll` (Maya 4) | der Blender-Exporter, siehe `docs/blender-replacement.md` |

## bk-pak: `.pak`-Archive erstellen und entpacken

Eine `.pak` ist ein Zip-Archiv. `bk-pak pack` schreibt eines so, wie es der Release-Packer des Spiels selbst tut: jede Datei unkomprimiert gespeichert, Namen sortiert, `/` als Trennzeichen. `unpack` prüft zuerst das ganze Archiv und schreibt nichts, wenn ein Eintrag beschädigt ist oder außerhalb des Zielordners landen würde.

```text
bk-pak pack <dir> <out.pak> [--force]
bk-pak unpack <in.pak> <dir> [--force]
bk-pak list <in.pak>
bk-pak verify <in.pak>
```

Die Namen im Archiv sind die Pfade unterhalb von `<dir>`; legen Sie `<dir>` also wie den `data`-Ordner des Mods an. Zip64-Archive (mehr als 65.535 Einträge oder 4 GiB) werden nicht unterstützt.

Beispiel: eine geänderte Einheit des Mods in den Mod packen.

```text
$ bk-pak pack src mods/MyMod/data/units.pak
packed 1 files (296 bytes) into mods/MyMod/data/units.pak
$ bk-pak list mods/MyMod/data/units.pak
         296  stored   units/tank/1.xml
1 files (296 bytes) in mods/MyMod/data/units.pak
```

## bk-font: eine Spielschrift aus einer TrueType-Schrift erzeugen

Rendert die Zeichen 32..255 einer Codepage in einen Schriftordner wie `Data/Fonts/small`: `1.tfd` (Metriken, Texturrechtecke, A/B/C-Abstände, Kerning), `1.tga` und `1_h.dds`, die Textur, die das Spiel liest.

```text
bk-font --ttf <file|face name> [--height <px>] [--weight <0..1000>] [--italic]
        [--no-aa] [--charset <name>] [--chars <file>] [--set <name>]
        [--clusters <table>] [--script <tag>] [--language <tag>]
        [--face-index <n>] [--out <dir>] [--force] [--modules <dir>]
```

- `--height` ist die Zellhöhe in Pixeln, Oberlänge plus Unterlänge (Vorgabe 20).
- `--charset` ist `ansi` (Codepage 1252, die Vorgabe), `easteurope` (1250), `russian` (1251) oder eine Codepage-Nummer.
- `--chars <file>` ist eine UTF-8-Datei: Die Schrift erhält jedes darin vorkommende verschiedene Zeichen der Basic Multilingual Plane sowie ASCII 32..126; Zeichen, die die Schrift nicht hat, entfallen. `--set latin1`, `cyrillic` oder `devanagari` fügt einen eingebauten Bereich hinzu und lässt sich wiederholen.
- `--clusters <table>` ist die Cluster-Tabelle, die der Export der language-mods geschrieben hat (Zeilen `U+E000<TAB>cluster`): Jeder Cluster wird zu einem Glyphen unter seinem Zeichen aus dem Private Use Area, geformt mit der `--ttf`-Schrift. `--script` (Vorgabe `Deva`) und `--language` (Vorgabe `hi`) wählen das Shaping.
- `--ttf` nimmt eine `.ttf`-, `.otf`- oder `.ttc`-Datei. Ein Schriftname wird in den Systemschriftordnern gesucht, nach bestem Bemühen; eine Datei ist der zuverlässige Weg.
- `--weight 600` oder mehr wählt die fette Variante oder macht eine Schrift fett; `--italic` wählt oder neigt. `--no-aa` schaltet die Kantenglättung aus.
- Prüfen Sie die Lizenz der Schrift, bevor Sie sie in einem Mod ausliefern.

Beispiel: eine 16-Pixel-Schrift für einen Mod aus einer frei lizenzierten Schriftdatei.

```text
$ bk-font --ttf ProggyClean.ttf --height 16 --out mods/MyMod/data/Fonts/small
wrote mods/MyMod/data/Fonts/small/1.tfd, 1.tga and 1_h.dds: 225 characters, 0 kerning pairs, 256x256 atlas
```

## bk-table: Wertedateien in einer Tabellenkalkulation bearbeiten

`export` fasst alle passenden XML-Wertedateien eines Ordners in einer einzigen, tabulatorgetrennten Tabelle zusammen: eine Zeile pro Datei, eine Spalte pro Feld. Excel, LibreOffice und Numbers öffnen sie. `import` schreibt die Zellen, die Sie geändert haben, in die Dateien zurück und ändert dabei nur die Bytes dieser Werte; Kommentare, Reihenfolge und Formatierung von allem anderen bleiben, wie sie waren.

```text
bk-table export <mask> <folder> <out.tsv> [--ignore <fields.txt> | --only <fields.txt>]
                [--no-recurse] [--root <name>]
bk-table import <sheet.tsv> [--root <name>]
```

- Spaltennamen sind Feldpfade: `MaxHP` ist ein Attribut, `KeyName;#text` der Text eines Elements, `Guns;item(01);Ammo` der zweite Eintrag einer Liste.
- Die Werte werden bei `*.bld` und `*.obt` aus `<desc>` gelesen, bei `*.eff` aus `<effect>` und sonst aus `<RPG>`. Mitgelieferte Gebäude haben ihre in `1.xml` unter `<desc>`; exportieren Sie sie daher mit `--root desc`. Ein eigener neuer Werttyp verwendet `--root <name>`.
- `--only` behält und `--ignore` verwirft die Spalten, die mit dem ersten Wort jeder Zeile von `<fields.txt>` beginnen.
- Beim Import lässt eine leere Zelle oder `_` den Wert unverändert. Eine fehlende Datei oder ein fehlendes Feld wird gemeldet und übersprungen; `import` fügt nie etwas hinzu.
- Die Spalte `FileName` ist relativ zu dem Ort, an dem Sie `export` ausgeführt haben; führen Sie `import` vom selben Ort aus. Von einer Tabellenkalkulation gespeicherte Tabellen (Zellen in Anführungszeichen, CRLF) und alte `.txt`-Tabellen werden akzeptiert.

Beispiel: die Trefferpunkte einer Einheit erhöhen.

```text
$ bk-table export "*.xml" Stats stats.tsv
exported 3 files (11 columns) into stats.tsv
```

Ändern Sie `MaxHP` der Zeile `Stats/tank.xml` in der Tabelle von 120 auf 150, speichern Sie sie als tabulatorgetrennten Text und dann:

```text
$ bk-table import stats.tsv
changed 1 values in 1 files from stats.tsv
```

`Stats/tank.xml` enthält nun `MaxHP="150"`; kein anderes Byte hat sich geändert.

## bk-map: Karten, Minikarten und Blender-Modelle

```text
bk-map to-bzm <in.xml|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map to-xml <in.bzm|dir> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map minimap <map|dir> --size <px> [--out <dir>] [--force] [--data <dir>] [--modules <dir>]
bk-map obj2mod <in.obj> <out.mod> [skeleton.txt] [anim.txt] [--modules <dir>]
bk-map validate-obj2mod <in.obj> [skeleton.txt] [anim.txt]
```

- `to-xml` und `to-bzm` wandeln eine Karte zwischen dem binären `.bzm` des Spiels und dem bearbeitbaren `.xml` um, mit demselben Leser und Schreiber wie der Karteneditor.
- `minimap` zeichnet `<map>_c.dds`, `<map>_l.dds` und `<map>_h.dds`, quadratisch mit `<px>`, die Bilder, die das Spiel für eine Karte zeigt.
- Ein Verzeichnis wird rekursiv durchsucht und unter `--out` gespiegelt; ohne `--out` landet jede Ausgabe neben ihrer Eingabe. Bei einer Karte, die sowohl ein `.bzm` als auch ein `.xml` hat, nimmt `minimap` die neuere, wie es das Spiel tut.
- Die Kartenbefehle brauchen das `Data` des Spiels (Straßen, Flüsse, Objekte und Tilesets): Führen Sie sie im Spielordner aus oder übergeben Sie `--data`.
- `obj2mod` macht aus der OBJ des Blender-Exporters und seinen Skelett- und Animationsdateien eine `.mod`; `validate-obj2mod` prüft dieselben Dateien und schreibt nichts. Die Dateiformate und die Blender-Seite stehen in `docs/blender-replacement.md`.

Beispiel: eine Karte als XML bearbeiten, zurückwandeln und ihre Minikarte zeichnen.

```text
$ bk-map to-xml maps/tutorial1.bzm --out edit
wrote edit/tutorial1.xml
$ bk-map to-bzm edit/tutorial1.xml --out back
wrote back/tutorial1.bzm
$ bk-map minimap back/tutorial1.bzm --size 256
wrote back/tutorial1_c.dds, _l.dds and _h.dds, 256x256
```

Beispiel: ein Modell aus Blender.

```text
$ bk-map validate-obj2mod tank.obj tank.skeleton.txt tank.anim.txt
Validation passed: mesh=4 verts, skeleton=2 nodes, animations=1
$ bk-map obj2mod tank.obj out/tank.mod tank.skeleton.txt tank.anim.txt
Converted OBJ to MOD: tank.obj -> out/tank.mod
```

## bk-sprite: Sprite-Animationen zusammenstellen

Packt die gerenderten Einzelbilder eines 2D-Objekts (Brücken, Zäune, Gebäude, Effekte) in eine Textur `<out-name>.tga` und beschreibt seine Animationen in `<out-name>.san`.

```text
bk-sprite compose <desc-dir> <out-name> [--force] [--modules <dir>]
```

`<desc-dir>` enthält `anims.txt`, mit einer `[section]` pro Animation, und die Einzelbilder: Bild `i` der Animation `<name>` ist `<desc-dir>/<name>/<iii>.tga` (`000.tga`, `001.tga`, ...). Eine Animation namens `default` hat das eine Bild `<desc-dir>/1.tga`.

```text
dirs=<n>                 number of directions (required)
dir<k> seq=<i>,<j>,...   frame indices of direction k (required)
frametime=<ms>           one frame's show time (default 125)
speed=<f>                translation speed (default 0)
cycled=<0|1>             cycled or one-shot (default 1)
shift=<x>,<y>            every frame's shift (default 0,0)
dir<k> shift=<x>,<y>     direction k's frames' shift (default: shift)
frame<i> shift=<x>,<y>   frame i's shift (default: its direction's)
```

Die Abschnittsnamen sind die Animationsplätze des Spiels (`idle`, `run`, `death`, ...); `default` und `effect` sind beide `idle`, verwenden Sie also nur eines der drei. `<out-name>` ist ein Pfad ab dem Ort, an dem Sie das Werkzeug ausführen; sein Ordner wird angelegt.

Beispiel: ein Objekt mit einer Ruheschleife in zwei Richtungen und einem einmaligen Tod.

```text
; smoke/anims.txt, frames in smoke/idle/000..002.tga and smoke/death/000..001.tga
[idle]
dirs=2
dir0 seq=0,1,2
dir1 seq=2,1,0
frametime=100
shift=4,8

[death]
dirs=1
dir0 seq=0,1
frametime=150
cycled=0
```

```text
$ bk-sprite compose smoke out/smoke
wrote out/smoke.tga and out/smoke.san: 2 animations, 5 frames, 32x16 atlas
```

## bk-image: Geländekacheln konvertieren

```text
bk-image offset-romb <in.tga> [--out <out.tga>]
```

`offset-romb` vertauscht die beiden rautenförmigen Hälften einer Geländekachel, wie es `OffsetRomb.exe` tat, um Kachelgrafik, die im alten versetzten Layout gezeichnet wurde, in das Layout des Spiels zu bringen. Es liest Echtfarb- und Graustufen-TGA-Dateien und schreibt eine 32-Bit-TGA. Ohne `--out` überschreibt es die Eingabe. Zweimaliges Ausführen ergibt nicht das Original zurück; behalten Sie Ihre Quelldatei.

Beispiel:

```text
$ bk-image offset-romb grass.tga --out grass-game.tga
wrote grass-game.tga: 16x16, romb halves swapped
```

## bk-video: Bink-Filme in Ogg Theora umwandeln

Das Spiel spielt Ogg-Theora-Filme (`.ogv`) ab. `bk-video convert` wandelt jede `.bik` unter einem Ordner in `<name>.ogv` daneben um und die erste Tonspur des Films in eine Begleitdatei `<name>.audio.ogg`. Es ruft `ffmpeg` und `ffprobe` auf, die Sie selbst installieren (jeder gängige Build hat den Bink-Decoder, Theora und Vorbis); ohne sie wird nichts umgewandelt.

```text
bk-video convert [--input-root <dir>] [--force] [--no-recurse] [--what-if]
                 [--ffmpeg <path>] [--ffprobe <path>]
                 [--key-frame-interval <n>] [--audio-suffix <suffix>]
```

- `--input-root` ist standardmäßig `Data/movies`. `--no-recurse` liest nur diesen Ordner.
- `--what-if` listet die Aufträge auf und führt nichts aus. Vorhandene Ausgaben bleiben erhalten, außer mit `--force`.
- `--ffmpeg` und `--ffprobe` zeigen auf die Programme, wenn sie nicht im `PATH` liegen.
- `--key-frame-interval` ist das `-g` von ffmpeg (Vorgabe 1, jedes Bild ein Keyframe).
- Ein Film, den ffmpeg nicht lesen kann, wird auf stderr gemeldet; der Rest wird trotzdem umgewandelt und das Werkzeug beendet sich mit 1.

Beispiel:

```text
$ bk-video convert --input-root movies --what-if
Would convert: movies/intro.bik -> movies/intro.ogv
Would extract audio: movies/intro.bik -> movies/intro.audio.ogg
BIK to OGV conversion finished. Converted=0 Skipped=0 AudioConverted=0 AudioSkipped=0 Failed=0
$ bk-video convert --input-root movies
Converting: movies/intro.bik -> movies/intro.ogv
Extracting audio: movies/intro.bik -> movies/intro.audio.ogg
BIK to OGV conversion finished. Converted=1 Skipped=0 AudioConverted=1 AudioSkipped=0 Failed=0
```
