# Golden: MFC export of `../project.3rd`

This folder holds what the MFC ResourceEditor exports for the fixture project `../project.3rd`:
the game data (stats XML, `_h.dds`, `_c.dds`, `.san`, ...) at the paths the project stores, relative to
the export folder. The D-11 comparator (`Sources/src/ResourceModel/comparator.h`) compares the port's
export of the same project with it, reading both through the engine's own readers.

## How it is made

Goldens are made on win-home only; the MFC `editor.exe` does not run on Linux or macOS.

1. Build or take the MFC editor (`Sources/src/bin/editor.exe` with its DLLs).
2. From the repository root run
   `powershell -ExecutionPolicy Bypass -File tools/zig/win-home/export-goldens.ps1 -Extensions 3rd`
   (no `-Extensions` exports all 20; for both VSO kinds use `-Extensions 3rd,3rv`). The script copies this fixture to a scratch folder, runs the
   editor's batch mode (`editor.exe *.3rd <source> <destination> -f`), clears this folder except
   `README.md` and `.gitkeep`, and copies the export here.
3. Check that the editor showed no error message box, then commit the folder.

Re-make the golden whenever `../project.3rd` or its source art changes.

## Until a golden exists

`zig build test-resource-model -Dtest-mode=run` reports `GOLDEN 3rd pending: golden missing`. That is
not a pass: the golden comparison stays open until this folder is filled on win-home.

## What the port tests prove meanwhile

`zig build test-resource-bridge -Dtest-mode=run` (S13 T05) exports `../project.3rd`, reads the road
back through the engine's `SVectorStripeObjectDesc` operator&, checks a second forced export is byte-identical,
imports the exported file and re-exports it field-equal, and prints `GOLDEN 3rd pending`. It also imports and
re-exports every shipped Roads3D and Rivers file (`VSO checked=N files=N unimportable=U`). This is not the MFC
golden comparison, which stays open until this folder is filled on win-home.
