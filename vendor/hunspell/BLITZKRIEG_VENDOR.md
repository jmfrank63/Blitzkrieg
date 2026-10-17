# Blitzkrieg Vendor Record: Hunspell

Source:

- `src/hunspell/`, `COPYING`, `COPYING.LESSER`, `COPYING.MPL`, `license.hunspell`, `license.myspell`, `README`:
  https://github.com/hunspell/hunspell, release `v1.7.2`, commit `2969be996acad84b91ab3875b1816636fe61a40e`,
  from the release tarball `hunspell-1.7.2.tar.gz`
  (SHA-256 `11ddfa39afe28c28539fe65fc4f1592d410c1e9b6dd7d8a91ca25d85e9ec65b8`).
  Only the library is kept: the tools, tests, build scripts and translations are left out.
  `hunvisapi.h` is the one the tarball's configure generated.

Licence: tri-licensed MPL 1.1 / GPL 2.0 or later / LGPL 2.1 or later (`license.hunspell`, `license.myspell`).
ELK uses it under the MPL 1.1 terms, unchanged and as source in this folder.

Local integration patches: none. Line endings were converted to CRLF (the repository's `.gitattributes`).

Built by `zig build` as a static library (`addHunspell` in `build.zig`, with `HUNSPELL_STATIC`) and linked
into ELK and the `test-elk-app` tier, which call its C API (`hunspell.h`) from
`Sources/editor/elk/app/hunspell.zig`. Used by ELK's spell check
(`docs/superpowers/specs/2026-09-30-portable-elk-design.md`, D-11).
