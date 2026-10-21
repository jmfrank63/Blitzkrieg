# Blitzkrieg Vendor Record: Noto and Liberation fonts

The original, unmodified font files (D059 forbids subsetting). `SHA256SUMS` lists their SHA-256; `test-bk-shaper`
checks it against the tracked files.

| File | Source | Version |
|------|--------|---------|
| `NotoSans-Regular.ttf` | https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf | Noto Sans, latin-greek-cyrillic (2022 release line) |
| `NotoSansDevanagari-Regular.ttf` | https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSansDevanagari/hinted/ttf/NotoSansDevanagari-Regular.ttf | Noto Sans Devanagari |
| `NotoSansSC-Regular.otf` | https://github.com/notofonts/noto-cjk/raw/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf | Noto Sans CJK, Simplified Chinese regional face |
| `NotoSansJP-Regular.otf` | https://github.com/notofonts/noto-cjk/raw/main/Sans/SubsetOTF/JP/NotoSansJP-Regular.otf | Noto Sans CJK, Japanese regional face |
| `OFL.txt` | https://github.com/notofonts/noto-cjk/raw/main/Sans/LICENSE | SIL Open Font License 1.1 |
| `LiberationSerif-Regular.ttf` | https://github.com/liberationfonts/liberation-fonts/files/7261482/liberation-fonts-ttf-2.1.5.tar.gz | Liberation Fonts 2.1.5, metric-compatible with Times New Roman |
| `LICENSE-Liberation.txt` | `LICENSE` of the same tarball | SIL Open Font License 1.1 |

The "SubsetOTF" CJK files are Google's own regional packaging of the full Noto Sans CJK (per-region glyph order,
not our subsetting). Downloaded 2026-10-20 from the `main` branch; the SHA-256 in `SHA256SUMS` pins the bytes.

Liberation Serif draws the fonts of the five Latin language mods (es, fr, de, pt-BR, id). Its advance widths are
those of Times New Roman, the face of the game's own `Data/Fonts` bitmaps, so a translation is measured against the
2003 layouts with the widths they were designed for, and the menus keep their serif look. Downloaded 2026-10-21 from
the 2.1.5 release.

Licences: SIL Open Font License 1.1 (`OFL.txt`, `LICENSE-Liberation.txt`), reproduced in `Data/THIRD-PARTY-NOTICES.txt`.

Local patches: none. `.gitattributes` marks the files and `SHA256SUMS` as `-text` so they stay byte-exact.
