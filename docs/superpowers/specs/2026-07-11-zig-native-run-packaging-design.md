# Zig-native run and distribution design

## Goal

Make `zig build run` reliably stage and launch the game, and create readable distribution archives in reasonable time, without invoking PowerShell or any other shell staging script.

## Command contract

The existing public build commands remain available:

- `zig build install-game` creates the runnable development layout.
- `zig build run` stages once and launches `Game.exe` from that layout.
- `zig build package-game` creates the game-only archive.
- `zig build package-game-editors` creates the archive containing editor tools.
- `zig build package` creates both archive variants.

The existing `-Dinstall-dir`, `-Dcopy-data`, and `-Dpackage-dir` options retain their meanings.

## Architecture

A Zig staging tool replaces `tools/zig/game_install.ps1`. It accepts explicit source, destination, data-mode, and editor-inclusion arguments. Development staging copies current build artifacts and configuration files, then creates a Windows directory junction to the repository `Data` directory unless `-Dcopy-data=true` is selected. Distribution staging copies data into a package staging directory.

The build graph invokes staging exactly once before `run`. Launching is a separate Zig build run step whose working directory is the staged game directory. It never copies or removes staged files, so an already-running `Game.exe` cannot make a second staging pass fail.

Packaging uses one base staging tree. The game-only archive is written from that tree. Editor files are then added to the same tree for the editors archive, avoiding a second copy of the large `Data` directory. Individual package commands create the state they require and remain independently usable.

The ZIP writer reads source files in bounded chunks. Because ZIP local headers need CRC and size before stored file data, each file is scanned once to calculate CRC and size and then streamed a second time into the archive. This avoids whole-file allocation while retaining the currently validated stored-entry ZIP format. Archive output is written to a temporary sibling path and renamed only after finalization, preventing an interrupted build from leaving an apparently complete archive.

## File and process handling

Staging updates ordinary files without deleting the entire install root. The data destination is replaced only when its mode or target is wrong. Errors name the source and destination involved.

`run` does not terminate existing game processes. If staging genuinely needs to replace a locked binary, it reports the lock clearly; normal repeated `zig build run` avoids the prior duplicate-copy path.

Editor inclusion remains optional and fails when no supported editor executable exists. Supported editor sources remain the same as in the current script.

## Validation and performance

Focused Zig tests cover argument handling, staging decisions, editor discovery, ZIP header/offset generation, streaming CRC calculation, and temporary-output cleanup. An integration check creates a small fixture tree, stages it, packages both variants, and reads every entry stream to completion using the project-controlled validator.

Final verification runs the real `zig build run` far enough to prove staging and process launch without a duplicate copy, and runs the requested distribution command while timing it. Both resulting archives must exist, open successfully, contain the expected editor difference, and pass a full entry stream probe.

## Removal

After the build graph has no PowerShell references, `tools/zig/game_install.ps1` is deleted. Documentation is updated to state that run and packaging are implemented entirely in Zig.
