# Localization overlays

The language mods (M002) are not translated files in the game: they are built by the
`language-mods` step (`zig build language-mods`) from the tracked ELK database `Data/ELK` plus the
overlays in this folder, through ELK's own pak export (`exportTo`).

## Layout

```
Localization/game/<code>/game_data_base/textes/<path>/<key>.txt   the translation, UTF-16LE with BOM
Localization/game/<code>/game_data_base/textes/<path>/<key>.xml   its state file (State="3": Approved)
```

`<code>` is one of the nine language mods: `ru`, `zh-Hans`, `ja`, `hi`, `es`, `fr`, `de`, `pt-BR` or `id` (English is the game itself). The tree mirrors `Data/ELK/game_data_base`, so an overlay file
replaces (or adds to) the database file of the same key; the originals (`.elk`) are never touched, and
`Data/ELK` itself is never written (the step hashes it before and after). The `.txt` files are kept
byte for byte (`Localization/**/*.txt -text -eol` in `.gitattributes`).

The overlays are real Unicode. Only the exported pak holds the shaped form: for `hi` the export rewrites
every text into Private Use Area characters, one per shaped cluster, and the generated font holds one
glyph per such character (M002 D058).

## Texts

Every language has the same seed set: the 14 main-menu keys of slice S01 plus the 11 keys the first Allied mission
briefing reads (S02 T01): the mission's `0h`, `0`, `header` and `description` under
`scenarios/scenariomissions/allies/norway/`, and `strings/imheader-mission-objectives`, `-map`, `-desc`,
`scenariomission`, `missiontypetext`, `missiontext`, `missiondescriptiontext`. The Latin languages (`es`, `fr`, `de`,
`pt-BR`, `id`) use the bitmap fonts and must stay inside cp1252; `language-mods` checks every key the overlay supplies (the English base still holds Russian
tutorial and medal texts, which the Latin mods inherit and the build only counts).

Review state: all of it is a machine seed (`ru` and the S01 menu texts of `zh-Hans`, `ja` and `hi` from slices S01,
the rest from S02) using the terms of `Localization/glossary/<code>.md`; no native speaker has reviewed it yet.

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

## The measuring tier

`zig build l10n-game-reads-it` (it builds the language mods first, and needs `hb-view` from
`libharfbuzz-bin`; without it the tier fails, D069) starts the real Game on each language mod and measures
the captured main menu against hb-view rasters of the same strings. The frame minus a capture of the menu
with the same fonts blanked is the ink of the labels. Thresholds are named constants in
`tools/zig/l10n_game_reads_it.zig`, each with how it was measured. Frames, the references and `report.txt` are
written under `zig-out/local-test/l10n-game-reads-it/<code>/`.

Known limit: a Hindi half form (a cluster ending in a halant, such as ख्) is drawn with the advance the
cluster has shaped alone (with a zero-width joiner), up to about a tenth wider than in running text.

## Glossaries

`Localization/glossary/<code>.md` holds one glossary per language (`ru`, `zh-Hans`, `ja`, `hi`, `es`, `fr`, `de`,
`pt-BR`, `id`) with one structure: a title, a note on scope and review state, and three tables
`| English term | Translation | Note |` for unit types, ranks and interface terms (the main menu and the
mission briefing). The S01 menu terms of `ru`, `zh-Hans`, `ja` and `hi` are the overlay texts; the other terms
of those four and all of `es`, `fr`, `de`, `pt-BR` and `id` are a machine seed that needs native review.
The overlays (S02 T04), the S03 checker and the S08 documents take their terms from these files.
`zig build test-l10n-glossary` checks that every file has the three sections, the table header and CRLF lines.
