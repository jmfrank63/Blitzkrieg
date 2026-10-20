[English](README.md)        [Русский](README_Russian.md)        [中文](README_Chinese.md)        [हिन्दी](README_Hindi.md)        [Español](README_Spanish.md)        [Français](README_French.md)        [Deutsch](README_German.md)        [Português](README_Portuguese.md)        [日本語](README_Japanese.md)        [Bahasa Indonesia](README_Indonesian.md)

# Proyek uji editor Blitzkrieg

Folder ini menyediakan satu folder fixture uji cepat untuk setiap editor sumber daya.

## Cara penggunaan

1. Buka `manifest.json` dan pilih folder editor.
2. Jika folder berisi file proyek salinan, buka file itu langsung di `editor.exe`.
3. Jika folder bertipe `external-project-reference`, gunakan jalur GOG eksternal yang tercantum di `README.md` folder tersebut.
4. Jika folder bertipe `runtime-reference`, tidak ada file proyek composer yang ditemukan; gunakan XML runtime yang tercantum sebagai acuan data terdekat saat menguji start editor, jalur impor, dan perilaku render.

## Kebijakan penyalinan

Hanya file yang sudah ada di repositori ini yang disalin ke sini. File instalasi GOG komersial hanya dirujuk lewat jalurnya dan tidak digandakan.
