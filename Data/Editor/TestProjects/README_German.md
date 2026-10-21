[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Testprojekte der Blitzkrieg-Editoren

Dieser Ordner enthält pro Ressourcen-Editor einen Ordner mit einer Testvorlage für den Smoke-Test.

## Verwendung

1. `manifest.json` öffnen und den Editor-Ordner auswählen.
2. Enthält der Ordner eine kopierte Projektdatei, sie direkt in `editor.exe` öffnen.
3. Ist der Ordner `external-project-reference`, den externen GOG-Pfad aus der `README.md` dieses Ordners verwenden.
4. Ist der Ordner `runtime-reference`, wurde keine Composer-Projektdatei gefunden; die angegebene Laufzeit-XML dient dann als nächstliegende Datenreferenz, um Editorstart, Importpfade und Darstellung zu testen.

## Kopierregel

Hier liegen nur Dateien, die bereits im Repository vorhanden sind. Dateien der kommerziellen GOG-Installation werden nur per Pfad referenziert und nicht dupliziert.
