---
schema_version: 1
open_count: 3
waived_count: 0
fixed_count: 0
total_count: 3
last_updated: 2026-09-28T17:53:07.199Z
---

# Broken Windows Ledger

> Cross-phase defect register. With `workflow.windows_enforce` enabled, `/gsd-ship` blocks while `open_count > 0`.
> Waive with `gsd-tools windows waive <id> "<reason>"` (reason required).
> Mark fixed with `gsd-tools windows fixed <id>`.

| id | phase | kind | file | line | description | status | reason | recorded_at | resolved_at |
|----|-------|------|------|------|-------------|--------|--------|-------------|-------------|
| 1 | 03 | deviation | Sources/editor/app/panels.zig |  | plan-5 carried: the status line is never cleared after a later success (Task 4) | open |  | 2026-09-28T17:53:06.985Z |  |
| 2 | 03 | unrun-verify | Sources/editor/app/view.zig |  | plan-5 carried: no test of view.zig's event-to-tool wiring beyond the routing function (Task 4) | open |  | 2026-09-28T17:53:07.093Z |  |
| 3 | 03 | deviation | Sources/editor/app/view_math.zig |  | plan-5 carried: the scroll-direction unit test restates its own constants; an engine-tier ScreenToWorld direction check would catch a sign error (Task 6) | open |  | 2026-09-28T17:53:07.199Z |  |

````json
[
  {
    "id": 1,
    "kind": "deviation",
    "phase": "03",
    "file": "Sources/editor/app/panels.zig",
    "line": null,
    "description": "plan-5 carried: the status line is never cleared after a later success (Task 4)",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-09-28T17:53:06.985Z",
    "resolved_at": null,
    "milestone": null
  },
  {
    "id": 2,
    "kind": "unrun-verify",
    "phase": "03",
    "file": "Sources/editor/app/view.zig",
    "line": null,
    "description": "plan-5 carried: no test of view.zig's event-to-tool wiring beyond the routing function (Task 4)",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-09-28T17:53:07.093Z",
    "resolved_at": null,
    "milestone": null
  },
  {
    "id": 3,
    "kind": "deviation",
    "phase": "03",
    "file": "Sources/editor/app/view_math.zig",
    "line": null,
    "description": "plan-5 carried: the scroll-direction unit test restates its own constants; an engine-tier ScreenToWorld direction check would catch a sign error (Task 6)",
    "status": "open",
    "reason": "",
    "recorded_at": "2026-09-28T17:53:07.199Z",
    "resolved_at": null,
    "milestone": null
  }
]
````
