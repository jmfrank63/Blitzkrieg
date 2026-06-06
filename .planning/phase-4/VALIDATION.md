# Phase 4 Validation

## Scope

Validate that Phase 4 execution covers dialog enlargement inventory, startup stability gates, and required closeout artifacts.

## Requirement to evidence mapping

| Requirement | Evidence Artifact | Status |
|---|---|---|
| All in-scope original-size dialogs are enlarged | `.planning/phase-4/artifacts/dialog-coverage-map.csv`, `.planning/phase-4/artifacts/dialog-coverage-gaps.txt`, `Sources/src/UI/UIScreen.cpp::ShouldScaleLegacyLayout` | PASS (automated) |
| Stable tutorial startup in Debug | `.planning/phase-4/VERIFICATION.md` startup rows | PENDING (human verify) |
| Stable mission startup in Debug | `.planning/phase-4/VERIFICATION.md` startup rows | PENDING (human verify) |
| Runtime stability improvements documented in project state | `.planning/STATE.md` Phase 4 entry | PASS |
| Deterministic verification process exists | `.planning/phase-4/PLAN.md` deterministic command/check sections, `.planning/phase-4/VERIFICATION.md` automated checks table | PASS |

## Validation checklist

- [x] In-scope dialog inventory command defined and executed
- [x] Coverage map schema and allowed status values validated
- [x] Gap detection and pass/fail behavior validated
- [x] Startup stability protocol and run counts defined
- [x] Required closeout update to `.planning/STATE.md` completed
- [ ] Human runtime startup verification completed (tutorial + mission)

## Notes

Task 1 and build-related automated checks are passing.
Final phase completion remains blocked on manual runtime startup verification rows in `VERIFICATION.md`.
