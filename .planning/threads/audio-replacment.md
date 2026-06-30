---
slug: audio-replacment
title: Audio replacement: open layer replacing FMOD
status: open
created: 2026-06-29
updated: 2026-06-29
---

# Thread: Audio replacement: open layer replacing FMOD

## Goal

Replace the proprietary FMOD sound layer with an open audio implementation. Audio comes first; replacing Bink/video playback is a later thread after sound is working.

## Context

Created 2026-06-29.

The current target is a staged migration, not a rewrite of all game audio call sites.

Known direction:

- Preserve the existing game-facing sound interfaces where possible.
- Treat `Sources/src/SFX` as the first migration boundary.
- Replace FMOD internals below that boundary.
- Prefer an open backend such as `miniaudio` for device output/mixing/playback.
- Use `libopus`/`opusfile` for long-form compressed audio when needed.
- Keep short WAV effects initially, especially UI, unit, weapon, and explosion sounds.
- OGG music currently exists under `Sources/src/data/Music`.
- Large sets of sound XML/WAV assets exist under `Sources/src/data/Sounds`.

Important prior runtime context:

- Fresh clone/runtime asset issues were mostly resolved by checking in missing data.
- Tutorial can run and saved games can load.
- Main-gun behavior was recently used as a practical audio/gameplay smoke test.
- FMOD replacement should preserve main menu audio, tutorial sounds, unit acknowledgements, movement loops, weapon fire, impacts, explosions, music, and save/load behavior.

Plan document:

- `docs/superpowers/plans/2026-06-29-open-audio-layer.md`

First implementation slice:

- Added `tools/audio/check_sfx_public_headers.ps1`.
- Moved direct FMOD exposure out of public SFX headers.
- Added private compatibility include `Sources/src/SFX/AudioFmodCompat.h`.
- Kept existing FMOD runtime behavior in `SampleSounds.cpp` and `SoundEngine.cpp`.
- Added `docs/audio/fmod-replacement-inventory.md`.
- Verified SFX and Game Debug Win32 builds after the boundary change.

Second implementation slice:

- Added `Sources/src/SFX/AudioBackend.h` and `Sources/src/SFX/AudioBackend.cpp`.
- Moved sample-level FMOD calls out of `SampleSounds.cpp`.
- Added `tools/audio/check_sfx_sample_backend.ps1` to keep sample code behind the backend boundary.

Third implementation slice:

- Routed `SoundEngine.cpp` sample/channel operations through `AudioBackend`.
- Added `tools/audio/check_sfx_soundengine_sample_backend.ps1`.
- Left stream and device FMOD calls in `SoundEngine.cpp` for the next slice.

Fourth implementation slice:

- Routed `SoundEngine.cpp` stream operations through `AudioBackend`.
- Added `tools/audio/check_sfx_soundengine_stream_backend.ps1`.
- Left FMOD device initialization and listener/rolloff calls in `SoundEngine.cpp` for later slices.

Fifth implementation slice:

- Routed `SoundEngine.cpp` device initialization, shutdown, output handle, distance, and rolloff operations through `AudioBackend`.
- Added `tools/audio/check_sfx_soundengine_device_backend.ps1`.
- Active direct FMOD calls are now concentrated behind `AudioBackend` plus other non-SFX projects/tools still listed in the inventory.

Sixth implementation slice:

- Renamed the module descriptor from `Sound (FMOD)` to `Sound`.
- Added `tools/audio/check_sfx_upper_layer_fmod.ps1`.
- SFX upper-layer source is guarded so FMOD naming/symbols stay in backend files only.

Seventh implementation slice:

- Added `Sources/src/SFX/AudioBackendImpl.h` as the internal backend implementation contract.
- Moved the current FMOD implementation into `Sources/src/SFX/AudioBackendFmod.cpp`.
- Replaced `Sources/src/SFX/AudioBackend.cpp` with a clean delegating wrapper.
- Added `tools/audio/check_sfx_backend_scaffold.ps1`.
- FMOD remains the default active backend through `SFX_USE_FMOD_BACKEND`.

Eighth implementation slice:

- Added `Sources/src/SFX/AudioBackendOpen.cpp` as an opt-in silent open backend scaffold.
- Added `/p:AudioBackend=Open` selection in `Sources/src/SFX/SFX.vcxproj`.
- Added `tools/audio/check_sfx_open_backend_scaffold.ps1`.
- Default builds still select the FMOD backend; the open backend is compile-tested only.

Ninth implementation slice:

- Added PCM WAV parsing to the opt-in open backend.
- `AudioBackendOpen.cpp` now extracts `RIFF/WAVE`, `fmt `, and `data` chunks.
- Open backend sample length now reports decoded PCM frame count and sample rate comes from the WAV format chunk.
- Added `tools/audio/check_sfx_open_backend_wav.ps1`.
- Playback remains silent; default runtime remains FMOD.

## References

- `Sources/src/SFX/Specific.h`
- `Sources/src/SFX/SoundEngine.h`
- `Sources/src/SFX/SoundEngine.cpp`
- `Sources/src/SFX/SampleSounds.h`
- `Sources/src/SFX/SampleSounds.cpp`
- `Sources/src/SFX/StreamingSound.h`
- `Sources/src/SFX/StreamingSound.cpp`
- `Sources/src/SFX/StreamFadeOff.h`
- `Sources/src/SFX/StreamFadeOff.cpp`
- `Sources/src/SFX/SoundManager.h`
- `Sources/src/SFX/SoundManager.cpp`
- `Sources/src/SFX/SoundObjectFactory.cpp`
- `Sources/src/SFX/SFX.vcxproj`
- `Sources/src/SFX/AudioBackend.h`
- `Sources/src/SFX/AudioBackend.cpp`
- `Sources/src/SFX/AudioBackendImpl.h`
- `Sources/src/SFX/AudioBackendFmod.cpp`
- `Sources/src/SFX/AudioBackendOpen.cpp`
- `Sources/src/SFX/AudioFmodCompat.h`
- `Sources/src/data/Sounds`
- `Sources/src/data/Music`
- `docs/audio/fmod-replacement-inventory.md`
- `tools/audio/check_sfx_public_headers.ps1`
- `tools/audio/check_sfx_sample_backend.ps1`
- `tools/audio/check_sfx_soundengine_sample_backend.ps1`
- `tools/audio/check_sfx_soundengine_stream_backend.ps1`
- `tools/audio/check_sfx_soundengine_device_backend.ps1`
- `tools/audio/check_sfx_upper_layer_fmod.ps1`
- `tools/audio/check_sfx_backend_scaffold.ps1`
- `tools/audio/check_sfx_open_backend_scaffold.ps1`
- `tools/audio/check_sfx_open_backend_wav.ps1`

## Next Steps

- Audit remaining SFX project-file FMOD references before removing the compatibility backend.
- Add the first open backend channel state for simple 2D sample playback, still silent if device output is not ready.
- Keep `tools/audio/check_sfx_public_headers.ps1` green.
- Keep `tools/audio/check_sfx_sample_backend.ps1` green.
- Keep `tools/audio/check_sfx_soundengine_sample_backend.ps1` green.
- Keep `tools/audio/check_sfx_soundengine_stream_backend.ps1` green.
- Keep `tools/audio/check_sfx_soundengine_device_backend.ps1` green.
- Keep `tools/audio/check_sfx_upper_layer_fmod.ps1` green.
- Keep `tools/audio/check_sfx_backend_scaffold.ps1` green.
- Keep `tools/audio/check_sfx_open_backend_scaffold.ps1` green.
- Keep `tools/audio/check_sfx_open_backend_wav.ps1` green.
- Continue to build `SFX.vcxproj` and `Game.vcxproj` after each small slice.
