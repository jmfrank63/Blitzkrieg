# Golden: MFC export of `../project.msh`

This folder holds what the MFC ResourceEditor exports for the fixture project `../project.msh`:
the game data (stats XML, `_h.dds`, `_c.dds`, `.san`, ...) at the paths the project stores, relative to
the export folder. The D-11 comparator (`Sources/src/ResourceModel/comparator.h`) compares the port's
export of the same project with it, reading both through the engine's own readers.

## How it is made

Goldens are made on win-home only; the MFC `editor.exe` does not run on Linux or macOS.

1. Build or take the MFC editor (`Sources/src/bin/editor.exe` with its DLLs).
2. From the repository root run
   `powershell -ExecutionPolicy Bypass -File tools/zig/win-home/export-goldens.ps1 -Extensions msh`
   (no `-Extensions` exports all 20). The script copies this fixture to a scratch folder, runs the
   editor's batch mode (`editor.exe *.msh <source> <destination> -f`), clears this folder except
   `README.md` and `.gitkeep`, and copies the export here.
3. Check that the editor showed no error message box, then commit the folder.

Re-make the golden whenever `../project.msh` or its source art changes.

## Source art

The export needs the project's models and pictures beside `../project.msh`: `1.mod`, `2.mod` and
`3.mod` copied unchanged from the shipped `Data/Units/Technics/German/Artillery/8_8_cm_FlaK18`
(a unit that ships all three variants), and the generated 16 x 16 targas `1.tga`, `1w.tga`,
`1a.tga`, `2.tga`, `2w.tga`, `2a.tga` and `icon.tga` (`zig build make-resource-fixtures`). The
scratch copy win-home makes of the fixture folder carries all of them.

## Until a golden exists

`zig build test-resource-model -Dtest-mode=run` reports `GOLDEN msh pending: golden missing`. That is
not a pass: the golden comparison stays open until this folder is filled on win-home.
