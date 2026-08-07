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
- `zig test tools/zig/verify_runtime.zig` passes 6/6 tests. The verifier now
  covers target runtime matrices, required shaders/configs, duplicate or
  foreign PlatformRuntime names, unsafe manifest paths, and cache/temp/user
  write artifacts.
- Existing native Linux evidence remains: `game-all` 113/113 and 11/11,
  `install-game` 159/159 and 6/6, and one Linux package hash recorded in
  `p08-m03-linux-link.md`.

## Remaining P08-M06 gates

- Exact all-target manifests still need explicit Data/metadata/license policy.
- Staging through paths containing spaces/non-ASCII names needs an integration
  fixture; pure helper coverage is not sufficient.
- Architecture/export/dependency/rpath/install-name and executable-bit checks
  are still native binary/CI work, and the generic verifier is not yet wired
  into the build graph.
- macOS arm64 package/bundle evidence and all-target CI package runs remain
  open.
