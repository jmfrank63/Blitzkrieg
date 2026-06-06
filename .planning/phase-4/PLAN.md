---
phase: 4
phase_name: Runtime stability and compatibility
plan_id: phase-4-dialog-enlargement
must_haves:
  truths:
    - All runtime gameplay/menu dialogs that were still original-size are enlarged.
    - Tutorial and mission startup remain stable in Debug.
    - Phase 4 closeout state clearly reports runtime stability outcomes.
  artifacts:
    - .planning/phase-4/artifacts/dialog-inventory.txt
    - .planning/phase-4/artifacts/dialog-producers.txt
    - .planning/phase-4/artifacts/dialog-coverage-map.csv
    - .planning/phase-4/VERIFICATION.md
    - .planning/phase-4/VALIDATION.md
    - .planning/STATE.md
  key_links:
    - Inventory completeness gates coverage mapping.
    - Coverage mapping gates runtime verification.
    - Runtime verification gates state closeout and validation summary.
---

# Phase 4 Plan: Runtime stability and compatibility

## Objective

Enlarge all in-scope runtime dialogs that still use legacy/original sizing so they align with the current UI scaling direction, while preserving click behavior, data compatibility, and runtime stability.

In-scope dialogs cannot be deferred. Each in-scope resource must end as either `scaled` or `fixed-geometry`, and both statuses must represent enlarged final behavior.

## Scope alignment

- Roadmap alignment: `.planning/ROADMAP.md` Phase 4 requires stable tutorial and mission startup and documented runtime-stability improvements.
- Locked decision coverage: `.planning/phase-4/CONTEXT.md` requires original-size dialogs to be enlarged.
- Technical basis and callsite inventory pattern: `.planning/phase-4/RESEARCH.md`.

## In-scope dialog definition

In scope means all runtime dialog resources that can appear in gameplay or menu flows, including but not limited to resources loaded through `pUIScreen->Load(...)` and any equivalent runtime dialog creation paths.

Coverage must include:

- Gameplay dialogs (mission/tutorial and in-mission popup flows)
- Frontend/menu dialogs (options/confirmation/selection flows)
- Runtime UI paths in `Sources/src/GameTT`, `Sources/src/UI`, and additional dialog-producing callsites discovered during inventory

The `pUIScreen->Load(...)` inventory is the initial seed, not the final boundary. Task 1 must expand the inventory until no additional runtime dialog producers are found for gameplay/menu flows.

No in-scope dialog may remain unaccounted or remain at original dimensions.

## Required artifacts

- `.planning/phase-4/artifacts/dialog-inventory.txt`
- `.planning/phase-4/artifacts/dialog-producers.txt`
- `.planning/phase-4/artifacts/policy-allowlist.txt`
- `.planning/phase-4/artifacts/dialog-coverage-map.csv`
- `.planning/phase-4/artifacts/dialog-coverage-gaps.txt`
- `.planning/phase-4/VERIFICATION.md`
- `.planning/phase-4/VALIDATION.md`

`dialog-coverage-map.csv` required header:

`resource,status,owner,evidence,notes`

Allowed status values:

- `scaled`
- `fixed-geometry`

## Execution tasks (3-task cap)

### Task 1 - Inventory, policy baseline, and enlargement coverage closure (auto)

- likely files/symbols:
  - `.planning/phase-4/artifacts/dialog-inventory.txt`
  - `.planning/phase-4/artifacts/policy-allowlist.txt`
  - `.planning/phase-4/artifacts/dialog-coverage-map.csv`
  - `.planning/phase-4/artifacts/dialog-coverage-gaps.txt`
  - `Sources/src/UI/UIScreen.cpp`
  - `Sources/src/GameTT/InterfaceMessageBox.cpp`
  - `Sources/src/GameTT/SaveReplay.cpp`
  - `Sources/src/GameTT/InterfaceNewDepotUpgrades.cpp`
  - `Sources/src/GameTT/SwitchToNextChapter.cpp`
  - `Sources/src/GameTT/InterfaceSwitchModeTo.cpp`
  - Symbols: `ShouldScaleLegacyLayout`, `CUIScreen::Load`, `CUIScreen::Reposition`
- action details:
  - Generate full runtime dialog inventory.
  - Generate runtime dialog producer inventory across non-`pUIScreen->Load(...)` creation paths.
  - Expand safe central scaling policy in `ShouldScaleLegacyLayout`.
  - Apply targeted geometry fixes for outliers not safely solved by central policy.
  - Ensure every inventory row is covered and enlarged via `scaled` or `fixed-geometry`.
- deterministic checks:
  - `New-Item -ItemType Directory -Force .planning/phase-4/artifacts | Out-Null`
  - `rg --no-heading --line-number --glob "Sources/src/GameTT/**/*.cpp" --glob "Sources/src/UI/UIScreen.cpp" 'pUIScreen->Load\("ui\\[^\"]+"' | ForEach-Object { if ($_ -match 'pUIScreen->Load\("(?<res>ui\\[^\"]+)"') { $matches['res'] } } | Sort-Object -Unique | Set-Content .planning/phase-4/artifacts/dialog-inventory.txt`
  - `rg --no-heading --line-number --glob "Sources/src/**/*.cpp" --glob "Sources/src/**/*.h" 'pUIScreen->Load\(|CreateObject<.*UI|CreateObject<.*GFX_TEXT|SetWindowTexture\(|SetFont\(' | Sort-Object -Unique | Set-Content .planning/phase-4/artifacts/dialog-producers.txt`
  - `rg --no-heading --line-number 'resourceName\s*==\s*"ui\\[^\"]+"' Sources/src/UI/UIScreen.cpp | ForEach-Object { if ($_ -match '"(?<res>ui\\[^\"]+)"') { $matches['res'] } } | Sort-Object -Unique | Set-Content .planning/phase-4/artifacts/policy-allowlist.txt`
  - `if (!(Test-Path .planning/phase-4/artifacts/dialog-coverage-map.csv)) { "resource,status,owner,evidence,notes" | Set-Content .planning/phase-4/artifacts/dialog-coverage-map.csv }`
  - `$inv = Get-Content .planning/phase-4/artifacts/dialog-inventory.txt; $map = Import-Csv .planning/phase-4/artifacts/dialog-coverage-map.csv; $covered = $map.resource; $gaps = $inv | Where-Object { $_ -and ($_ -notin $covered) }; $gaps | Set-Content .planning/phase-4/artifacts/dialog-coverage-gaps.txt; if ($gaps.Count -gt 0) { Write-Error "FAIL: unaccounted dialogs found"; exit 1 } else { Write-Output "PASS: all inventory dialogs accounted" }`
  - `$prod = Get-Content .planning/phase-4/artifacts/dialog-producers.txt; if ($prod.Count -eq 0) { Write-Error "FAIL: no dialog producers discovered"; exit 1 } else { Write-Output "PASS: dialog producers discovered" }`
  - `$badStatus = $map | Where-Object { $_.status -notin @('scaled','fixed-geometry') }; if ($badStatus.Count -gt 0) { Write-Error "FAIL: invalid status values"; exit 1 }`
  - `$missingNotes = $map | Where-Object { $_.status -eq 'fixed-geometry' -and [string]::IsNullOrWhiteSpace($_.notes) }; if ($missingNotes.Count -gt 0) { Write-Error "FAIL: fixed-geometry rows missing notes"; exit 1 }`
  - `$dup = $map | Group-Object resource | Where-Object { $_.Count -ne 1 }; if ($dup.Count -gt 0) { Write-Error "FAIL: duplicate or missing resource mapping"; exit 1 } else { Write-Output "PASS: one resource per row" }`
- acceptance criteria:
  - Inventory is non-empty and fully mapped.
  - Producer inventory is generated and reviewed for non-`pUIScreen` dialog creation paths.
  - Gaps file is empty.
  - No in-scope dialog remains original-size.

### Task 2 - Deterministic runtime stability verification (checkpoint:human-verify, blocking)

- likely files:
  - `.planning/phase-4/VERIFICATION.md`
  - `.planning/phase-4/artifacts/dialog-coverage-map.csv`
- action details:
  - Build and run deterministic startup checks for tutorial and mission paths.
  - Validate per-dialog interactions across required resolutions.
  - Capture full pass/fail table in `VERIFICATION.md`.
- deterministic checks:
  - `msbuild Sources/src/A7.sln /t:Game /p:Configuration=Debug /p:Platform=Win32 /m /v:minimal`
  - Tutorial startup: 3/3 passes at `1024x768` and 3/3 passes at `1366x768`.
  - Mission startup: 3/3 passes at `1024x768` and 3/3 passes at `1366x768`.
  - Switch loop: `1024x768 -> 1366x768 -> 1920x1080 -> 1024x768`, then one tutorial and one mission startup pass.
  - Per-resource dialog pass: 3/3 open/close cycles, 3/3 actionable-control interactions, zero clipping on actionable controls, zero crashes/asserts.
- required verification rows in `VERIFICATION.md`:
  - `tutorial-startup@1024x768`
  - `tutorial-startup@1366x768`
  - `mission-startup@1024x768`
  - `mission-startup@1366x768`
  - `tutorial-startup@switch-loop`
  - `mission-startup@switch-loop`
- acceptance criteria:
  - Human verification returns `approved`.
  - All required startup rows pass with required run counts.
  - No startup crash/assert/hang.

### Task 3 - Phase closeout documentation and validation artifacts (auto)

- likely files:
  - `.planning/phase-4/VALIDATION.md`
  - `.planning/phase-4/VERIFICATION.md`
  - `.planning/STATE.md`
- action details:
  - Write/update `VALIDATION.md` with requirement-to-evidence mapping for Phase 4.
  - Update `STATE.md` with Phase 4 runtime stability results, dialog coverage summary, final pass/fail, and evidence links.
  - Record whether low-level contingency is needed. If still required, create a follow-up plan instead of expanding this phase plan.
- deterministic checks:
  - `rg --line-number "Phase 4|runtime stability|dialog coverage|pass/fail|phase-4/VERIFICATION.md|dialog-coverage-map.csv" .planning/STATE.md`
  - `rg --line-number "Requirement|Evidence|Status|Phase 4" .planning/phase-4/VALIDATION.md`
  - `$state = Get-Content .planning/STATE.md -Raw; if ($state -notmatch 'Phase 4' -or $state -notmatch 'runtime stability' -or $state -notmatch 'dialog coverage' -or $state -notmatch 'pass/fail') { Write-Error "FAIL: STATE.md missing Phase 4 closure fields"; exit 1 } else { Write-Output "PASS: STATE.md captures Phase 4 closure evidence" }`
- acceptance criteria:
  - `VALIDATION.md` exists and maps requirements to evidence.
  - `STATE.md` includes required Phase 4 summary fields.
  - Closeout evidence is complete enough for phase completion decision.

## Dedicated verification section

### Automated pass/fail

- Run Task 1 checks; all must pass.
- Run Debug build command; must pass.
- Run Task 3 checks; all must pass.

Required PASS outputs:

- `PASS: all inventory dialogs accounted`
- `PASS: one resource per row`
- `PASS: STATE.md captures Phase 4 closure evidence`

### Manual pass/fail

- Tutorial startup stability:
  - `tutorial-startup@1024x768`: 3/3 pass
  - `tutorial-startup@1366x768`: 3/3 pass
  - `tutorial-startup@switch-loop`: pass
- Mission startup stability:
  - `mission-startup@1024x768`: 3/3 pass
  - `mission-startup@1366x768`: 3/3 pass
  - `mission-startup@switch-loop`: pass

## Regression checklist

- [ ] Inventory, allowlist, and dialog-producer artifacts generated.
- [ ] Coverage map has exactly one row per in-scope resource.
- [ ] Coverage gaps file is empty.
- [ ] Game Debug build passes.
- [ ] Every in-scope resource is enlarged and marked `scaled` or `fixed-geometry`.
- [ ] Tutorial and mission startup deterministic checks pass.
- [ ] Verification evidence is recorded in `VERIFICATION.md`.
- [ ] Validation artifact exists in `VALIDATION.md`.
- [ ] `STATE.md` contains Phase 4 results and evidence references.

## Success criteria

- No in-scope runtime dialog resource remains at original dimensions.
- Stable tutorial startup and mission startup are demonstrated with deterministic pass/fail evidence.
- Runtime stability improvements are documented in both `VALIDATION.md` and `STATE.md`.
- If low-level contingency is still needed, it is explicitly split into a follow-up plan instead of inflating this phase plan.

## PLANNING COMPLETE
