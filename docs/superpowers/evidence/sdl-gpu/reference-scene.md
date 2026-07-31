# P08-M02 deterministic reference scene

Status: deterministic capture mode, harness, and RGBA8 comparator implemented; baseline capture is pending successful runtime readback.

## Capture contract

`tools/zig/capture_gfx_reference.ps1` runs a trusted capture command three times for one renderer. The command receives these substitutions:

`{renderer}`, `{output}`, `{width}`, `{height}`, `{run}`, `{seed}`, `{camera}`, `{time}`, and `{data}`.

It must write exactly `width * height * 4` bytes of tightly packed RGBA8 pixels to `{output}`. The harness records renderer, dimensions, driver, commit, fixed time, camera, seed, data directory, fixture version, and SHA-256 metadata beside each temporary capture. It rejects any renderer whose three hashes differ.

The game producer is the opt-in `-reference-scene <path>` mode. It reuses the existing startup-smoke main-menu-ready checkpoint, forces the zero random seed, calls the active renderer's `TakeScreenShot`, writes RGBA bytes, and exits. Normal game launches are unchanged.

`tools/zig/compare_gfx_reference.zig` compares two RGBA8 captures, writes an uncompressed lossless 32-bit BMP difference image, and enforces the initial thresholds: alpha exact, maximum RGB error 12/255, and mean RGB error 2/255.

## Current evidence

No six capture hashes or cross-renderer metrics are recorded yet. The new deterministic producer compiles for both renderers, and ordinary `-x64-startup-smoke -windowed` exits successfully in this session. However, launching `-reference-scene` from this non-interactive session exits with code 3 before producing a file; the existing video trace is written but no readback output is created. This runtime readback failure must be reproduced under an interactive Windows desktop/debugger session before M02 can be marked complete.

Temporary artifacts are intentionally written below the system temporary directory and must not be committed.

## Required follow-up

Run the producer under an interactive Windows desktop for both renderer builds, resolve the runtime readback failure, invoke the harness for three runs per renderer, and append the six SHA-256 hashes, exact dimensions, metrics, diff path, and classified differences here.
