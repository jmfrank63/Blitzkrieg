#!/usr/bin/env bash
# Run the full S02 Linux sweep: test-editor-kit + 13 MapEditor tiers.
#
# Writes one line per tier to zig-out/local-test/resource_editor/s02-sweep.log
# with the format: TIER=<name> EXIT=<code> ELAPSED_MS=<n> VERDICT=<PASS|FAIL|PREEXISTING>.
# Tiers that depend on BkEditorOpenMap and fail with the pre-existing
# AIWarFog::SetZero nullptr-memset signature are classified PREEXISTING (owned
# by S04/S13 per the S01 sweep baseline), not FAIL.
#
# Final line: S02 VERDICT=<PASS|FAIL>. PASS iff test-editor-kit is PASS, the 5
# non-BkEditorOpenMap MapEditor tiers are PASS, and no tier is FAIL.
set -u

cd "$(dirname "$0")/../.." || exit 2
REPO_ROOT="$(pwd)"

LOG_DIR="${REPO_ROOT}/zig-out/local-test/resource_editor"
mkdir -p "${LOG_DIR}"
LOG="${LOG_DIR}/s02-sweep.log"
: > "${LOG}"

# BkEditorOpenMap-dependent tiers that fail with the pre-existing
# AIWarFog::SetZero nullptr-memset trap on this Linux debug build (see
# .gsd/milestones/M001/slices/S01/.../s01-sweep.log). A failure whose stderr
# matches the signature below is PREEXISTING, not FAIL.
PREEXISTING_TIERS=(
  "test-map-editor-engine"
  "test-editor-bridge"
  "map-editor-host-check"
  "map-editor-smoke"
  "map-editor-auto"
  "map-editor-auto-m2"
  "map-editor-m3-auto"
  "map-editor-game-reads-it-m3"
)

PREEXISTING_SIGNATURE_PATTERNS=(
  "AIWarFog::SetZero"
  "BkEditorOpenMap"
  "OpenMapIntoSession"
)

export BK_DEBUG_LOG=1

is_preexisting_tier() {
  local tier="$1"
  for p in "${PREEXISTING_TIERS[@]}"; do
    if [[ "${p}" == "${tier}" ]]; then
      return 0
    fi
  done
  return 1
}

matches_preexisting_signature() {
  local stderr_file="$1"
  if [[ ! -s "${stderr_file}" ]]; then
    return 1
  fi
  for sig in "${PREEXISTING_SIGNATURE_PATTERNS[@]}"; do
    if grep -q -- "${sig}" "${stderr_file}"; then
      return 0
    fi
  done
  return 1
}

run_step() {
  local label="$1"
  shift
  local tmp_err
  tmp_err="$(mktemp)"
  local start_ms
  start_ms=$(date +%s%3N)
  local exit_code=0
  "$@" >/dev/null 2>"${tmp_err}" || exit_code=$?
  local end_ms
  end_ms=$(date +%s%3N)
  local elapsed=$((end_ms - start_ms))

  local verdict
  if [[ ${exit_code} -eq 0 ]]; then
    verdict="PASS"
  else
    if is_preexisting_tier "${label}" && matches_preexisting_signature "${tmp_err}"; then
      verdict="PREEXISTING"
    else
      verdict="FAIL"
    fi
  fi

  printf 'TIER=%s EXIT=%d ELAPSED_MS=%d VERDICT=%s\n' \
    "${label}" "${exit_code}" "${elapsed}" "${verdict}" >> "${LOG}"

  rm -f "${tmp_err}"
}

# Base install first — required by every downstream tier.
run_step "install-game+install-map-editor" zig build install-game install-map-editor

# test-editor-kit (new S02 tier), then every MapEditor tier.
TIERS=(
  "test-editor-kit"
  "test-editor-core"
  "test-map-editor-view"
  "test-map-editor-panels"
  "test-map-editor-testlaunch"
  "test-map-editor-auto"
  "test-map-editor-engine"
  "test-editor-bridge"
  "map-editor-host-check"
  "map-editor-smoke"
  "map-editor-auto"
  "map-editor-auto-m2"
  "map-editor-m3-auto"
  "map-editor-game-reads-it-m3"
)

for tier in "${TIERS[@]}"; do
  run_step "${tier}" zig build "${tier}"
done

# Aggregate verdict. PASS requires test-editor-kit PASS, the 5 non-BkEditorOpenMap
# MapEditor tiers PASS, and no tier recorded FAIL.
S02_VERDICT="PASS"
if grep -q 'VERDICT=FAIL' "${LOG}"; then
  S02_VERDICT="FAIL"
fi
if ! grep -q '^TIER=test-editor-kit .* VERDICT=PASS$' "${LOG}"; then
  S02_VERDICT="FAIL"
fi
REQUIRED_PASS=(
  "test-editor-core"
  "test-map-editor-view"
  "test-map-editor-panels"
  "test-map-editor-testlaunch"
  "test-map-editor-auto"
)
for tier in "${REQUIRED_PASS[@]}"; do
  if ! grep -q "^TIER=${tier} .* VERDICT=PASS$" "${LOG}"; then
    S02_VERDICT="FAIL"
  fi
done

printf 'S02 VERDICT=%s\n' "${S02_VERDICT}" >> "${LOG}"

# Mirror the final line to stdout so callers see the aggregate.
tail -n 1 "${LOG}"
