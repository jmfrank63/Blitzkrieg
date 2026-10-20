# Blitzkrieg Vendor Record: Noto fonts

The original, unmodified font files (D059 forbids subsetting). `SHA256SUMS` lists their SHA-256; `test-bk-shaper`
checks it against the tracked files.

| File | Source | Version |
|------|--------|---------|
| `NotoSans-Regular.ttf` | https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSans/hinted/ttf/NotoSans-Regular.ttf | Noto Sans, latin-greek-cyrillic (2022 release line) |
| `NotoSansDevanagari-Regular.ttf` | https://github.com/notofonts/notofonts.github.io/raw/main/fonts/NotoSansDevanagari/hinted/ttf/NotoSansDevanagari-Regular.ttf | Noto Sans Devanagari |
| `NotoSansSC-Regular.otf` | https://github.com/notofonts/noto-cjk/raw/main/Sans/SubsetOTF/SC/NotoSansSC-Regular.otf | Noto Sans CJK, Simplified Chinese regional face |
| `NotoSansJP-Regular.otf` | https://github.com/notofonts/noto-cjk/raw/main/Sans/SubsetOTF/JP/NotoSansJP-Regular.otf | Noto Sans CJK, Japanese regional face |
| `OFL.txt` | https://github.com/notofonts/noto-cjk/raw/main/Sans/LICENSE | SIL Open Font License 1.1 |

The "SubsetOTF" CJK files are Google's own regional packaging of the full Noto Sans CJK (per-region glyph order,
not our subsetting). Downloaded 2026-10-20 from the `main` branch; the SHA-256 in `SHA256SUMS` pins the bytes.

Licence: SIL Open Font License 1.1 (`OFL.txt`), reproduced in `Data/THIRD-PARTY-NOTICES.txt`.

Local patches: none. `.gitattributes` marks the files and `SHA256SUMS` as `-text` so they stay byte-exact.
