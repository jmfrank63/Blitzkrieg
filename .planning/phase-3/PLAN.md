# Phase 3 plan: Compressed videos, install-time extraction, and full game runtime support

## Objective

Enable the Blitzkrieg repository to include all original game videos in a GitHub-safe compressed form, and provide an installation workflow that expands those videos so the game runs fully from any clone.

## Success criteria

- The repository contains tracked compressed video archives covering every original movie asset needed by the game.
- The compressed asset set is GitHub-friendly: strong compression and/or split volumes to avoid oversized files.
- A bootstrap extraction script is available and works on a clean clone.
- After extraction, the game can start and load movies from the extracted runtime data tree.
- Raw extracted video folders remain gitignored; only compressed archives are tracked.

## Tasks

1. Audit missing assets and runtime requirements
   - Confirm that `Versions/Current` and the full `Data/` install tree are ignored by `.gitignore`.
   - Identify the exact movie path and file name conventions used by the runtime (`movies\intro.bik`, `_l.bik`, etc.).
   - Inventory the original video assets from the upstream installed copy and map them to required runtime files.

2. Design a GitHub-friendly compressed asset package
   - Choose a compression format with strong ratios for video: `7z` (LZMA2) or `zstd`.
   - If needed, split the archive into GitHub-safe volumes (e.g. `VideoAssets.7z.001`, `VideoAssets.7z.002`, ...).
   - Select a tracked location for archives that is not excluded by current `.gitignore`, such as `Data/CompressedMovies/` or `Assets/VideoArchive/`.
   - Define a checksum manifest for archive integrity.

3. Add install-time extraction support
   - Create a bootstrap script (`install_videos.ps1` or `scripts/install_videos.ps1`) that:
     - Reassembles split archive volumes if necessary.
     - Extracts all video assets into the runtime data tree.
     - Validates archive and extracted file checksums.
     - Preserves the required `movies\` folder structure.
   - Ensure the script is repeatable and safe when run multiple times.

4. Add runtime verification hooks
   - Add a fast verification step to confirm that all expected `.bik` files are present after extraction.
   - If the game supports a `-movie` or `MovieDir` override, document how to use it for testing.
   - If helpful, add a small `verify_video_assets.ps1` script that checks the extracted folder.

5. Document the workflow
   - Update `README.md` with the new clone/install flow:
     1. `git clone ...`
     2. run `install_videos.ps1`
     3. build and run `Sources/src/Game/Debug/Game.exe`
   - Document archive location, folder structure, and expected runtime path.

## Deliverables

- `.planning/phase-3/RESEARCH.md`
- `.planning/phase-3/PLAN.md`
- `.planning/phase-3/VERIFICATION.md`
- `install_videos.ps1` or equivalent extraction helper (planned)
- Optional `.gitignore` exception plan for compressed archives
- Readme instructions for install-time decompression
