[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Blitzkrieg Editor Test Projects

This folder provides one smoke-test fixture folder per resource editor.

## How to use

1. Open `manifest.json` and pick the editor folder.
2. If the folder contains a copied project file, open that file directly in `editor.exe`.
3. If the folder is `external-project-reference`, use the listed external GOG path from that folder's `README.md`.
4. If the folder is `runtime-reference`, no composer project file was found; use the listed runtime XML as the closest available data reference while testing editor startup, import paths, and rendering behavior.

## Copy policy

Only files already present in this repository are copied here. Commercial GOG installation files are referenced by path only and are not duplicated into the repo.
