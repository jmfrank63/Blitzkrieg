# Phase 01 — Sync Planning Primitives

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Turn a profile directory into safe bisync parameters. Every packet here is pure logic, testable with no network.

| Packet | Depends on | Owns |
|---|---|---|
| P01-M01 | P00-M04 | short link creation and repointing |
| P01-M02 | M01 | session-name budget validation |
| P01-M03 | M02 | filter rules, machine-local state paths, and the sentinel |
| P01-M04 | M03 | bisync parameter construction |

Exit: a profile directory at a pathological depth yields a short, budget-checked, correctly defaulted parameter set offline.

P01-M01 macOS checkpoint: `zig build test-cloudsync-plan -Dtarget=aarch64-macos
-Dtest-mode=run` passes 15/15; `x86_64-linux-gnu` and `x86_64-windows-gnu`
compile. All phase-00 steps and `test-streamio` still pass. Nothing leaks into
`~/Library/Caches/blitzkrieg` — the link root is injected the way `daemon.zig`
injects `Search`. Commit `c651b195a`.

The gap the Windows probe left is closed here: a real file crosses the link in
both directions, with a `statFile` at the target proving it is the same file
rather than a copy. Both settled facts are re-asserted as regressions — the link
method must not be `.symlink` on Windows (junctions need no elevation, so a
symlink there would be a bug), and the profile name must appear in the link's
*target* but never in its *path*, which is the byte string rclone mangles.

Absoluteness is enforced in exactly one place: every path into a link goes
through `canonicalDir`, which stats and then resolves, so the target reaching
`symLink`, the reparse buffer and `mklink` is always absolute. That matters
because `mklink /J` silently resolves a relative target against the current
directory.

Carried forward:

- **`std.posix.symlink` does not exist in Zig 0.16** — it is
  `Io.Dir.symLink(io, target, path, .{ .is_directory = true })`. Fourth entry in
  the same family as `std.crypto.random`, `std.Thread.Mutex` and `std.net.Server`.
- `Io.Dir.cwd().realPath` cannot resolve `AT_FDCWD`, so a relative path must be
  joined against an explicitly opened directory before resolving.
- `std.os.windows.kernel32` exports only `CreateProcessW` here, so
  `CreateFileW`/`CreateDirectoryW`/`RemoveDirectoryW`/`DeviceIoControl` are
  file-scope externs, as `daemon.zig` already does for `OpenProcess`.
- `ensureShortLink` returns a `ShortLink{ path, target, slot, method }` rather
  than the packet's `![]u8`: a slice cannot also record which creation path was
  taken, and `method == null` distinguishes a reused slot from a created one.
- ~~The junction path is semantically analyzed for `x86_64-windows-gnu` (proved
  by injecting a type error only that target rejects) but no junction has ever
  been created by this code. P08-M02 owns that.~~ **Closed during P01-M02**, run
  on a real Windows 11 machine: all 15 P01-M01 tests pass natively on
  `x86_64-windows-msvc` from an unelevated shell, so `createJunction` made real
  junctions without administrator rights, a file crossed them in both
  directions, and repoint/reclaim ran against real reparse points. P08-M02
  still owns the shipped-build confirmation.

P01-M02 Windows checkpoint: `zig build test-cloudsync-plan -Dtest-mode=run`
passes 21/21 natively on `x86_64-windows-msvc` — the target macOS could not
even configure. `x86_64-linux-gnu`, `aarch64-linux-gnu` and
`x86_64-windows-gnu` compile. All other suites re-verified natively the same
day: rc 6/6, daemon 22/22 (including the three live cases against a real
rclone v1.75.0 with no orphan left), abi 9/9 plus the C++ consumer, streamio
32/32. Commit `5c3137782`.

Carried forward from P01-M02:

- The session-name arithmetic is platform-invariant on purpose:
  `canonicalPath` folds `\` and `/` alike to `_`, so the measured pairs
  (`C__bk_p0..C__bk_remote`, `tmp_bkp..tmp_bkremote`) assert the same bytes on
  every OS and the suite needs no per-platform expectations.
- The packet's `SESSION_SUFFIX_MAX`/`SESSION_BUDGET` are `session_suffix_max`/
  `session_budget` — this file's constant style (`max_slots`,
  `mklink_timeout_ms`).
- "An error carrying the projected length" is expressed as an out-parameter
  written on success and failure both, because a Zig error carries no payload.
  P01-M04 should surface that number, not the error name.
- A named remote endpoint is passed to `fsPath` as the combined `name:root`
  string; there is no separate name/root pair in the API. P01-M04 constructs
  it that way.
- `aarch64-macos` cannot be configured from Windows — same SDL `--sysroot`
  refusal as `x86_64-macos` from the arm Mac, pre-existing and symmetric.
  macOS re-verification of phases 00–01 happens when the branch next sits on
  a Mac; nothing in P01-M02 is platform-conditional beyond `fsPath`'s
  separator choice, which is covered by the windows-gnu compile.

P01-M03 Windows checkpoint: `zig build test-cloudsync-plan -Dtest-mode=run`
passes 28/28 natively on `x86_64-windows-msvc`; `x86_64-linux-gnu`,
`aarch64-linux-gnu` and `x86_64-windows-gnu` compile; rc, daemon, abi and
streamio all unaffected. Commit `9b3e9f1a4`.

Carried forward from P01-M03:

- `stateRoot` takes `game_dir` as a parameter rather than the packet's
  `(allocator)`-only signature — discovery of the game directory belongs to
  the ABI layer, exactly as `daemon.zig` takes `Options.game_dir`, and tests
  inject a fixture. `writeFiltersFile` likewise takes `(io, path)` and no
  allocator: the content is a comptime constant, which is what makes rewriting
  idempotent under bisync's filters-changed MD5 check.
- `ensureSentinel` returns a `SentinelAction`
  (`already_present`/`written`/`deferred_to_remote`) instead of the packet's
  `!void`, widened the way `ensureShortLink`'s return was: the pairing report
  should say whether this machine seeded the profile or is waiting for the
  resync to deliver the sentinel. P02-M01 supplies `remote_has_sentinel` by
  asking the remote; nothing in this module does network.
- `plan.state_dir_name` is textually identical to `daemon.state_dir_name`
  ("cloudsync") but deliberately not imported — the planning module must not
  pull the daemon's process machinery into its compile. If either constant
  ever changes, change both.
- The sentinel's content is `<profile_id>\n`. Its *content* is never read
  back and never compared — existence alone short-circuits, because the
  unchanged modification time is the `foundSame` guarantee, and that holds
  even when a later call passes a different id.
- `max_delete_percent` and `deleteWithinRatio` live here, not in P01-M04's
  parameter builder, because the rc API defaults `maxDelete` to zero (the
  CLI's 50 is applied in `cmd.go`, which rc never runs) — every run must send
  it explicitly, and the design-table test `single_delete_passes_with_sentinel`
  pins the `<=` boundary the sentinel arranges.

P01-M04 Windows checkpoint: `zig build test-cloudsync-plan -Dtest-mode=run`
passes 35/35 natively on `x86_64-windows-msvc`; the three cross-targets
compile; rc, daemon, abi and streamio unaffected. Commit `01f2d71ea`.

Carried forward from P01-M04:

- `bisyncParams` returns an arena-owned `BisyncParams{arena, value}` rather
  than a bare `std.json.Value` — the object's strings are allocations, and a
  bare Value has no owner to free them. `params.value` feeds
  `rc.Client.callAsync` directly; note `callAsync`'s `encodeParams` also puts
  `_async: true`, so the builder's copy is redundant-but-harmless on that
  path and load-bearing on `call`.
- The session budget check runs inside `bisyncParams` on every call. P02-M02's
  worker does not need its own check; the surface that shows the number to
  the player calls `sessionName` itself.
- `runId(gpa, io)` produces `YYYYMMDDTHHMMSSZ-<8 hex>` — 25 bytes, colon-free
  because Windows refuses colons in filenames. Wall-clock time in Zig 0.16 is
  `Io.Clock.now(.real, io).toSeconds()` (there is no `std.time.timestamp`
  anymore — fifth entry in the `std.crypto.random` family) fed through
  `std.time.epoch`; randomness is the established
  `io.randomSecure(&b) catch io.random(&b)`.
- `SyncContext.mode` is how "resync only on first pairing" is expressed: the
  builder never reads pairing state itself. P02-M01 owns setting `.pairing`
  from `<stateRoot>/state/<profile>.json` and flipping it after success.

**Phase 01 exit: met.** A profile directory at a pathological depth yields a
short, budget-checked, correctly defaulted parameter set offline: the short
link bounds Path1, the budget check refuses what bisync would abort on, the
filter set and state paths keep machine-local data out of the sync set, the
sentinel arms both bisync guards, and `bisyncParams` emits the correct
object for pairing and steady state. All run-verified on
`x86_64-windows-msvc`; macOS holds at P01-M01 until the branch next sits on
a Mac.

