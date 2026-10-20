# Localization overlays

The language mods (M002) are not translated files in the game: they are built by the
`language-mods` step (`zig build language-mods`) from the tracked ELK database `Data/ELK` plus the
overlays in this folder, through ELK's own pak export (`exportTo`).

## Layout

```
Localization/game/<code>/game_data_base/textes/<path>/<key>.txt   the translation, UTF-16LE with BOM
Localization/game/<code>/game_data_base/textes/<path>/<key>.xml   its state file (State="3": Approved)
```

`<code>` is `ru`, `zh-Hans`, `ja` or `hi`. The tree mirrors `Data/ELK/game_data_base`, so an overlay file
replaces (or adds to) the database file of the same key; the originals (`.elk`) are never touched, and
`Data/ELK` itself is never written (the step hashes it before and after). The `.txt` files are kept
byte for byte (`Localization/**/*.txt -text -eol` in `.gitattributes`).

The overlays are real Unicode. Only the exported pak holds the shaped form: for `hi` the export rewrites
every text into Private Use Area characters, one per shaped cluster, and the generated font holds one
glyph per such character (M002 D058).

## Texts

Slice S01 translates the main menu: its header (`strings/mainmenu`, `strings/imheader-mainmenu-header`) and
the thirteen button labels `strings/imheader-gbutton-*` the main menu and its first sub-screens draw
(new game, load game, multiplayer, options, custom game, tutorial, credits, exit game, campaigns,
cutscenes, back, settings).

## Test strings

The tier `l10n-game-reads-it` checks regional Han forms and Hindi shaping on these labels:

- `zh-Hans`: 直 in the tutorial label (`直观教程`) and 化 in the options label (`个性化选项`), both
  characters that differ between the Simplified Chinese and the Japanese face.
- `ja`: 直 in the tutorial label (`直感操作ガイド`) and 化 in the options label (`最適化オプション`).
- `hi`: the reordered short i matra (कि) in the cutscenes label `किस्से`; the conjuncts क्ष in the tutorial
  label `प्रशिक्षण`, त्र in the multiplayer label `मित्रों के साथ` and श्र in the credits label `श्रेय`.
