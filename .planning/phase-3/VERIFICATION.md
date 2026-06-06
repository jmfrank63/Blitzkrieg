# Phase 3 verification

## Verification summary

- Confirmed the repo ignores `Versions/` and `**/Data`, so missing installed video assets are not present in a clean clone.
- Confirmed the runtime uses `movies\*.bik` asset names and a `movies\` folder structure for video playback.
- Confirmed there is no existing archive/extraction helper for movie assets in the current repo.

## Verification checklist

- [ ] Track compressed video archive volumes in a repo-safe location.
- [ ] Compression format chosen and documented (e.g. `7z` LZMA2 or `zstd`).
- [ ] Archive splitting strategy documented for GitHub-safe file sizes.
- [ ] `install_videos.ps1` or equivalent script added and tested on a clean clone.
- [ ] Extracted `movies\*.bik` files land under the runtime data tree used by the game.
- [ ] Game launch with extracted movies succeeds, specifically the intro/cutscene path.
- [ ] `README.md` updated with clone + install + run instructions.

## Outstanding verification actions

- Run the extraction script on a clean clone and confirm the archive reassembles successfully.
- Validate that all expected movie file names are present after extraction.
- Start the game and confirm the `movies\intro` cutscene loads without missing asset errors.
- Verify that repo-tracked archives are excluded from the raw `Data/` extraction destination and do not cause clone-size explosion.

## Conclusion

Phase 3 will be complete when the repository includes a tracked compressed video package, a clean extraction/install workflow, and a verified runtime path that ensures all original videos are present when the game runs from a GitHub clone.
