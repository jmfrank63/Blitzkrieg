# Phase 3 research: GitHub-friendly compressed video assets and install-time extraction

## Problem statement

The current repository is missing the original game videos required for a full runtime experience. Upstream original installations keep those movies under `Versions/Current`, but this path is ignored in the repo and therefore not present in a clean GitHub clone.

The original video package is roughly 1.7GB. To make the game runnable from a GitHub clone, the video assets should be stored in a compressed, repo-friendly form and expanded locally during installation.

## Relevant repository facts

- `.gitignore` currently excludes `Versions/` and `**/Data`, which means installed asset folders are intentionally omitted from version control.
- `Sources/autorun/autorun.pak` is tracked, confirming that binary asset containers can be stored in the repo.
- The game runtime expects movie assets in a `movies\` folder and plays `.bik` files such as `movies\intro.bik` and legacy `_l.bik` variants.
- `Sources/src/Game/main.cpp` makes the game launch the intro movie via `movies\intro`, and `CPlayMovieInterface` resolves video file names with `.bik` or `_l.bik`.
- The current build path uses a data junction from `Sources/src/Game/Debug/data` to `Sources/src/data`, so extracted assets must land in the runtime data tree accessible from the game executable.

## What must be true for success

- All original Bink video assets are present in the repository in some compressed form.
- The compressed package is small enough to be stored on GitHub reliably, ideally under 1GB total, with files split as needed to satisfy GitHub file limits.
- A local installation bootstrap step can decompress the packaged videos into the runtime data tree (`Data/` or `Sources/src/data/`) without manual asset copying.
- The game can launch and play the expected movies from the extracted location.

## Risk analysis

- GitHub file size limits mean a single 1.7GB archive is unsafe; a split archive or multi-volume approach is required.
- If the repo tracks raw extracted video files, repository size will explode and clone performance will suffer.
- `Data/` and `Versions/` are currently ignored, so tracked compressed packages must be located in a path that is not excluded by existing ignore rules, or `.gitignore` must be updated carefully.
- The game may require exact file names and folder structure. Asset extraction must preserve that structure exactly.

## Candidate approach

1. Store video assets as a compressed, Git-friendly archive set.
   - Use `7z` with LZMA2 or `zstd` to maximize compression.
   - Split into volumes under GitHub's 100MB file limit if necessary.
   - Place archives in a tracked folder such as `Data/CompressedMovies/` or `Assets/VideoArchive/`.

2. Add a bootstrap/install script.
   - A PowerShell script that reconstructs and extracts the archives into the runtime data folder.
   - Validate checksums after extraction.
   - Keep extracted raw data directories gitignored.

3. Ensure runtime discovery.
   - The game should find `movies\*.bik` files under the same data root used by the build and runtime.
   - If needed, add a `-movie` command-line or `MovieDir` fallback to point the game to the extracted folder.

4. Document the clone + install flow.
   - Clearly document the `git clone`, `install_videos.ps1`, and run steps in `README.md`.
   - Note the archive location, extraction destination, and verification commands.
