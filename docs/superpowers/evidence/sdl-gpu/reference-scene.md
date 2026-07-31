# P08-M02 deterministic reference scene

Status: capture harness and RGBA8 comparator implemented; baseline capture is pending a deterministic game-scene producer.

## Capture contract

`tools/zig/capture_gfx_reference.ps1` runs a trusted capture command three times for one renderer. The command receives these substitutions:

`{renderer}`, `{output}`, `{width}`, `{height}`, `{run}`, `{seed}`, `{camera}`, `{time}`, and `{data}`.

It must write exactly `width * height * 4` bytes of tightly packed RGBA8 pixels to `{output}`. The harness records renderer, dimensions, driver, commit, fixed time, camera, seed, data directory, fixture version, and SHA-256 metadata beside each temporary capture. It rejects any renderer whose three hashes differ.

`tools/zig/compare_gfx_reference.zig` compares two RGBA8 captures, writes an uncompressed lossless 32-bit BMP difference image, and enforces the initial thresholds: alpha exact, maximum RGB error 12/255, and mean RGB error 2/255.

## Current evidence

No six capture hashes or cross-renderer metrics are recorded yet. The game screenshot API currently writes TGA after an interactive `screenshot` command, and the repository has no command-line entry point that selects a fixed scene, camera, time, and seed and emits raw RGBA8. The harness therefore requires that missing deterministic capture command explicitly instead of treating an interactive launch as evidence.

Temporary artifacts are intentionally written below the system temporary directory and must not be committed.

## Required follow-up

Add a deterministic reference-scene producer that renders the same fixture through `renderer=legacy` and `renderer=sdl_gpu`, converts readback to tightly packed RGBA8, and invokes the harness for three runs per renderer. Then run the comparator and append the six SHA-256 hashes, exact dimensions, metrics, diff path, and classified differences here.
