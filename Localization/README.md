# Localization overlays

The language mods (M002) are not translated files in the game: they are built by the
`language-mods` step (`zig build language-mods`) from the tracked ELK database `Data/ELK` plus the
overlays in this folder, through ELK's own pak export (`exportTo`).

## A fresh install

`language-mods` only runs as a test tier and `install-game` does not depend on it, so a plain install holds
no `mods/lang-<code>/`. The install step is `install-language-mods` (M002 D087); it writes the nine mods into
the staged game in every build mode. One command installs the game, the nine mods, the editors with their
`Fonts/` and ELK, then checks the result:

```
zig build install-game install-language-mods install-map-editor install-resource-editor install-elk verify-install-l10n
```

`verify-install-l10n` checks the staged folder of the build mode (`zig-out/game/<os>/<arch>/debug` or
`release`; add `-Doptimize=ReleaseFast` for the release one). It prints one line
`verify-runtime: missing <path>` for each absent item and exits 1: the `mods/lang-<code>/data/mod.xml` and
`data/texts.pak` of the nine languages, `Fonts/` with the four Noto faces and `OFL.txt` beside the editors and
ELK, and the ten `Tools/README*.md`. The step depends on the install steps above, so it also works alone.


## A missing overlay fails the run

`language-mods` and `install-language-mods` stop with `language-mods: <code>: no overlay at <path>` and a nonzero exit when
a language's overlay is not there, so a sparse checkout without `Localization/` fails at once instead of leaving mods out
for a later step to miss (CI run 39446226101). A deliberate subset is the explicit option `-Dlm-allow-missing` (it sets `BK_LM_ALLOW_MISSING=1` for the test), which prints
`skipped (BK_LM_ALLOW_MISSING)` for each language it leaves out; `-Dlm-langs=ru,ja` limits the run to some codes without it.
`-Dlm-overlays=<folder>` reads the overlays from another root than `Localization/game`; the failure test uses a copy under
`zig-out/local-test/`. The build step does not pass the caller's environment on, which is why these are build options.
## Layout

```
Localization/game/<code>/game_data_base/textes/<path>/<key>.txt   the translation, UTF-16LE with BOM
Localization/game/<code>/game_data_base/textes/<path>/<key>.xml   its state file (State="3": Approved)
```

`<code>` is one of the nine language mods: `ru`, `zh-Hans`, `ja`, `hi`, `es`, `fr`, `de`, `pt-BR` or `id` (English is the game itself). The tree mirrors `Data/ELK/game_data_base`, so an overlay file
replaces (or adds to) the database file of the same key; the originals (`.elk`) are never touched, and
`Data/ELK` itself is never written (the step hashes it before and after). The `.txt` files are kept
byte for byte (`Localization/**/*.txt -text -eol` in `.gitattributes`).

The overlays are real Unicode, and so is the exported pak: since M004 S05 the Game draws with bk_text, which shapes
the text itself in the shipped faces, so a mod holds texts only, with no generated font, atlas or Private Use Area
table (M002 D058's Hindi rewrite is gone); `language-mods` fails on a Private Use Area code point in any exported text.

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

Besides the menu and the briefing the tier captures two more routes (M003 S01), each against a blank background
of its own and written as `options.png` and `encyclopedia.png` next to the report:

- `options`: the settings screen with the cursor parked on its restore-defaults button until the tooltip
  (`tt_button_default`) is up; the caption and the tooltip's box are measured. A reference-scene capture skips
  the screen's step, where the cursor reaches the tooltip, so the schedule lifts `X64.ReferenceScene` with
  `var=` once the screen is up.
- `encyclopedia`: the Allied Churchill's unit page, opened from the main menu with `cmdc=` and the encyclopedia
  command's configuration (`1;Infantry_MKIV_Churchill_GB`); the title, the two panel headers and the wrapped
  description are measured.

Three more screens hold texts the ELK database lacks (2026-10-24), which reach a mod only because `language-mods` adds
an original for every overlay text without one before ELK's export: the main menu's Options page (`mainmenu-options.png`,
Load Mod), the gameplay mods screen (`mods.png`, its caption and Detach All MODs) and the first Allied mission's escape
menu with its End Mission page (`escape.png`, `escape-end.png`: Save Game, Load Game, Help Screen, Finish Mission). Each of
their labels must also match the language's string better than the English one, so a screen left in English fails.

Without a desktop session the tier runs under `xvfb-run -a -s "-screen 0 1280x1024x24"`, with `--cache-poison=poisoned`
so the build does not reuse a configuration cached without `DISPLAY`.

Every language is measured for ink in each label's box and for no missing-glyph box. A language is measured
further only where its scope in `Localization/game/scope.txt` (read with l10n-check's own parser) holds the
route's keys, because a seed-scope language still shows English there: a bitmap-font language must draw all but
one label differently from English, and a face language's static labels are compared with hb-view like the
menu's. A route's report section names its PNG and says which of the two applied.

Known limit: a Hindi half form (a cluster ending in a halant, such as ख्) is drawn with the advance the
cluster has shaped alone (with a zero-width joiner), up to about a tenth wider than in running text.

## Editor tables

The editors' own texts (M002 S04) live in `Sources/editor/kit/lang/<code>.lang`, one table per language
(`en` and the nine above), embedded in the binaries. Keys carry their owner as a prefix: `kit.` for the
shared kit (the language picker and `kit.common.*`, the verbs OK, Cancel, Yes, No, Apply, Close, Open, Save,
Delete and Add), `elk.` for ELK, `app.` for the Map Editor (`app.m2.*` and `app.m3.*` for its second and third
milestone's windows, `app.cmd.*` and `app.core.*` for command and core messages), `resource_app.` for the Resource Editor
(`resource_app.gui.*` for its GUI Editor dock). To add a key: put it in `en.lang` first, then in all nine other tables with the same
`{0}`..`{9}` placeholders (the glossaries above give the terms), call it as `l10n.t("<key>")` for plain text (`igText*`, tooltips, status, format results) and as `l10n.id("<key>")` for every ImGui label that is
a widget, menu, menu item, button, tab, header, popup or window (ImGui hashes the label as the ID; `l10n.id` keeps it the same in every language and apart from an equal translation), and run `zig build l10n-check`,
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

The Resource Editor (M002 S06) draws every label of its app from the `resource_app.*` and `kit.common.*` keys, with the
same `l10n.id` rule for titles (its `layout.ini` is reset once) and the same scripted-runs-stay-English rule (`--smoke`,
`--smoke-edit`, `--check`, `--auto`, the batch CLI). A normal start reads `ui_language` from `resourceeditor.cfg`;
Tools > Language switches it live. The engine's text (D081), the operating system's and the model's property names stay
English. `zig build resource-editor-l10n-auto` (`tools/zig/resource_editor_l10n_auto.zig`, needs `hb-view`) is the same tier
(`ResourceEditor --l10n-auto <fixtures> <scratch>`, `resourceeditor.cfg`), with four shots per language: the main window's menu
bar, the View menu opened by `do=open_menu:<key>`, the Help window opened by `key=f1`, and a second sub-editor with the
Thumbnails dock. It checks the fit as well (every measured label inside the frame, the Help rows inside the Help window).
The drop fixture is `tools/zig/fixtures/l10n/resource-editor-drop/ru.lang` (without `resource_app.menu.help`); frames and
`report.txt` are in `zig-out/local-test/resource-editor/l10n-auto/<code>/`. The dialogs are not measured by a frame.

## Tool tables

The seven `bk-*` tools print their `--help` and messages from `tools/zig/lang/<code>.lang`, one table per language, embedded in each tool
by `tools/zig/bk_l10n.zig` (the C++ tools use its C ABI, `bk_l10n.h`). The language is the first of `--lang <code>`, `BK_LANG`, the OS locale,
then English. An unknown `--lang` value prints an English line listing the valid codes, then the usage, and exits 2.

What stays English in every language, because scripts and CI parse it: the `usage: <tool> ` and `<tool>: ` prefixes, the synopsis and option
names, exit codes, the summary lines and the debug logs.

Two seams for tests and translators: `BK_L10N_DIR=<dir>` reads `<dir>/<code>.lang` instead of the embedded table, and `BK_DEBUG_LOG=1` makes a
tool print `l10n: missing <code> <key>` on stderr (otherwise stderr stays empty on success). A key missing from a language falls back to English.

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
- `glossary-term`, `term-exception`, `term-exception-unused`: see Rule glossary-term below.
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
  English string literal. An ID-bearing call (`igButton`, `igMenuItemEx`, `igBegin`, ...) whose label is `l10n.t(` fails too: use `l10n.id`.

### Scope per language

`Localization/game/scope.txt` says which ELK keys (relative to `Data/ELK/game_data_base`, no extension, forward
slashes) each language overlay must hold. Every line that is not a comment or one of the two directives is a
seed key; all nine languages hold the seed keys. A language widens its scope with:

- `whole-database <code>`: every text the Game reads (`databaseKeys` in `tools/zig/l10n_check.zig`, 3,547 texts): each
  `.elk` original of the database (found case-insensitively), each `.txt` of the database without one, and each text of
  the Game's own text trees (`Data/Textes`, `Data/Scenarios`, `Data/Units`, `Data/Buildings`, `Data/Medals`,
  `Data/Objects`, `Data/movies`) the database lacks, such as the escape menu's save and load captions, the mod screen
  and most tutorial texts. A text whose English is blank is not counted.
- `prefix <code> <key prefix>`: the seed keys plus every such key that starts with the prefix; several lines
  are allowed. `whole-database` makes a language's prefix lines redundant (finding `scope`).

A language with neither line is seed-only. An unknown language code or a malformed line is a `scope`
finding, and an overlay text outside the language's scope is `extra` (the detail names the rule that excludes
it). The run prints one `l10n-check: scope: <code>: <seed | seed + prefixes ... | whole-database>, <n> keys checked`
note per language.

Progress per language (M003 S01 to S06, 2026-10-25): all nine languages are `whole-database`, all 3,547 texts translated
and passing every rule (completeness, placeholders, glyphs, glossary terms and fit); all are machine translations not
reviewed by a native speaker. The English source of a key (`readBase` in `tools/zig/l10n_check.zig`) is the text the
Game shows from its own text trees (`Data/Textes`, `Data/Scenarios`, `Data/Units`, ...), else its ELK original, else
the database `.txt`. The ELK database comes from an early release: for about 1,600 keys (tutorial and mission
objectives, unit names and descriptions with their specification block, the personal names of unit commanders) its
original differs from what the English Game shows, so the translations follow the shipped English text. Where that
English is longer than its room (the unit names of the selection panel, the statistics rows), the translations use
short forms that fit, measured by the fit rule below.

`Localization/game/base-hashes.txt` (D075) holds one SHA-256 of the English bytes per text of the database (and per
seed key without one), because an overlay `.xml` cannot record which English text it was made from; a language
can widen its scope without a new hash run. When `stale` fires, review the translations of that key in every
language, then run `zig build l10n-check-update-base` to rewrite the file. A `same-as-base <code> <key>` line
in the same file says that language's translation may equal the English byte for byte (a proper name); without
it `untranslated` fires.

### Rule fit

The fit rule (`fit`, `fit-word`, `fit-unmapped`, `fit-known`, `fit-known-unused`) measures a translation the way the Game
draws it: the `bk_text` Zig module shapes it with HarfBuzz in the vendored faces (`vendor/fonts`, Liberation Serif and the
Noto faces) at the Game's own size table (`bk_text.sizes`) and breaks the lines with `bk_text`'s wrap. No `1.tfd` and no
generated atlas is read, so all nine languages are measured in plain `zig build l10n-check` and in the cheap CI job;
every language prints a `l10n-check: fit: <code>: ...` note. A character no face holds is measured with the missing glyph
and noted per language. The fit-base allowance compares the translation with the English original measured the same way.
A Japanese text has no spaces, so `fit-word` measures the runs between break opportunities (the game's
`CanBreakTextBetween`), not the whole sentence. `language-mods` runs the same stage for the language it builds, before any
font work, and stops with `TextDoesNotFit` on a finding.

`Localization/game/fit-known.txt` (D108) is the baseline of today's overflows: one line `<code> <key> # S06|M003`. A listed
text is counted as `N known` in its language's note and not reported, so the gate stays meaningful for every other key
while M003 finishes the CJK ones (`# M003`); S06 reworded the Latin and Cyrillic texts, so no `# S06` line is left. A listed line whose text
fits again is the finding `fit-known-unused`; a malformed line is `fit-known`. S04 recorded 102 ru, 46 de, 62 fr, 57 es,
45 pt-BR, 52 id (all `S06`, now reworded) and 25 ja (`M003`) keys; the last seven ja ones were reworded when ja became
`whole-database` (2026-10-24), so the file holds no line today.

`language-mods` builds no atlas any more (M004 S05), so the former 4096x4096 atlas bound no longer applies. The no-start set of the line breaker
also holds U+2025, U+2026, U+30A0 and U+FF5E (TextBreak.h, l10n_wrap.zig and gfx_text_wrap_test.cpp in step).

`l10n-check` does not depend on the `language-mods` step in `build.zig`, on purpose: that step takes about a
minute and takes `-Dlm-langs`, so every check would pay for it.

### Rule glossary-term

For each in-scope key, every English term of `Localization/glossary/<code>.md` (the `English term |
Translation | Note` tables) that occurs as a whole word in the English original, ignoring case, must occur in
the overlay text as a case-insensitive substring of its translation (D098). A multi-word term matches over a
line break. Two ways out where a term does not fit:

- A glossary row whose Note holds `no-term-check` is skipped (a word that is also ordinary English: `back`,
  `general`, `major`, `private`).
- `Localization/game/term-exceptions.txt` holds `<code> <key> <english term> # <reason>` lines that excuse one
  key and one term; the reason is required (`term-exception` otherwise) and a line that excuses nothing any
  more is the finding `term-exception-unused`.

### Translating a prefix: the overlay sheet

`tools/zig/l10n_sheet.zig` writes the overlay files, so nobody types UTF-16LE by hand:

1. `zig build l10n-sheet-export -- <code> <key prefix>...` writes `zig-out/local-test/l10n-sheet/<code>-<n>.txt`
   (UTF-8, CRLF, cut at about 40,000 bytes). Each text is a `=== <key>` line followed by the English text, read as
   l10n-check reads it, with its line breaks; an existing translation follows under a `--- current` line.
2. Replace the English text of every block by the translation (keep the `=== <key>` lines; a trailing line
   break belongs to the text, so keep it where the English has one).
3. `zig build l10n-sheet-import -- <code> <sheet>` writes `Localization/game/<code>/game_data_base/<key>.txt`
   (UTF-16LE with a byte order mark, CRLF inside) and `<key>.xml` (State 2). It refuses the whole sheet, and
   writes nothing, for a key that is not a text of the game (none of the `whole-database` keys above) or an empty block.
4. Add `prefix <code> <prefix>` to `scope.txt` (or `whole-database <code>` when the language is complete) and run
   `zig build l10n-check`. Tests: `zig build test-l10n-sheet`.

Not active yet (each prints the note until its scope is filled): `table-*` until a directory with an
`en.lang` is listed in `table_dirs`, `doc-*` until S08 adds the
root README set to `document_sets`, and `literal` until S04 to S07 add the migrated source directories to
`strict_dirs`. `--doc-set <stem>` and `--strict-dir <dir>` add one for a single run.
