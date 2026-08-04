# Phase 06 — Portable Game Shell

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Replace the Win32 application shell with the SDL platform runtime while retaining a thin Windows resource adapter.

| Packet | Depends on | Owns |
|---|---|---|
| P06-M01 | P02-M05 | portable entry point and command line |
| P06-M02 | M01 | `WinFrame` split and SDL window operations |
| P06-M03 | M02, P03-M06 | system keys and input routing |
| P06-M04 | M03 | module load/unload and startup errors |
| P06-M05 | M04 | main loop, focus, quit, restart |
| P06-M06 | M05, P04-M05, P05-M05 | Game bootstrap integration gate |

Exit: Game reaches module initialization and clean shutdown natively on Windows and Linux without compiling Win32 shell code on Linux/macOS.
