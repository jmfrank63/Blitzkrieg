---
slug: open-audio-layer
title: Open audio layer replacing FMOD
status: open
created: 2026-06-29
updated: 2026-06-29
---

# Thread: Open audio layer replacing FMOD

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
- `Sources/src/data/Sounds`
- `Sources/src/data/Music`

## Next Steps

- Approve the first design slice: keep public SFX interfaces stable, add a private open-audio backend boundary, and initially keep FMOD available only as a comparison path.
- Inventory FMOD references in source/project files with a narrower search than a full repository scan.
- Inventory audio asset formats and representative XML references.
- Add the backend boundary headers/classes before changing playback behavior.
