# FMOD Replacement Inventory

Created: 2026-06-29

Thread: `.planning/threads/audio-replacment.md`

## Current Migration State

The first boundary step has started:

- Public SFX headers no longer include `fmod.h`.
- Public SFX headers no longer expose `FMOD` or `FSOUND` symbols.
- Sample-level FMOD operations now go through `Sources/src/SFX/AudioBackend.cpp`.
- The existing FMOD implementation is still active behind private SFX implementation files.
- `Sources/src/SFX/AudioFmodCompat.h` is the current private compatibility include.

This is not the open backend yet. It is the seam that lets the open backend replace FMOD with less churn in game-facing code.

## Runtime FMOD Implementation Surface

Primary files still using FMOD directly:

- `Sources/src/SFX/AudioBackend.cpp`
- `Sources/src/SFX/AudioFmodCompat.h`
- `Sources/src/SFX/SoundEngine.cpp`
- `Sources/src/SFX/SoundObjectFactory.cpp`
- `Sources/src/SFX/SFX.vcxproj`

Important FMOD responsibilities currently in `SoundEngine.cpp`:

- device enumeration and initialization
- output selection
- channel playback
- stream playback
- stream end callbacks
- pause/resume
- master volume
- 3D listener updates
- 3D distance and rolloff factors
- current playback position
- channel cleanup

Important FMOD responsibilities currently isolated in `AudioBackend.cpp`:

- sample loading from in-memory data
- sample free
- loop mode and loop point management
- min/max distance for 3D samples
- sample length and sample rate queries
- direct 2D/3D play helpers
- current sample checks for channel ownership

## Project And Copy References

Known project references:

- `Sources/src/SFX/SFX.vcxproj`
  - FMOD include paths
  - FMOD library paths
  - `fmodvc.lib` linker dependency
- `Sources/src/Game/Game.vcxproj`
  - `fmod.dll` post-build copy commands
  - `BlitzkriegRuntimeDll` item for `fmod.dll`
- `Sources/src/soundverifycation/SoundVerifycation.vcxproj`
  - FMOD include paths
  - FMOD library paths
  - `fmodvc.lib` linker dependency
- `Sources/src/soundverifycation/SoundVerifycation.cpp`
  - direct FMOD sample validation tool
- `Sources/src/Scene/Scene.vcxproj`
  - FMOD include path remains present and should be checked before removal

## Checked-In FMOD SDK Files

Known checked-in FMOD SDK/runtime files:

- `Sources/sdk/FMOD/api/fmod.dll`
- `Sources/sdk/FMOD/api/fmod64.dll`
- `Sources/sdk/FMOD/api/inc/fmod.h`
- `Sources/sdk/FMOD/api/inc/fmod_errors.h`
- `Sources/sdk/FMOD/api/inc/fmoddyn.h`
- `Sources/sdk/FMOD/api/lib/fmodvc.lib`
- `Sources/sdk/FMOD/api/lib/fmod64vc.lib`
- `Sources/sdk/FMOD/api/lib/fmodbc.lib`
- `Sources/sdk/FMOD/api/lib/fmodlcc.lib`
- `Sources/sdk/FMOD/api/lib/fmodwc.lib`
- `Sources/sdk/FMOD/documentation/FMOD.chm`
- FMOD Delphi, VB, and sample files under `Sources/sdk/FMOD`
- generated runtime copy: `Sources/src/Game/Debug/fmod.dll`

These stay for now while the compatibility backend still builds. Phase 6 removes them after the open backend is verified.

## Audio Asset Snapshot

Narrow scan of `Sources/src/data/Sounds`, `Sources/src/data/Music`, and `Data` found:

- `.wav`: 9022
- `.ogg`: 42
- `.xml`: 24691
- `.cfg`: 2
- many non-audio game data and texture/model/script files

Initial asset policy:

- Keep short `.wav` effects for now.
- Keep existing `.ogg` music playable until Opus streaming is implemented.
- Add Opus for long-form audio after the backend can play existing assets.
- Do not convert every one-shot sound as part of the first replacement.

## Removal Tracking

Phase 1, backend boundary:

- Keep public SFX interfaces free of FMOD symbols.
- Add internal backend interfaces.
- Keep FMOD behind private compatibility implementation while behavior remains unchanged.
- Keep `SampleSounds.cpp` free of direct FMOD calls.

Phase 2, samples:

- Replace the FMOD implementation inside `AudioBackend.cpp`.
- Replace one-shot channel ownership checks.
- Preserve weapon, UI, unit acknowledgement, movement, and explosion sounds.

Phase 3, streaming:

- Replace stream open/play/stop/fade/callback behavior.
- Add Opus support for long-form audio.
- Preserve existing OGG music while conversion policy is decided.

Phase 4, 3D parity:

- Replace listener, distance, rolloff, and positional channel behavior.
- Verify camera movement, weapon fire, movement loops, and explosions.

Phase 5, tooling:

- Add sound reference validation.
- Add optional Opus conversion tooling for long-form assets.

Phase 6, FMOD removal:

- Remove FMOD project include paths, library paths, linker inputs, and copy steps.
- Remove checked-in FMOD SDK/runtime files.
- Replace or retire `soundverifycation`.
- Update docs and fresh-clone build instructions.

## Smoke Checklist

Minimum runtime checklist before removing FMOD:

- launch game from fresh build
- logo/video path still runs
- main menu music plays
- tutorial starts
- UI clicks play
- unit selection acknowledgements play
- movement/engine loops play
- main gun fires audibly
- impacts/explosions play
- pause/resume does not permanently silence audio
- save/load does not duplicate or lose streams
- exit is clean
