# Phase 09 — Native Acceptance and Cutover

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Prove native runtime behavior and make the platform ABI the only playable host-service path.

| Packet | Depends on | Owns |
|---|---|---|
| P09-M01 | P08-M06 | Linux native launch and smoke |
| P09-M02 | M01 | Linux mission/save/load/endurance UAT |
| P09-M03 | P08-M06 | macOS native smoke and human UAT |
| P09-M04 | P08-M06 | Windows native regression and endurance |
| P09-M05 | M02, M03, M04 | final cutover, allowlist removal, handoff |

Exit: all native gates are accepted, the temporary allowlist is empty, and PlatformRuntime is the sole playable platform boundary.
