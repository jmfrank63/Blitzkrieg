# P08-M03 Linux x64 link closure

Status: link and Linux `game-all` closure; staged desktop runtime acceptance
and packaging remain open.

## Passed

- Native WSL build: `zig build game -Dtarget=x86_64-linux-gnu.2.39 -Dtest-mode=compile --verbose`.
- The final link installed `zig-out/bin/Game` with no unresolved native symbols.
- The link command used the native GNU C++ runtime and unwind runtime:
  `/usr/lib/x86_64-linux-gnu/libstdc++.so.6` and
  `/usr/lib/x86_64-linux-gnu/libgcc_s.so.1`.
- Direct native WSL audit: `zig test tools/zig/runtime_platform_audit_test.zig`
  passed all 11 tests with zero inventory hits and zero allowlist ownership
  entries.
- Native ext4 WSL `zig build game-all -Dtarget=x86_64-linux-gnu.2.39
  -Dtest-mode=run --summary all` completed with `112/112` steps succeeded and
  `11/11` tests passed. The first post-fix run took 111 seconds; a warmed
  rerun completed in 3 seconds.
- The completed build installed the Linux `Game` executable and all playable
  shared modules, including `libGFXGPU.so`, `libAILogic.so`, `libAnim.so`,
  `libImage.so`, `libInput.so`, `libNet.so`, `libSFX.so`, and `libUI.so`.
- ELF inspection of the UI module shows one `libPlatformRuntime.so` dependency
  and only relative build-cache runpaths; no archive member contains a shared
  PlatformRuntime object.

## Fixes

- `HPTimer.cpp` now preserves the project-wide `long long` ABI for `int64`.
  On x86_64 Linux, using `std::int64_t` there changed the mangled symbols to
  `long` and caused the previous link failure.
- The Linux target build adds the concrete native C++ and GCC unwind runtimes
  only when the build host is Linux; the Windows target remains on its MSVC
  path.

## Remaining gate

The build-runner still prints its known Zig 0.16 `--listen=-` diagnostic for
the audit child, but the parent build exits successfully and reports all
112/112 steps and 11/11 tests passed. No Linux desktop runtime launch or
package acceptance is claimed yet.

A subsequent native WSL retry against the shared checkout failed earlier with
`AccessDenied` while renaming `.zig-cache` compilation results because Windows
and WSL processes were using the same cache. Redirecting the top-level Zig
cache to `/tmp` did not isolate the run artifact: Zig 0.16 still passed the
repository-relative `.zig-cache` to its child test process. This confirms a
build-runner/cache-contending environment blocker, not a new Linux link error.

To separate cache contention from compile/runtime correctness, the same
`HEAD` was checked out into the persistent ext4 WSL worktree at
`/home/jmfrank/blitzkrieg-wsl`. The post-fix run compiled normally without
`AccessDenied` and reached the complete `game-all` summary above.

The isolated `/tmp/blitzkrieg-*` caches used for these checks are generated
artifacts and are not repository changes.
