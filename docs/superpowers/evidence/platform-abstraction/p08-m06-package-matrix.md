# P08-M06 stage/package matrix evidence

## Closed evidence

- `tools/zig/package.zig` now preserves source file handles for deep paths,
  sorts normalized archive names before writing local and central ZIP records,
  preserves literal POSIX backslashes, and uses explicit positional reads.
- The package worker verified a 102-entry archive for ordering, readability,
  and two-run byte determinism.
- Full Windows x64 `package-game` was run twice from the detached worktree at
  `566285eba`, with both runs producing SHA-256
  `73453868F3D57542789722314CAB11F2AB02D565EC9FF6944CA308F1A0E46446`.
- `zig build verify-runtime -Dtarget=x86_64-windows-msvc -Dtest-mode=run`
  passes 10/10 tests through the native build graph. The verifier now
  covers target runtime matrices, required shaders/configs, duplicate or
  foreign PlatformRuntime names, exact staged-root traversal, unsafe manifest
  paths, duplicate entries, and cache/temp/user-write artifacts.
- `zig test tools/zig/stage_test.zig` passes 18/18 tests, including the
  spaces/non-ASCII destination fixture, canonical metadata, and forbidden
  artifact exclusion.
- Windows x64 `install-game`, `verify-runtime`, and `package-game` completed
  from the native build graph. The package artifact was emitted at
  `zig-out/packages/windows-x64/Blitzkrieg-game.zip` with size
  `3,073,345,577` bytes.
- Existing native Linux evidence remains: `game-all` 113/113 and 11/11,
  `install-game` 159/159 and 6/6, and one Linux package hash recorded in
  `p08-m03-linux-link.md`.

## Remaining P08-M06 gates

- Exact all-target manifests still need native Data/metadata/license policy
  confirmation; Windows now stages the canonical `LICENSE.md` and `README.md`.
- Staging through paths containing spaces/non-ASCII names is now covered by an
  integration fixture; native package execution for all targets remains open.
- Architecture/export/dependency/rpath/install-name and executable-bit checks
  are still native binary/CI work; `verify-runtime` is now wired into the
  build graph.
- macOS arm64 package/bundle evidence and all-target CI package runs remain
  open.
