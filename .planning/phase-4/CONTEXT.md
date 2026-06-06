# Phase 4: Runtime stability and compatibility - Context

**Gathered:** 2026-06-06
**Status:** Ready for planning
**Source:** User request in planning session

<domain>
## Phase Boundary

This phase targets runtime-facing UI scalability improvements for dialog windows so they match the enlarged text/UI scale already introduced. The scope includes identifying all gameplay and menu dialog boxes still using legacy dimensions and defining a safe, compatibility-preserving approach to enlarge them without breaking interactions.

</domain>

<decisions>
## Implementation Decisions

### Locked decisions
- Dialog boxes that still use original dimensions must be enlarged to match current UI scaling direction.
- Changes must preserve runtime stability and compatibility with existing assets/data.
- Work should be planned as part of Phase 4 (runtime stability and compatibility).

### the agent's Discretion
- Exact scaling multipliers and per-dialog overrides.
- Whether to centralize scaling constants or apply targeted layout fixes.
- Validation strategy depth (automated where possible, otherwise manual checklist).

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Roadmap and scope
- `.planning/ROADMAP.md` - phase goals and intended outcomes for Phase 4.
- `.planning/REQUIREMENTS.md` - runtime compatibility and preservation constraints.

### Prior phase context
- `.planning/phase-3/PLAN.md` - latest planning style and execution constraints.
- `.planning/phase-3/VERIFICATION.md` - current verification rigor and checklist approach.

### Likely code areas
- `Sources/src/UI/` - dialog/window layout code.
- `Sources/src/GFX/` - text rendering and scaling behavior.
- `Sources/src/ELK/` - editor-side font/layout generation constants.

</canonical_refs>

<specifics>
## Specific Ideas

- Audit all dialog classes for fixed pixel widths/heights and hardcoded offsets.
- Define a dialog scaling baseline that aligns with enlarged font rendering.
- Add a manual runtime checklist covering key dialog-heavy flows (menus, mission UI, confirmations, settings, tooltips where relevant).

</specifics>

<deferred>
## Deferred Ideas

- Full responsive UI rewrite.
- New theming/skin systems unrelated to size/compatibility.

</deferred>

---

*Phase: 04-runtime-stability-and-compatibility*
*Context gathered: 2026-06-06 via plan-phase prompt*