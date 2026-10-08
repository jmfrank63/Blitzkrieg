# Golden: MFC export of `../project.bld`

This folder holds what the MFC ResourceEditor exports for the fixture project `../project.bld`:
the game data (stats XML, `_h.dds`, `_c.dds`, `.san`, ...) at the paths the project stores, relative to
the export folder. The D-11 comparator (`Sources/src/ResourceModel/comparator.h`) compares the port's
export of the same project with it, reading both through the engine's own readers.

## How it is made

Goldens are made on win-home only; the MFC `editor.exe` does not run on Linux or macOS.

1. Build or take the MFC editor (`Sources/src/bin/editor.exe` with its DLLs).
2. From the repository root run
   `powershell -ExecutionPolicy Bypass -File tools/zig/win-home/export-goldens.ps1 -Extensions bld`
   (no `-Extensions` exports all 20). The script copies this fixture to a scratch folder, runs the
   editor's batch mode (`editor.exe *.bld <source> <destination> -f`), clears this folder except
   `README.md` and `.gitkeep`, and copies the export here.
3. Check that the editor showed no error message box, then commit the folder.

Re-make the golden whenever `../project.bld` or its source art changes.

## Until a golden exists

`zig build test-resource-model -Dtest-mode=run` reports `GOLDEN bld pending: golden missing`. That is
not a pass: the golden comparison stays open until this folder is filled on win-home.

## GOG brandenburgertor (win-home only, never committed)

The named test `bld-gog-brandenburgertor` in `Sources/src/EditorBridge/resource_bridge_test.cpp` compares the
port's export of the GOG mod project `INTEX2/brandenburgertor/current.bld` with the MFC export of it. GOG files
are never copied into the repository, so the golden lives in an uncommitted folder:

1. On win-home run
   `powershell -ExecutionPolicy Bypass -File tools/zig/win-home/export-goldens.ps1 -GogProject <GOG>\...\INTEX2\brandenburgertor -GogOut <folder outside the repo>`.
   It exports that one project and prints the two values below.
2. Run `zig build test-resource-bridge -Dtest-mode=run` with `BK_GOG_ROOT` (the GOG install holding the INTEX2
   mod projects) and `BK_GOG_GOLDEN` (the `-GogOut` folder) set.

The test compares the stats XML field by field, `_h.dds` byte for byte and the other DDS within
`../../dxt-tolerance.json`. Without both variables it prints
`GOLDEN bld-gog-brandenburgertor pending: BK_GOG_ROOT/BK_GOG_GOLDEN not set (win-home only)`, which is not a pass.
