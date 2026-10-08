Steering after an independent review of M001 S01-S04 (fix commits b4be7df9c..0b37480f9 are already on the branch; read them).

The review found that slices marked complete are not done. Change the plan as follows, then continue:

1. Reopen S03 (ResourceModel). It is largely a shell: the item classes have no property tables (MFC has, for example, 48 properties for the weapon and 108 for the mesh tree items in Sources/src/editor), typed items serialise in a format that is not MFC's, and a node inserted through the bridge saves as `<>` (invalid XML). Port the property lists and the `operator&( IDataTree & )` serialisation of every MFC tree item faithfully (D-04, D-07), so every MFC project in Data/Editor/TestProjects and the fixtures opens, saves and reopens with equal content and MFC can read what the port writes.
2. D-07 fidelity: content-identical round trip is required for every project. Byte-identical output for a project opened and saved without edits is the goal: write with MFC's indentation and element order. Record in the spec whatever cannot be byte-identical, with the reason.
3. Comparator and goldens (S01/S03): every golden folder is empty, the comparator rejects real MFC projects, and it compares project XML instead of the exported game data. It must compare exported data (stats read by the engine's own reader, field by field, floats exact) against goldens generated from the MFC export, per D-11. Where an MFC export golden cannot be generated on this Linux machine, say so in the slice summary and keep the comparison ready for the win-home run; do not mark the comparison done.
4. DXT tolerance: re-measure on representative shipped textures (several DXT1, DXT3 and DXT5 `_c.dds` from Data, including alpha), not random noise; record the new numbers and use them as the gate.
5. Lock file: use MFC's `locked_<user>` as D-08 says, not `<path>.lock`; keep the atomic create from 0b37480f9.
6. Geometry: save it for every node that has it, not only the root, and make undo of a delete restore it.
7. Keep one sweep script, not both run-s02-sweep.sh and run-s03-sweep.sh. No sweep may classify a Map Editor tier failure as pre-existing.
8. Every task summary must cite commands that were really run in that task. Do not claim results from earlier tasks or other machines.

Note for the Map Editor tiers: map-editor-smoke and the auto tiers built on it fail when the real mouse pointer rests inside the hidden test window's screen area (the review saw this at 5ad96ec7f too). If they fail with the pointer over the window, that is the known test weakness. Make the smoke immune to the real pointer if that is cheap (for example, ignore the global mouse position while a script drives input); otherwise record it.
