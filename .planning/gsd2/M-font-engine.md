# Milestone brief: a TrueType text engine for the game (UTF-16 kept)

Johannes, 2026-10-22: "We need to replace this mess of fonts ... make this change to keep UTF-16
but with a proper modern TTF engine. In zig, of course." and "There are so many bugs for now in
languages that we first want the engine fixed, then fix the bugs, otherwise we fix twice." So this
milestone runs now: M003 (game database translation) is parked at S07/T03 and resumes on top of the
new engine, and this milestone takes over M003's final all-ten-languages acceptance (its S10). Do
not chase language bugs that the engine will change (fit findings, glyph coverage, the Latin
stopgap fonts); fix the engine, then the bugs. Branch `feat/font-engine` (fix/latin-fonts plus
feat/localization); the maintainer merges with `--no-ff`. Verify locally (Linux) while the bitmap
fonts are still the default; the machines and CI come once the engine draws the game.

## Why

The game draws text from pre-baked bitmap fonts: `Data/Fonts/{tiny,small,medium,large}/1.tfd`
plus a `1_h.dds` atlas, made by Nival's FontGen from Times New Roman for one Windows code page
(bytes 32..255). The shipped set is charset 204 (cp1251): 225 characters, ASCII and Cyrillic, no
accented Latin letter. Each 2003 retail edition shipped its own font folder for its code page; the
port has only this one. Found on 2026-10-21/22:

- The five Latin mods drew every accent as the default character, a hollow box, on every platform,
  and no tier noticed (the solid-box measure misses a hollow box). Fixed for now on
  `fix/latin-fonts`: bk-font `--base` keeps the shipped glyphs and adds the missing letters from
  Liberation Serif; `BK_GLYPH_TRACE` makes the Game report a missing glyph and the tiers fail on it.
- Russian, Chinese, Japanese and Hindi ship atlases bk-font generates from Noto; Hindi needs a
  private-use cluster table (D058) because the engine cannot shape. Every new character means a
  rebuilt atlas; a text measured by l10n-check is measured with metrics copied from the atlas, not
  with the code that draws it.
- With texture quality "compressed" or "low" a language mod's font was drawn from the base game's
  `1_c`/`1_l` atlas (fixed on `fix/latin-fonts`: fonts take `1_h.dds` by name).
- Measured with the game's own Times Cyrillic, about 36 Russian texts (unit names, a few labels)
  overflow their rooms; the M002/M003 check measured Russian with the wider Noto Sans atlas and
  never saw them (`zig build language-mods -Dlm-langs=ru` with `keeps_game_fonts` set lists them).

## Decisions already made

1. **UTF-16 stays.** Texts on disk (Data/Textes, ELK's database, the mods' paks), `IText`, the
   save games and every engine API keep UTF-16. The text engine takes UTF-16, decodes surrogate
   pairs to code points and works in code points (or UTF-8) inside.
2. **A Zig text engine** (working name `bk_text`) behind a C ABI the C++ engine calls, as the
   editors talk to the engine through `Sources/src/EditorBridge`. It owns: loading TTF/OTF faces;
   a fallback chain per language; shaping with the vendored HarfBuzz (every script, so Hindi
   conjuncts are real shaping and the PUA cluster path of D058 goes away); rasterising
   (stb_truetype as vendored with dcimgui, or a Zig rasteriser if it measures better) into a glyph
   cache that fills a GPU atlas on demand; line breaking (word wrap as `GFX/Text.cpp` PreFormat and
   `GFXGPU/TextWrap.h` do now, the kinsoku rules of D065, UAX #14 where it matters); measuring.
3. **The game's look:** Liberation Serif (vendored on `fix/latin-fonts`, SIL OFL 1.1) is the
   primary face; its advance widths are Times New Roman's, the face of the 2003 bitmaps. Then Noto
   Sans, Noto Sans SC or JP (Japanese puts JP before SC, everything else SC before JP, as
   `Sources/editor/kit/fonts.zig` does), Noto Sans Devanagari. Sizes: the layouts' `FontSize`
   (0..3: tiny, small, medium, large; 364 x 1 and 51 x 2 in Data/UI) map to the shipped cell heights
   10, 16, 20, 40 at 1024x768, scaled with the UI's own scale so text is rendered at its real pixel
   size (sharp at any resolution). Before the engine becomes the default, show the maintainer
   captured frames of the main menu, a briefing, the encyclopedia and the options screen, old and
   new side by side.
4. **The engine side:** `FontGpu` and `TextGpu` (GFXGPU) implement `IGFXFont` / `IGFXText` on
   `bk_text`; the code above them (UI, Scene, GameTT, about 14 files) does not change. Save games
   stay readable: the font objects serialise what they serialise now.
5. **One measure:** l10n-check's fit rule (D097) and language-mods measure with `bk_text` itself,
   the code the Game draws with. The fit rule then needs no generated fonts and runs in the cheap
   l10n-check job again.
6. **What goes away:** bk-font in the language-mods build, the per-language atlases, the Hindi
   cluster tables and the shaper hook in ELK's export, the `--base`/`--fallback-ttf` stopgaps for
   the game. Language mods become texts only. bk-font itself stays as a modding tool.
7. **Old mods (default decision, the maintainer may change it):** a mod that ships its own bitmap
   font (`fonts/<size>/1.tfd`) keeps it: the bitmap path stays as the fallback for exactly that
   case, and only for it.

## Scope

- `bk_text` with unit tests: shaping (Latin, Cyrillic, Han with regional forms, Devanagari
  conjuncts and matras), fallback, metrics, wrap and kinsoku, the cache and its eviction, surrogate
  pairs, a missing glyph after the whole chain reported (the `BK_GLYPH_TRACE` line stays).
- The engine adapter, the texture path for the glyph atlas through the SDL GPU backend, and the
  draw path of `DrawText`, `DrawString` and `DrawStringA`.
- Every place that measures text (`GetTextWidth`, `GetNumLines`, the wrap shared with
  `TextGpu::GetNumLines`, about 21 call sites) gives the numbers the drawing uses.
- The fit rule on `bk_text`; the Russian overflows above fixed by rewording (D097: reword first;
  an engine rule only where no natural wording fits, applied to all languages alike).
- The l10n Game tiers on the new engine: hb-view stays the independent reference (a different
  rasteriser, the same face), the bitmap-only measures and the PUA checks go, the glyph trace stays.
- The gfx reference captures that hold text re-captured and re-measured by code.
- M003 S10, moved here: for each of the ten languages the installed Game draws the main menu, an
  options screen with a tooltip, a unit's encyclopedia page and a briefing from every campaign,
  captured and measured by code, nothing overflowing; live language switch and restart.
- Performance: frame time on the most text-heavy screens (encyclopedia, options, briefing, the
  in-mission interface) no worse than the bitmap path, measured.

## Verification

Every slice runs its tiers on Linux, then on win-home, mac-office and macbook-air-frank through
`~/bk-verify/run-all.sh` before anything reaches CI (win-home has hb-view in
`D:\tools\harfbuzz-14.6.0`, so its Game tiers run, not only compile). CI green on every platform
at the end; existing Map Editor, Resource Editor, ELK and Game tiers stay green.
