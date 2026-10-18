# Milestone brief: Localization (game, editors, tools, documents)

Johannes, 2026-10-18: "We translate everything, so we maximise the user engagement.
Localisation to finish first." Item 4 of the migration order (after ELK and the tools,
both merged). Work on the branch `feat/localization`; the maintainer merges with
`--no-ff` into main when the milestone is done.

## Languages

The ten of the README set: English (the source), Russian, Chinese (Simplified), Hindi,
Spanish, French, German, Portuguese (Brazil), Japanese, Indonesian. Every user-facing
text exists in all ten.

## Scope: everything a player, modder or editor user reads

1. Documents. `README.md` is current (166 lines, 2026-10-16); the nine `README_<Language>.md`
   are the 91-line text of 2026-06-04 and miss three of its seven sections. Translate the
   current README in full into each language and keep the language bar in every file.
   `README_English.md` is a stale duplicate of `README.md`: delete it and point the bars at
   `README.md`. Every other user-facing document gets the same set: the tools guide that
   ships in `Tools/`, the editors' and ELK's help (`Sources/editor/elk/help/elk.md` and any
   other embedded help), `Data/Editor/TestProjects/README.md` and the per-project READMEs
   there, and anything else the packages ship. Developer notes (`docs/`, `.planning/`,
   fixture READMEs, `Sources/**/README.md` for programmers) stay English.
2. The game. Its text is English data under `Data/` (Data/ELK holds the database ELK edits).
   Produce a language mod per language with ELK's own export (File > Export to language mod)
   from translated ELK databases, so the in-game Language option (Phase 7, E-11) offers all
   ten. The translation covers every string the database holds, interface and campaign text
   alike.
3. The editors (Map Editor, Resource Editor, ELK). Their interface text is hard-coded English
   in the Zig sources. Introduce one string-table mechanism shared through `Sources/editor/kit`
   (keys, one table per language, English as the fallback), a language setting in each
   editor's settings (the BK_*_SETTINGS seams), and move every user-visible string into it.
4. The tools (`bk-*`): their `--help` text and user-facing messages, through the same tables or
   a Zig equivalent the tools share. Machine-readable output (logs that tests parse) stays as it
   is.

## Research first (before planning the slices)

- Scripts. Chinese and Japanese need thousands of glyphs; Hindi (Devanagari) needs text shaping
  (conjuncts, vowel signs), which neither the 2003 engine's bitmap fonts nor Dear ImGui does.
  Measure what the engine's font path (bk-font generates its fonts) and ImGui can show, and
  decide per surface (game, editors) how each script is shown correctly: a glyph atlas per
  language, a shaping step (e.g. HarfBuzz) before the glyphs are placed, or pre-shaped text.
  "Correct" means a native reader would accept it, proved by captured frames measured by code,
  not by eyeballing.
- Fonts and licences: which fonts cover all ten scripts and may be shipped (record each in
  THIRD-PARTY-NOTICES).
- Sizes: German, Russian and Hindi texts are longer than English; find the fixed-size UI
  elements that clip and how the game and editors handle overflow.

## Proof (each slice)

- A completeness check in CI: every language has every key/string the English source has
  (game database, editor tables, tools), and every README translation has the same section
  structure as `README.md`. A missing or stale translation fails the build.
- The game reads it: for each language, the real Game started on its language mod draws a
  translated main menu and a mission briefing; frames are captured and measured by code
  (as elk-game-reads-it and bk-font-game-reads-it do), including the CJK and Devanagari ones.
- Each editor started with each language setting draws its translated interface (hidden
  window, captured frame, measured), and a missing key falls back to English visibly in a log.
- Existing tiers stay green on Linux; CI (cross-platform.yml, the fan-out) on all platforms.

## Rules (AGENTS.md applies)

- Translation quality: natural, consistent terminology per language (one glossary per language
  for game terms: unit types, ranks, UI verbs), keep placeholders/format codes and markup
  exactly, no truncation. Military and historical terms follow the language's usual usage.
- Never write into shipped `Data/` for tests; the language mods are generated build outputs or
  tracked under a new folder agreed in planning, not edits of the original English data.
- CRLF for all text files, conventional commits with a scope, no push (the maintainer pushes).
- A full sweep longer than 10 minutes is left to the maintainer, as in AGENTS.md.
