# Next Packet

Resume at `phase-08-link-package-closure/P08-M03-linux-link-closure.md`.

P07-M01 through P07-M06 and P08-M01 are committed and pushed. The Windows
playable-source audit reports zero hits with an empty temporary allowlist, the
target policy now avoids MSVC paths and developer utilities for non-Windows
targets, and Windows `game-all` remains the authoritative local game-build
gate. PlatformRuntime is now one shared dynamic dependency in the playable
graph and is included in every target-specific stage manifest. The native
Linux `Game` link and full `game-all` run are closed and recorded in
`evidence/platform-abstraction/p08-m03-linux-link.md`; the clean Linux
game-all repeat is now recorded; continue with the
Linux desktop runtime and package acceptance. On Windows, the shared runtime is
now staged by the Input, Net, and SFX module-test runners; Net, SFX, platform
foundation, GFXGPU, and Input gates pass. The remaining Windows closure work
and package gates now pass as well; the x64 CDB and native Zig runtime
verifiers now pass after fixing duplicated module paths in the portable file
iterator. The clean-cache Windows `game-all` rerun also passes with an
isolated cache and install prefix. The remaining Windows work is the full
regression matrix and CI comparison. The Linux run gate now passes from a clean
isolated ext4 WSL worktree: the cold run took 857 seconds and the current-head
rerun took 6.4 seconds.
Both `game-all` runs report 113/113 steps succeeded and 11/11 tests passed;
`install-game` and `package-game` also pass
for Linux x64. The shared NTFS checkout still has Zig 0.16 cache rename
contention. Linux staged launch acceptance now has three clean WSLg startup
smokes, and Windows staged SDL_GPU acceptance reaches the C6 main-menu
checkpoint across two native restarts on the pushed head. Windows foundation
and GfxGPU/D3D12 gates also pass. The remaining
P09-M01 diagnostics, P09-M02 gameplay/UAT, and the broader P09-M04 regression
acceptance remain open. P08-M06 now records deterministic Windows package
hash evidence; its all-target manifest, native path, and macOS package gates
remain open.

## Important working-tree files

The intentional implementation checkpoints are the tracked files shown by
`git status --short`: source/build/docs changes only. Zig 0.16 local caches
(`b/`, `h/`, `z/`) and directly emitted `*-test.exe` files are ignored by
`.gitignore` and must not be staged. The untracked
`tools/zig/platform_display_contract_test.cpp` is pre-existing and is also
not part of this packet.
