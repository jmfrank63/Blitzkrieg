# Blitzkrieg Vendor Record: HarfBuzz

Source:

- `src/`, `COPYING`: https://github.com/harfbuzz/harfbuzz, release `11.2.1`, from the release tarball
  `harfbuzz-11.2.1.tar.xz` (SHA-256 `093714c8548a285094685f0bdc999e202d666b59eeb3df2ff921ab68b8336a49`,
  https://github.com/harfbuzz/harfbuzz/releases/download/11.2.1/harfbuzz-11.2.1.tar.xz).
  Kept: the `.cc`, `.hh` and `.h` files of `src/` (including `OT/` and `graph/`), which is what the amalgamation
  `src/harfbuzz.cc` includes. Left out: the Rust, WASM and MS-USE folders, the subsetter's sources (only its headers stay, the shaping headers include them;
  `harfbuzz-subset.cc` is gone), the GObject glue, the build scripts, the Python helpers and the data tables' sources.

Licence: Old MIT (`COPYING`), reproduced in `Data/THIRD-PARTY-NOTICES.txt`.

Local integration patches: none. The files are byte-exact upstream (LF line endings): `.gitattributes` marks
`vendor/harfbuzz/**` as `-text`, so a checkout does not convert them to CRLF.

Built by `zig build` as a static library (`addHarfBuzz` in `build.zig`): only `src/harfbuzz.cc`, with no FreeType, ICU
or glib, so the built-in OpenType shaper and Unicode data (`hb-ucd`) do all the work, and the platform shapers
(CoreText, DirectWrite, Uniscribe) stay off because their `HAVE_*` macros are not defined. Used by
`Sources/src/FontAtlas/Shaper.cpp` (`BkShapeClusters`) for the Hindi shaping of the language mods
(M002 D058, D059).
