# Open Audio Layer Replacement Plan

## Goal

Replace the proprietary FMOD-based sound implementation with an open audio layer while keeping the existing game-facing sound interfaces stable.

Primary success condition: the game builds, runs, and plays sound without FMOD headers, import libraries, DLLs, or redistribution requirements.

## Recommended Direction

Use the current `Sources/src/SFX` layer as the migration boundary.

The rest of the game already appears to talk through interfaces such as `ISFX`, `ISound`, and `ISoundManager`. We should preserve those contracts and replace the implementation underneath them.

Recommended backend stack:

- Device, mixing, and playback: `miniaudio`
- Compressed music/long streams: `libopus` plus `opusfile`
- Existing short effects: keep `.wav` initially for low latency and minimal churn
- Optional legacy compatibility: support `.ogg` only if the existing asset set or tools still need it

Important distinction: Opus is a codec, not a complete audio engine. We still need a playback/mixer backend. `miniaudio` is the smallest practical fit for this codebase because it can handle device output, mixing, WAV decoding, and streaming without bringing in a large framework.

## Current FMOD Surface

The FMOD dependency is concentrated enough to migrate safely in phases.

Known starting points:

- `Sources/src/SFX/Specific.h` includes `fmod.h`
- `Sources/src/SFX/SoundEngine.h`
- `Sources/src/SFX/SoundEngine.cpp`
- `Sources/src/SFX/SampleSounds.h`
- `Sources/src/SFX/SampleSounds.cpp`
- `Sources/src/SFX/StreamingSound.h`
- `Sources/src/SFX/StreamingSound.cpp`
- `Sources/src/SFX/StreamFadeOff.*`
- `Sources/src/SFX/SoundManager.*`
- `Sources/src/SFX/SoundObjectFactory.cpp`
- Visual Studio project/linker settings that reference FMOD libraries
- Runtime output folders that carry `fmod.dll`

The game-facing users should mostly remain unchanged:

- `Sources/src/Game/main.cpp`
- movie/UI code that calls `GetSingleton<ISFX>()`
- mission, object, and unit systems that request sounds through manager interfaces

## Non-Goals For This First Audio Pass

- Replacing Bink/video playback
- Changing gameplay sound design
- Reauthoring all sound assets
- Replacing the serialization format unless absolutely required
- Converting every short `.wav` sound to Opus

Removing Bink is a separate proprietary-technology project. It should be tracked, but not mixed into the first audio backend migration.

## Phase 0: Inventory And Acceptance Criteria

Tasks:

1. Search for all FMOD references in source, project files, scripts, and output-copy steps.
2. Locate all checked-in FMOD binaries, headers, import libs, and generated copies.
3. Inventory sound asset extensions under `Data`, `Sources/src/data`, and any copied runtime data folders.
4. Identify every sound path format used by XML/config/database files.
5. Add a simple tracking document listing every FMOD reference and the phase that removes it.

Acceptance criteria:

- We know every place that references FMOD.
- We know which sound formats are actually used.
- We have a runtime smoke checklist before changing behavior.

Estimate: 0.5 to 1 day.

## Phase 1: Backend Boundary

Tasks:

1. Introduce a private backend interface inside `Sources/src/SFX`, for example:
   - `IAudioBackend`
   - `IAudioSample`
   - `IAudioVoice`
   - `IAudioStream`
2. Keep public interfaces such as `ISFX`, `ISound`, and `ISoundManager` intact.
3. Move FMOD-specific types out of public SFX headers.
4. Add a compile-time backend switch while the migration is in progress:
   - `BK_AUDIO_FMOD`
   - `BK_AUDIO_OPEN`
5. Make the open backend initialize and shut down cleanly, even before it can play all sounds.

Acceptance criteria:

- SFX public headers no longer leak FMOD types.
- The game can compile with the open backend selected.
- The old FMOD backend can remain temporarily as a comparison path.

Estimate: 1 to 2 days.

## Phase 2: Short Sound Effects

Tasks:

1. Implement WAV loading for existing one-shot effects.
2. Map old sample concepts to backend objects:
   - load/free sample
   - play once
   - play looped
   - stop
   - pause/resume
   - volume
   - pan or 2D position if currently used
   - playback cursor where required
3. Implement channel or voice tracking to replace FMOD channel IDs.
4. Preserve current XML/database sound references.
5. Add logging when a sound file is missing or unsupported.

Acceptance criteria:

- Menu/UI click sounds work.
- Unit selection acknowledgements work.
- Tank movement/engine loops work if they use sample playback.
- Weapon fire and explosion samples work.
- Missing sounds are logged with the resolved path.

Estimate: 3 to 5 days.

## Phase 3: Streaming Audio And Opus

Tasks:

1. Implement stream playback for music, ambient tracks, and long sounds.
2. Add Opus decoding through `opusfile`.
3. Keep WAV stream support if existing assets depend on it.
4. Recreate stream operations currently handled by FMOD:
   - start
   - stop
   - pause/resume
   - looping
   - volume
   - fade out
   - end-of-stream callback or equivalent polling
5. Add asset lookup that can resolve legacy references to converted files when appropriate:
   - exact referenced path
   - same basename with `.opus`
   - same basename with `.ogg`
   - same basename with `.wav`

Acceptance criteria:

- Main menu music or ambient stream plays.
- In-game music/ambient stream plays.
- Fade-outs work without clicks or hangs.
- Loading a saved game does not leave duplicate streams running.

Estimate: 3 to 5 days.

## Phase 4: 3D Audio Parity

Tasks:

1. Implement listener updates from the current camera/player view.
2. Implement positional sounds with distance attenuation.
3. Match existing FMOD distance/min-distance/rolloff behavior closely enough for gameplay.
4. Support channel limits and priority rules so too many simultaneous sounds degrade gracefully.
5. Verify moving units, weapon fire, explosions, and ambient sounds from camera movement.

Acceptance criteria:

- Positional sounds pan and attenuate plausibly.
- Unit and weapon sounds do not all play at full volume from anywhere on the map.
- Channel exhaustion does not crash or permanently silence sound.

Estimate: 1 to 2 weeks.

## Phase 5: Asset Migration Tools

Tasks:

1. Add a script under `tools/audio` to convert selected long-form assets to Opus.
2. Do not require conversion for short WAV effects unless size becomes a problem.
3. Document required external tools, such as `opusenc`, without committing proprietary encoders.
4. Update sound XML/config references only where lookup fallback is insufficient.
5. Add a validation script that checks every referenced sound file resolves.

Acceptance criteria:

- Converted Opus assets are checked in where needed.
- Existing short effects still work.
- A fresh clone can validate all referenced sound assets.

Estimate: 3 to 7 days, depending on asset count.

## Phase 6: Remove FMOD

Tasks:

1. Remove FMOD includes.
2. Remove FMOD libraries from Visual Studio project files.
3. Remove FMOD DLL copy steps.
4. Remove checked-in FMOD binaries and headers if they exist in the repo.
5. Rename module descriptions such as `Sound (FMOD)` to the new backend name.
6. Update README/build docs.

Acceptance criteria:

- Clean clone builds without any FMOD SDK installed.
- No `fmod.dll`, `fmod.h`, or FMOD import library is required at build or runtime.
- Searching the repo for `FMOD`, `FSOUND`, and `fmod` finds only historical notes or migration documentation.

Estimate: 1 to 2 days after the open backend is complete.

## Phase 7: Regression Pass

Runtime smoke tests:

1. Launch game from a fresh clone.
2. Verify logo/video sequence still works.
3. Verify main menu audio.
4. Start tutorial.
5. Select units and hear acknowledgements.
6. Move units and hear engine/movement loops.
7. Fire main gun.
8. Hear impact/explosion sounds.
9. Change game speed.
10. Save and load a game.
11. Return to menu and start another mission.
12. Alt-tab or lose focus, then resume.
13. Exit cleanly.

Build checks:

1. Debug Win32 build.
2. Release Win32 build.
3. Fresh clone build.
4. Repo search proves FMOD is not required.

Acceptance criteria:

- The tutorial can be completed.
- A saved game can be loaded.
- Main weapon fire works.
- There are no startup asserts caused by missing sound files.
- There are no FMOD runtime dependencies.

Estimate: 2 to 5 days for first pass, then repeat as bugs are found.

## Rough Total Estimate

Usable open audio backend:

- 4 to 6 weeks

Polished replacement with confidence across tutorial, campaigns, save/load, and asset validation:

- 6 to 8 weeks

The largest unknown is not Opus decoding itself. The harder part is matching FMOD-era behavior around 3D sounds, channels, callbacks, fades, and weird legacy assumptions in old game code.

## Suggested First Commit

Start with a small, non-invasive inventory commit:

1. Add FMOD reference inventory.
2. Add sound asset inventory script.
3. Add runtime smoke checklist.
4. Add the backend boundary headers without changing behavior.

This gives us a safe baseline and makes the first real implementation pass much less mysterious.
