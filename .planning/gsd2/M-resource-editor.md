# Milestone: portable Resource Editor (planning phase 6)

Port the MFC Blitzkrieg Resource Editor (`Sources/src/editor`, `editor.exe`) to the portable editor stack as a new
`ResourceEditor` executable built by `zig build`.

## Source of truth (read these first; they are complete and approved)

- Design spec: `docs/superpowers/specs/2026-09-30-portable-resource-editor-design.md`
- Decisions D-01..: `.planning/phases/06-resource-editor-portable-port/06-CONTEXT.md`
- Feature checklist with the target plan per feature: `.planning/phases/06-resource-editor-portable-port/06-PARITY.md`
- Discussion log: `.planning/phases/06-resource-editor-portable-port/06-DISCUSSION-LOG.md`
- How the Map Editor was built (patterns to reuse): `.planning/phases/03-*`, `04-*`, `05-*` summaries,
  `docs/superpowers/specs/2026-09-19-portable-map-editor-design.md`, and the code in `Sources/editor`.

## Changes to the approved plan (newer than the spec; apply and record them in the spec)

1. **Linux x64 is a target too**, alongside macOS arm64/x64 and Windows x64. The Map Editor now builds and runs on
   Linux (merge `1d7264fd6`); the Resource Editor and the shared editor kit must as well, with their tiers green on
   Linux. This machine is Linux, so Linux is the native development and test target here.
2. The planned order of the remaining work is: this milestone, then the small tools (phase 8), then ELK (phase 7),
   then game localisation, editor cloud sync, and the installer. The shared editor kit (`Sources/editor/kit`) this
   milestone extracts is consumed by ELK later: keep it free of the engine bridge as D-02 requires.
3. Editor cloud sync will be designed later for all editors. Do not build it here, but do not make choices that
   block it: user content (projects, settings, recent files) lives under the kit's user-data root, never beside
   the executable, and paths stored inside projects are relative where the format allows.

## Scope and done

Everything in 06-PARITY.md: all 21 sub-editors, projects, export, MOD settings, PAK, picture options, directories,
batch mode, run game, reference pickers, property inspector, plus undo/redo, safe save, autosave and recovery, and
Import from game data. The work is large (64k lines of MFC); split it into as many slices, and if needed follow-on
milestones, as the work needs, in the order the context sets: kit extraction with the Map Editor unchanged and
green first, then the portable `ResourceModel` C++ library and the `BkRes*` bridge, then the core and app, then
the sub-editors.

Done means: every parity row is done with evidence (tests, captured output, the game reading the export), the Map
Editor tiers are still green on Linux, the Resource Editor builds and its tiers pass on Linux, and the MFC editor
is deleted as the context describes (D-for-deletion). Update `.planning/STATE.md` at the end.
