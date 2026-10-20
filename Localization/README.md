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

## Editor tables

The editors' own texts (M002 S04) live in `Sources/editor/kit/lang/<code>.lang`, one table per language
(`en` and the nine above), embedded in the binaries. Keys carry their owner as a prefix: `kit.` for the
shared kit (the language picker and `kit.common.*`, the verbs OK, Cancel, Yes, No, Apply, Close, Open, Save,
Delete and Add), `elk.` for ELK, `app.` for the Map Editor (`app.m2.*` and `app.m3.*` for its second and third
milestone's windows, `app.cmd.*` and `app.core.*` for command and core messages); the Resource Editor adds
its own when it moves to the tables. To add a key: put it in `en.lang` first, then in all nine other tables with the same
`{0}`..`{9}` placeholders (the glossaries above give the terms), call it as `l10n.t("<key>")` (a menu or
window label that must keep its identity across languages as `l10n.id`), and run `zig build l10n-check`,
which fails on a key one table lacks or holds extra, a changed placeholder, a glyph the language's Noto face
does not cover, and a literal in the strict directories (`Sources/editor/elk/app`, `Sources/editor/app`, `Sources/editor/resource_app`). ELK's help is one file per language,
`Sources/editor/elk/help/elk_<code>.md`, embedded.

A key a table lacks shows the English text and logs `l10n: missing <code> <key>` once to stderr.
`BK_L10N_DIR=<dir>` makes ELK and the Map Editor read `<dir>/<code>.lang` instead of the embedded table, and `BK_FONT_DIR=<dir>`
reads the Noto faces from `<dir>` instead of `Fonts/` beside the executable; both are test seams.

`zig build elk-l10n-auto` (`tools/zig/elk_l10n_auto.zig`, needs `hb-view`) starts the installed ELK hidden
once per language with `ui_language` in a scratch `elk.cfg`, captures its main window, File menu and help
(`ELK --l10n-auto`), and measures every label against hb-view's raster of the table's text. Hindi labels
also pass the headline and i matra checks, `zh-Hans` and `ja` the regional Han face check, and the Latin
languages must differ from the English frame. A tracked table that lacks one key
(`tools/zig/fixtures/l10n/elk-drop/ru.lang`) must log exactly one `l10n: missing ru <key>` line and draw
that label in English. Frames and a `report.txt` with one `ok` or `FAIL` line per check are kept under
`zig-out/local-test/elk/l10n-auto/<code>/`. Hindi labels with a conjunct are held to a lower correlation
because of the half-form limit below.

The Map Editor (M002 S05) draws every label of its app and core from the `app.*` and `kit.common.*` keys;
window and popup titles are `l10n.id` (`Title###key`), so ImGui's saved window positions (`layout.ini`) are
keyed by the key and reset once for people who had one. Scripted runs (`BK_EDITOR_AUTO`, `--smoke`,
`--check`, `--game-reads-it`) draw English whatever the setting says, so their English assertions hold on
any machine; a normal start reads `ui_language` from `mapeditor.cfg`, which the Settings window changes
live. Texts that come from outside the tables stay English: the engine's reasons (`BkEditorLastMessage`),
the operating system's (file and dialog errors) and `fatal()` before the language is known (D080).
The data it shows (object type and game type names such as `SGVOGT_UNIT`, map and script file names) are
the game's own and are not translated.

`zig build map-editor-l10n-auto` (`tools/zig/map_editor_l10n_auto.zig`, needs `hb-view`) is the same
tier for the Map Editor: one hidden start per language with `ui_language` in a scratch `mapeditor.cfg`
(`MapEditor --l10n-auto <map> <folder>` under a fixed `BK_EDITOR_AUTO` schedule), capturing the main window
with the menu bar, the Tools palette and the Settings dialog, and measuring every label the editor logs
(`l10n-auto: rect <key> x y w h`) with the checks of the ELK tier (`tools/zig/l10n_auto_common.zig`). It also
checks the fit: every Tools palette button, the Settings rows, the Objects panel's filter buttons and the
Players panel's labels lie inside their windows, so a translation that a layout constant clips fails with its
language and key. The drop fixture is `tools/zig/fixtures/l10n/map-editor-drop/ru.lang` (without
`app.settings.use_default`); frames and `report.txt` are in `zig-out/local-test/map-editor/l10n-auto/<code>/`.
Hindi labels with a conjunct may have a headline gap of three columns instead of two (the same half-form limit).

## Glossaries

`Localization/glossary/<code>.md` holds one glossary per language (`ru`, `zh-Hans`, `ja`, `hi`, `es`, `fr`, `de`,
`pt-BR`, `id`) with one structure: a title, a note on scope and review state, and three tables
`| English term | Translation | Note |` for unit types, ranks and interface terms (the main menu and the
mission briefing). The S01 menu terms of `ru`, `zh-Hans`, `ja` and `hi` are the overlay texts; the other terms
of those four and all of `es`, `fr`, `de`, `pt-BR` and `id` are a machine seed that needs native review.
The overlays (S02 T04), the S03 checker and the S08 documents take their terms from these files.
`zig build test-l10n-glossary` checks that every file has the three sections, the table header and CRLF lines.

## The completeness gate

`zig build l10n-check` (`tools/zig/l10n_check.zig`, M002 S03) reads the tracked tree and fails when a
translation is missing, stale or malformed. Every finding is one line on stderr,
`l10n-check: <rule>: <code>: <where>: <detail>` (`<code>` is a language code or `all`, `<where>` an ELK key
or a file), the run ends with `l10n-check: <n> findings, <m> checked`, and a scope with nothing to check yet
prints `l10n-check: <scope>: not active`. Exit status is 1 on a finding. The tests are
`zig build test-l10n-check test-l10n-glossary`; CI runs all three as the job `l10n-check` on every push,
without an area gate, so a docs-only push is checked too (D063).

Rules:

- `missing`, `missing-xml`, `empty`, `extra`, `state`, `encoding`: every language overlay holds each scope key as a `.txt` (UTF-16LE with a
  byte order mark) beside its `.xml` state file, and the state is not "not translated".
- `stale`, `base-hash-missing`, `untranslated`: the English source of a key must still match its hash in
  `Localization/game/base-hashes.txt`, and a translation must not equal the English bytes.
- `placeholder`: the complete C printf codes (`%[n$][flags][width][.precision][length]conversion`, so `%ld` and
  `%ls`, `%1$s` and `%2$s` differ and `%%` is a literal), `{n}`, `$name`, `<tag>` tokens and literal `\n` of the English text and
  the translation are the same multiset.
- `glyph`: each code point is in the language's face or NotoSans-Regular (ru, zh-Hans, ja, hi), or in cp1252
  for the five Latin languages that draw with the game's bitmap fonts.
- `table-missing`, `-extra`, `-placeholder`, `-id`, `-format`: a string table directory has `en.lang` and the
  nine other `.lang` files with the same keys, placeholders and ImGui `##`/`###` ID suffixes.
- `doc-missing`, `-structure`, `-code`, `-bar`, `-extra`: a document set (English `<stem>.md` and nine
  `<stem>_<Language>.md`) has the same heading levels and code blocks and the language bar.
- `literal`: in the strict source directories a UI call takes `l10n.t`, `l10n.id` or `l10n.format`, not an
  English string literal.

`Localization/game/scope.txt` lists the ELK keys (relative to `Data/ELK/game_data_base`, no extension) every
overlay must hold. `Localization/game/base-hashes.txt` (D075) holds one SHA-256 of the English bytes per
scope key, because an overlay `.xml` cannot record which English text it was made from. When `stale` fires,
review the translations of that key in every language, then run `zig build l10n-check-update-base` to
rewrite the file. A `same-as-base <code> <key>` line in the same file says that language's translation may
equal the English byte for byte (a proper name); without it `untranslated` fires.

Not active yet (each prints the note until its scope is filled): `table-*` until a directory with an
`en.lang` is listed in `table_dirs`, `doc-*` until S08 adds the
root README set to `document_sets`, and `literal` until S04 to S07 add the migrated source directories to
`strict_dirs`. `--doc-set <stem>` and `--strict-dir <dir>` add one for a single run.
