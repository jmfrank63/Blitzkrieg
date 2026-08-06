# P07-M04 — Port Scene Cursor, Video, and Transitions

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove cursor clipping/warping and remaining host timing from Scene.

**Dependencies:** P07-M03.

**Allowed files:** `Sources/src/Scene/Cursor.cpp`, `Sources/src/Scene/OpenVideoPlayer.cpp`, `Sources/src/Scene/VideoPlayer.cpp`, `Sources/src/Scene/Transition.cpp`, `Sources/src/Scene/SceneInternal.cpp`, `tools/zig/scene_platform_test.cpp`, `tools/zig/runtime_platform_allowlist.txt`.

- [x] Test cursor capture/warp/focus loss, logical/pixel coordinate conversion, transition timing, video seek/loop, minimize, and shutdown.
- [x] Route cursor state through the portable software cursor/input state and timing through monotonic services.
- [x] Ensure the cursor never remains captured after focus loss or module shutdown.
- [x] Preserve video frame selection and transition opacity under simulated time.
- [ ] Run native Windows/Linux fixtures and compile macOS.
- [x] Commit: `scene: port cursor video and transition services`

**Evidence:**

- Windows `scene-platform-test` passed: `cursor=1 coordinates=1 transition=16/255 video=0/8 minimize=1 shutdown=1`.
- `zig build game -Dtarget=x86_64-windows-msvc` passed. The Scene source scan is clean for native cursor calls, `_stricmp`, `timeGetTime`, and `GetTickCount`.
- Existing SDL window/event tests compile successfully for Windows. Their runtime window gate hung during the fullscreen lifecycle step in this environment and was terminated; the deterministic Scene fixture and game build remain green.
- Cursor capture/warp/clipping and OS cursor resource loading are now represented by Scene-owned software state, so focus loss and shutdown cannot leave a native cursor captured by this module.
