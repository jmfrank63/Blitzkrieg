# Bundled rclone — size, licence and packaging

Packet `P00-M03` of `2026-08-21-cloud-provider-coverage`. Host: macOS 26.5.2
(Darwin 25.5.0), aarch64, Zig 0.16.0. Native target `aarch64-macos`; the other
four targets are cross and are reported from the P00-M01 fetch, which is where
they were measured.

## Sizes

rclone v1.75.0, one official `downloads.rclone.org` archive per target triple.
"Fetched" is the zip Zig downloads and hashes; "installed" is the extracted
executable that ends up beside the game.

| Target | Archive | Fetched | Installed |
|---|---|---|---|
| `aarch64-macos` | `rclone-v1.75.0-osx-arm64.zip` | 32,473,219 B (32.5 MB) | 88,387,298 B (88.4 MB) |
| `x86_64-macos` | `rclone-v1.75.0-osx-amd64.zip` | 34,885,643 B (34.9 MB) | 95,506,608 B (95.5 MB) |
| `x86_64-linux-gnu` | `rclone-v1.75.0-linux-amd64.zip` | 31,456,234 B (31.5 MB) | 85,323,938 B (85.3 MB) |
| `aarch64-linux-gnu` | `rclone-v1.75.0-linux-arm64.zip` | 28,620,931 B (28.6 MB) | 78,315,682 B (78.3 MB) |
| `x86_64-windows-msvc` / `-gnu` | `rclone-v1.75.0-windows-amd64.zip` | 31,472,895 B (31.5 MB) | 85,160,448 B (85.2 MB) |

The packet text quoted 31.0 / 84.3 MB for macOS arm64. The measured figures
above supersede it; both Windows ABI flavours share the one x64 archive.

## Licence

rclone is **MIT**, so redistributing the binary is permitted, and the licence
obliges the copyright line and permission notice to accompany every copy we
ship. The official archive holds exactly `rclone`, `rclone.1`, `README.html`,
`README.txt` and `git-log.txt` — verified against the fetched
`aarch64-macos` package — so there is **no `COPYING` to stage**, and the MIT
text exists only inside `README.txt`.

The notice is therefore a file we own and review, not a build-time scrape of
upstream prose that could silently stop matching:

- in the repository: `Data/THIRD-PARTY-NOTICES.txt`
- in the staged and packaged layout: `THIRD-PARTY-NOTICES.txt`, at the root
  beside `LICENSE.md` and beside the `rclone` binary it covers

The root is the right place for it: it sits with the licence a player already
looks for, it stays present under `--link-data` (which replaces the staged
`Data` tree with a link into the repository), and it cannot be mistaken for
game content. The copy under `Data/` travels with the Data tree as well, which
is harmless.

The 19 licence lines were copied verbatim out of the "License" section of the
archive's `README.txt` and diffed against it byte for byte after writing.
`Copyright (C) 2019 by Nick Craig-Wood https://www.craig-wood.com/nick/`.

Staging pairs the binary and the notice so neither can ship without the other:
`stage.zig` verifies the staged payload at the end of the run that produces it,
and `verify_runtime.zig` requires `bundledToolName(target)` (`rclone`, or
`rclone.exe` on Windows) and `THIRD-PARTY-NOTICES.txt` in the same layout check
that requires the game and its runtime set.

## Signing

**Out of scope, and bundling does not change that.** rclone's official macOS
binary is ad-hoc (linker-)signed and `spctl --assess` rejects it — the same
state as the game's own `Game` binary. Both are ad-hoc signed, which is what
lets an arm64 binary execute at all, and neither is notarized. The game already
ships that way, so the bundled binary adds no new Gatekeeper condition. No
signature is asserted anywhere in this evidence; there is no identity to sign
with, and a gate an unsigned development build cannot close is a gate that
stops the plan. `2026-08-02-linux-macos-platform-port/README.md` excludes
installers, signing and notarization, and that still holds.

Conditional constraint, recorded for whoever adopts Developer ID signing later
because it is easy to get wrong once and hard to notice: **a signed app requires
every nested Mach-O to be signed too, so the bundled `rclone` must be signed
before the archive is built.** Signing after archiving signs nothing. Staging it
as an ordinary file keeps that path open; nothing in this packet forecloses it.

## Commands and results

```text
$ zig build install-game -Dtarget=aarch64-macos --release=fast
(exit 0)

$ ls -l zig-out/game/macos/arm64/release/rclone \
        zig-out/game/macos/arm64/release/THIRD-PARTY-NOTICES.txt
-rwxr-xr-x  1 johannes  staff  88387298 Aug 21 18:29 .../rclone
-rw-r--r--  1 johannes  staff      2389 Aug 21 18:29 .../THIRD-PARTY-NOTICES.txt

$ ./zig-out/game/macos/arm64/release/rclone version
rclone v1.75.0
- os/version: darwin 26.5.2 (64 bit)
- os/kernel: 25.5.0 (arm64)

$ zig build verify-runtime -Dtarget=aarch64-macos -Dtest-mode=run --summary all
Build Summary: 3/3 steps succeeded; 11/11 tests passed

$ zig build test-cloudsync-daemon -Dtarget=aarch64-macos -Dtest-mode=run --summary all
Build Summary: 3/3 steps succeeded; 27/27 tests passed

$ zig build test-streamio -Dtarget=aarch64-macos -Dtest-mode=run --summary all
Build Summary: 3/3 steps succeeded; 32/32 tests passed
```

Mutation check on the new assertion — the notices copy removed from
`stage.zig`, the package tree deleted, `package-game` rerun:

```text
stage: staged layout is missing 'THIRD-PARTY-NOTICES.txt': FileNotFound
stage: step 'verify staged payload' failed: MissingStagedFile
error: MissingStagedFile
+- run exe stage-game failure
```

The same check, run with the two policy lines removed from
`verify_runtime.zig`, fails the new unit test rather than silently passing:
`expected error.MissingBundledTool, found void`.

## Packaged layout

`package-game` stages the tree the zip is built from and then zips it. The
staged tree contains both files, with the binary still executable:

```text
-rwxr-xr-x  1 johannes  staff  88387298  zig-out/game/macos/arm64/release/package/rclone
-rw-r--r--  1 johannes  staff      2389  zig-out/game/macos/arm64/release/package/THIRD-PARTY-NOTICES.txt
```

63,728 files, 2,863,804,234 bytes of content.

### Determinism

Three independent staging runs — the second and third after deleting the
package tree outright — produce a byte-identical package:

```text
cbd57eb6c7c7baa7d274bb8b7331f1e6ad4a0f29713a09e677bc1a1596fe1d45  pkg-a.zip  (2,876,018,942 B)
cbd57eb6c7c7baa7d274bb8b7331f1e6ad4a0f29713a09e677bc1a1596fe1d45  pkg-b.zip  (2,876,018,942 B)
cbd57eb6c7c7baa7d274bb8b7331f1e6ad4a0f29713a09e677bc1a1596fe1d45  pkg-c.zip  (2,876,018,942 B)
```

The zip carries `rclone` (88,387,298 B) and `THIRD-PARTY-NOTICES.txt` at the
root, and no timestamps — every entry is dated 1980-00-00 — which is what makes
the hash reproducible. The two archives were produced by an out-of-tree copy of
`tools/zig/package.zig`; see the second defect below for why the in-tree tool
could not produce them on this host, and note that the copy differs only in when
it opens each file, not in a byte it writes.

## Defects found, and what was done about each

1. **`package-game` could never run on macOS.** The zip was built from
   `<stage_root>/game`, and on a case-insensitive filesystem that path *is* the
   staged `Game` executable sitting in the same directory, so every run died in
   `stage-game` with `error: NotDir` before copying a byte. Reproduced on the
   unmodified tree by stashing. Fixed here, in `build.zig`, by naming the
   directory `package`.

2. **The zip tool exhausts the process file-descriptor quota.**
   `tools/zig/package.zig` opens every entry during its walk and holds the
   handle until the archive is finished: 63,728 open files against a
   `kern.maxfilesperproc` of 61,440, so it dies with
   `error: ProcessFdQuotaExceeded`. Raising `ulimit -n` cannot help — the file
   count exceeds the per-process hard maximum. `package.zig` is outside this
   packet's allowlist, so the fix is deferred; it is a small one (open each
   entry in the write loop and close it again, as the out-of-tree copy used for
   the determinism figures above does). **Until that lands, `zig build
   package-game` fails on this host at the zip step, after staging succeeds**,
   leaving a zero-byte `zig-out/packages/macos/arm64/release/Blitzkrieg-game.zip`
   behind — an empty file is not a package, and nothing downstream should treat
   its presence as success.

3. **The zip does not carry the executable bit.** `package.zig` writes
   version-made-by as MS-DOS and external file attributes as zero, so a file
   staged `0755` extracts as `0644`; measured directly on a two-file fixture.
   This is pre-existing and applies to `Game` and every `.dylib` as much as to
   `rclone`, so it is not a condition bundling introduced — but for rclone it
   has a specific consequence: discovery finds a non-executable neighbour and
   rejects it as `.not_executable`, so a player installing from the zip on
   macOS or Linux would get "no rclone" until the bit is restored. Same file,
   same allowlist, same deferral as (2).

Items (2) and (3) both belong to `tools/zig/package.zig` and should be one
follow-up packet, which would also let `package-game` produce the hashes above
itself rather than through a copy of the tool.

## Not covered

Windows and Linux packaging remain unverified on this host: the Linux cross
build fails compiling the engine's C++ (`'stdio.h' file not found`, no libc
headers for that target here), and there is no Windows host in this session.
The staging and layout policy are platform-parametrised and unit-tested for all
three targets; only the end-to-end run is macOS-only.
