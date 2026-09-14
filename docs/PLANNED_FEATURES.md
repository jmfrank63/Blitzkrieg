# Planned Features

This document records the next product features after the current cross-platform
game and platform work. It is a planning guide, not a promise that every item is
already implemented or scheduled.

## Goals

- Make the game and its authoring tools usable on every supported desktop
  platform.
- Let players and mod authors choose their language and keep that choice with
  the appropriate profile or installation settings.
- Make mods installable, selectable, loadable, playable, and isolated from the
  base game and from one another.
- Produce repeatable, versioned releases with an installer or package for each
  supported platform.

## 1. Editors on every platform

Port the supported authoring workflow so modding does not require Windows-only
tools. The editor set should be identified explicitly rather than treating the
legacy Visual Studio solution as the product definition.

Planned work:

- Inventory the existing editors, asset converters, validators, and their file
  formats.
- Separate portable editor logic from Windows/MFC presentation code.
- Provide native editor binaries or a supported portable UI for Windows, macOS,
  and Linux where the platform is supported by the game.
- Keep editor and game data formats compatible, with explicit versioning and
  migration rules for formats that must change.
- Add headless validation for maps, campaigns, sprites, effects, meshes,
  localisation data, and mod manifests.
- Package editors independently from the game, while allowing a release to
  include both.

Exit criteria:

- A clean checkout can build and launch the supported editor set on each target
  claimed by the release matrix.
- A project created or edited on one supported platform opens on the others.
- A small fixture mod can be authored, validated, packaged, and loaded by the
  game without manual path edits.

## 2. Localisation

Replace hard-coded or implicitly selected language behavior with an explicit
localisation system shared by the game, editors, installers, and diagnostics
where user-facing text is involved.

Planned work:

- Define the supported language list, fallback language, locale identifiers, and
  encoding rules.
- Inventory existing text resources and assign stable message keys.
- Add language selection to settings and persist it at the correct scope:
  installation/user defaults for the launcher, and profile data only where the
  choice is intentionally profile-specific.
- Localise menus, dialogs, errors, editor UI, installer text, and mod metadata.
- Support text expansion, missing-key fallback, sorting rules, and fonts for
  every supported script.
- Give mods a namespaced localisation table with a deterministic fallback to the
  base game language.
- Add validation for duplicate keys, missing required keys, malformed files,
  unsupported encodings, and text that cannot fit its UI contract.

Exit criteria:

- Switching language takes effect without corrupting saves, profiles, or mods.
- A missing translation falls back visibly and diagnostically without preventing
  the game from starting.
- Automated checks cover every required base-game key and representative mod
  keys in every release language.

## 3. Mods and profile integration

Treat a mod as a versioned, discoverable package rather than an arbitrary data
directory copied over the installation. The loader must preserve the base game,
make load order deterministic, and keep profile data safe when a mod is
enabled, disabled, upgraded, or removed.

Planned work:

- Define a manifest containing an identifier, display name, version, game
  compatibility range, dependencies, conflicts, load order, entry points, and
  localisation metadata.
- Define package layout, installation locations, trust boundaries, and whether
  mods may contain native code.
- Discover installed mods per user and installation, with clear precedence and
  no accidental loading from the source checkout.
- Add enable/disable and load-order controls to the game and editor workflows.
- Mount mod assets through a layered virtual filesystem with deterministic
  override rules.
- Scope saves, screenshots, settings, and campaign progress to the active mod
  set; record the exact mod manifest versions in save metadata.
- Include active mod identity in profile/cloud-sync paths so profiles cannot
  silently exchange incompatible saves.
- Validate dependencies and conflicts before launch, with actionable recovery
  when a mod is missing or incompatible.
- Add package signing or at least checksum verification before this becomes an
  online distribution feature.

Exit criteria:

- A fixture mod can add or override content without changing the base install.
- Two profiles can use different mod sets without sharing incompatible saves.
- Reopening a profile reproduces the same resolved mod graph or reports the
  exact missing/incompatible dependency.
- Cloud sync never overwrites a save from a different active mod set without an
  explicit user decision.

## 4. Releases and installers

Turn the existing target matrix and package steps into reproducible, user-facing
releases. Each artifact must identify its target, architecture, build mode,
version, and included content.

Planned work:

- Define release channels, versioning, build provenance, and supported upgrade
  paths.
- Produce installers or native packages for each supported platform and
  architecture, with a portable archive as a fallback.
- Include the game, required runtime libraries, data, bundled tools, notices,
  default configuration, and optional editors according to the package variant.
- Add platform integration where appropriate: application metadata, icons,
  shortcuts, uninstall behavior, file associations, and user-data locations.
- Add signing, notarisation, checksum generation, and release artifact
  verification.
- Test fresh install, upgrade, uninstall, repair/reinstall, clean user data,
  existing profiles, mods, localisation, and rollback behavior.
- Publish a release matrix showing which combinations are playable, build-only,
  or unsupported.

Exit criteria:

- A release can be built from a tagged revision in a clean environment.
- Every advertised installer installs and launches the correct target.
- Existing profiles and mods survive supported upgrades, or the installer
  performs and reports an explicit migration.
- Release artifacts are signed or checksummed and can be reproduced or
  independently verified from the recorded build inputs.

## Suggested order

1. Finish platform/editor boundaries and define the portable authoring formats.
2. Define localisation keys and fallback behavior before expanding editor and
   mod UI.
3. Implement the mod manifest, virtual filesystem, dependency resolver, and
   profile/save identity together.
4. Build release packaging around the resolved game/editor/mod/localisation
   layout, then add signing and platform-specific installers.

The boundaries are intentionally coupled: editor output defines mod inputs,
localisation is part of both the base game and mod contract, profile identity
must include the active mod set, and release packaging must preserve all of
those decisions.
