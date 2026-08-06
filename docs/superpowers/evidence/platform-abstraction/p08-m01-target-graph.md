# P08-M01 Target Graph Evidence

`zig test tools/zig/build_support.zig` passed 5/5 and
`zig test tools/zig/platform_build_matrix_test.zig` covers the three supported
triples. The source-set policy names shared, Windows, POSIX, Linux, macOS,
Windows-oracle, and excluded utility groups.

Windows `zig build game-all -Dtarget=x86_64-windows-msvc -Dtest-mode=compile`
passed. Developer utilities and the legacy DirectX renderer are now created
only for the Windows target; the SDL GPU renderer is required elsewhere.
MSVC include paths, library paths, and CRT selection are target-gated rather
than host-gated, preventing a Windows host from contaminating a Linux target.

The Windows-host Linux cross-build was also inspected. It now avoids MSVC
headers/CRT and reaches the expected host SDL shared-library/sysroot boundary;
native Linux closure continues in WSL/CI where the Linux toolchain owns those
files.
