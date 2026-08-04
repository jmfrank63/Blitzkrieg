# P07-M04 — Port Scene Cursor, Video, and Transitions

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; stop after this packet.

**Objective:** Remove cursor clipping/warping and remaining host timing from Scene.

**Dependencies:** P07-M03.

**Allowed files:** `Sources/src/Scene/Cursor.cpp`, `Sources/src/Scene/OpenVideoPlayer.cpp`, `Sources/src/Scene/VideoPlayer.cpp`, `Sources/src/Scene/Transition.cpp`, `Sources/src/Scene/SceneInternal.cpp`, `tools/zig/scene_platform_test.cpp`, `tools/zig/runtime_platform_allowlist.txt`.

- [ ] Test cursor capture/warp/focus loss, logical/pixel coordinate conversion, transition timing, video seek/loop, minimize, and shutdown.
- [ ] Route cursor state through PlatformClient and timing through monotonic services.
- [ ] Ensure the cursor never remains captured after focus loss or module shutdown.
- [ ] Preserve video frame selection and transition opacity under simulated time.
- [ ] Run native Windows/Linux fixtures and compile macOS.
- [ ] Commit: `scene: port cursor video and transition services`

**Evidence:** cursor lifecycle trace and frame/opacity fixture hashes.
