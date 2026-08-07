# Next Packet

Resume at `phase-08-link-package-closure/P08-M03-linux-link-closure.md`.

P07-M01 through P07-M06 and P08-M01 are committed and pushed. The Windows
playable-source audit reports zero hits with an empty temporary allowlist, the
target policy now avoids MSVC paths and developer utilities for non-Windows
targets, and Windows `game-all` remains the authoritative local game-build
gate. PlatformRuntime is now one shared dynamic dependency in the playable
graph and is included in every target-specific stage manifest. The native
Linux `Game` link is closed and recorded in
`evidence/platform-abstraction/p08-m03-linux-link.md`; continue with the
remaining Linux `game-all` run/staging gate. On Windows, the shared runtime is
now staged by the Input, Net, and SFX module-test runners; Net, SFX, platform
foundation, GFXGPU, and Input gates pass. The remaining Windows closure work
and package gates now pass as well; the remaining Windows work is the x64
verifier (now narrowed to Game-loop termination), clean-cache evidence, and
full regression matrix. The Linux run gate
is currently limited by Zig 0.16 child-test cache handling when Windows and WSL
share the checkout; the native Linux link and direct audit remain passed.

## Important working-tree files

The intentional implementation checkpoints are the tracked files shown by
`git status --short`: source/build/docs changes only. Zig 0.16 local caches
(`b/`, `h/`, `z/`) and directly emitted `*-test.exe` files are ignored by
`.gitignore` and must not be staged. The untracked
`tools/zig/platform_display_contract_test.cpp` is pre-existing and is also
not part of this packet.
