# mod-roundtrip fixture

A tiny stand-in for a third-party mod's `data` folder, for `zig build test-resource-mod-roundtrip`.
Every file is a copy of a tracked file below `Data/`, lower-cased path as a mod would have it; no file of
a real third-party mod is ever copied into the repository. The tier runs over this folder first, then over
`BK_MOD_ROOT` (or `-Dmod-root`) when one is given.
