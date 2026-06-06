# Blitzkrieg Video Asset Packaging

This folder contains tooling and guidance for storing Blitzkrieg movie assets as compressed archives.

## Purpose

The game expects movie files under `Data/movies` at runtime. Because original video assets are too large for a normal GitHub repository, we store them as compressed 7z archives and provide a script to extract them into `VideoAssets/Movies`, then link them into the runtime data tree.

## Contents

- `create_video_archive.ps1`: create a compressed 7z archive from a source movie folder.
- `install_videos.ps1`: extract the archive into `VideoAssets/Movies` and create a local junction from `Data/movies` to the extracted files.

## Recommended Workflow

1. Place the original movie asset folder contents in a local source path, for example:
   - `C:\archive\BlitzkriegMovies\`
   - or an extracted installation path containing `*.bik` files and any subfolders.

2. Create a repository-friendly archive:
   ```powershell
   .\VideoAssets\create_video_archive.ps1 -SourcePath "C:\archive\BlitzkriegMovies" -OutputPath ".\VideoAssets\BlitzkriegVideos.7z"
   ```

   The archive should contain the movie files and subfolders directly, not an extra top-level `movies\` folder.

3. Commit the archive files into the repository.

4. After cloning the repository, install the videos:
   ```powershell
   .\install_videos.ps1
   ```

   This will extract the archive into `VideoAssets/Movies`.
   If `VideoAssets/Movies` exists, the build will automatically create a `Sources/src/data/Movies` junction so the game sees the files through the normal runtime data tree.

   If the archive is present and the extracted folder is missing, the build also now attempts to extract the archive automatically before creating the `Sources/src/data/Movies` junction.

## Archive Naming and Storage

- Use `.7z` archives for best compression.
- Split large archives into volumes if needed. The extraction script supports both single-file `.7z` archives and split archives where the first volume ends in `.7z.001`.
- Keep archives under `VideoAssets/` so they are clearly separated from source code and runtime data.

## Extraction Destination

The installer writes files into:

- `VideoAssets/Movies`

After extraction, the script creates a local junction at `Data/movies` so the built game sees the files through the normal runtime `Data` tree.

## Requirements

- 7-Zip installed and available on `PATH` (`7z.exe` or `7za.exe`).

## Notes

- This repository does not ship the original uncompressed movie sources directly.
- The archive is the canonical distribution artifact for video assets.
- If you need to regenerate the archive later, use `create_video_archive.ps1` with the original source folder.
