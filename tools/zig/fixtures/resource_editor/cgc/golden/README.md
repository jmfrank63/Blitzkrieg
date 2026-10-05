# Golden: MFC export of `../project.cgc`

This folder holds what the MFC ResourceEditor exports for the fixture project `../project.cgc`:
the game data (stats XML, `_h.dds`, `_c.dds`, `.san`, ...) at the paths the project stores, relative to
the export folder. The D-11 comparator (`Sources/src/ResourceModel/comparator.h`) compares the port's
export of the same project with it, reading both through the engine's own readers.

## How it is made

Goldens are made on win-home only; the MFC `editor.exe` does not run on Linux or macOS.

1. Build or take the MFC editor (`Sources/src/bin/editor.exe` with its DLLs).
2. From the repository root run
   `powershell -ExecutionPolicy Bypass -File tools/zig/win-home/export-goldens.ps1 -Extensions cgc`
   (no `-Extensions` exports all 20). The script copies this fixture to a scratch folder, runs the
   editor's batch mode (`editor.exe *.cgc <source> <destination> -f`), clears this folder except
   `README.md` and `.gitkeep`, and copies the export here.
3. Check that the editor showed no error message box, then commit the folder.

Re-make the golden whenever `../project.cgc` or its source art changes.

## Until a golden exists

`zig build test-resource-model -Dtest-mode=run` reports `GOLDEN cgc pending: golden missing`. That is
not a pass: the golden comparison stays open until this folder is filled on win-home.
