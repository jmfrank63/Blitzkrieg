# P08-M03 Linux x64 link closure

Status: partial closure; the native `Game` link is closed, while the complete
`game-all -Dtest-mode=run` build wrapper and staged runtime gate remain open.

## Passed

- Native WSL build: `zig build game -Dtarget=x86_64-linux-gnu.2.39 -Dtest-mode=compile --verbose`.
- The final link installed `zig-out/bin/Game` with no unresolved native symbols.
- The link command used the native GNU C++ runtime and unwind runtime:
  `/usr/lib/x86_64-linux-gnu/libstdc++.so.6` and
  `/usr/lib/x86_64-linux-gnu/libgcc_s.so.1`.
- Direct native WSL audit: `zig test tools/zig/runtime_platform_audit_test.zig`
  passed all 11 tests with zero inventory hits and zero allowlist ownership
  entries.

## Fixes

- `HPTimer.cpp` now preserves the project-wide `long long` ABI for `int64`.
  On x86_64 Linux, using `std::int64_t` there changed the mangled symbols to
  `long` and caused the previous link failure.
- The Linux target build adds the concrete native C++ and GCC unwind runtimes
  only when the build host is Linux; the Windows target remains on its MSVC
  path.

## Remaining gate

`zig build game-all -Dtarget=x86_64-linux-gnu.2.39 -Dtest-mode=run` reached the
audit run artifact, printed the expected zero-hit result, and then timed out in
the Zig 0.16 WSL build-runner protocol (`--listen=-`). Running the same test
directly with `zig test` passes 11/11. A clean `install-game` attempt also
exceeded the local six-minute command window before the staged SDL runtime
could be launched. No Linux native runtime acceptance is claimed yet.

A subsequent native WSL retry against the shared checkout failed earlier with
`AccessDenied` while renaming `.zig-cache` compilation results because Windows
and WSL processes were using the same cache. Redirecting the top-level Zig
cache to `/tmp` did not isolate the run artifact: Zig 0.16 still passed the
repository-relative `.zig-cache` to its child test process. This confirms a
build-runner/cache-contending environment blocker, not a new Linux link error.

To separate cache contention from compile/runtime correctness, the same
`HEAD` was checked out into an ext4 WSL worktree at `/tmp/blitzkrieg-wsl`.
`zig build game-all -Dtarget=x86_64-linux-gnu.2.39 -Dtest-mode=run` then
compiled normally without `AccessDenied`, but exceeded the five-minute command
window while still compiling the native dependency graph. It did not reach the
audit/run artifact or staged runtime gate. The temporary worktree and its
process tree were removed after the bounded retry; no WSL build processes remain.

The isolated `/tmp/blitzkrieg-*` caches used for these checks are generated
artifacts and are not repository changes.
