# Phase 01 — Core Host Services

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`; execute only the assigned packet.

**Goal:** Move process-wide clocks, synchronization, diagnostics, dynamic loading, storage paths, and system services behind the ABI.

| Packet | Depends on | Owns |
|---|---|---|
| P01-M01 | P00-M05 | clock, sleep, atomics |
| P01-M02 | M01 | mutex, event, thread |
| P01-M03 | M02 | diagnostics and debugger state |
| P01-M04 | M03 | dynamic libraries and symbols |
| P01-M05 | M04 | paths, metadata, enumeration |
| P01-M06 | M05 | environment, dialogs, launch, process |

Exit: core native contracts pass and converted consumers contain no direct core Win32 calls.
