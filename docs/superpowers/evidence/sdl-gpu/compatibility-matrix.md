# Phase 8 compatibility matrix

P08-M01 owns the automated matrix. The executable matrix is defined in
Sources/src/GFXGPU/compatibility_test.zig and is required to contain a
legacy symbol, replacement symbol, test name, and evidence kind for every row.

Current matrix: 19 rows, zero unmapped rows.

Coverage groups:

- pixel, index, topology, compare, cull, blend, and FVF conversion
- immutable render-state and pipeline-key behavior
- all catalog effect IDs and shader-family policies
- shader manifest and pipeline probes
- buffer, texture, render-target, screenshot, and frame lifecycle

Validation commands:

    zig build test-gfxgpu-compatibility -Doptimize=Debug -Dtarget=x86_64-windows-msvc
    zig build test-gfxgpu-compatibility -Doptimize=ReleaseSafe -Dtarget=x86_64-windows-msvc

Both optimization modes must remain part of the packet evidence. Image
comparison and real-game parity are owned by P08-M02 and later packets.
