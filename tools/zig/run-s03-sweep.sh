#!/usr/bin/env bash
# Run the full S03 Linux sweep: install-game + install-map-editor +
# test-resource-model (S03's slice-level gate) + every S02 regression tier
# (test-editor-kit + 13 MapEditor tiers).
#
# Writes one line per tier to zig-out/local-test/resource_editor/s03-sweep.log
# with the format: TIER=<name> EXIT=<code> ELAPSED_MS=<n> VERDICT=<PASS|FAIL>.
# Every tier must pass: the Map Editor tiers that open a map used to trap in
# CArray2D::SetZero on a Linux debug build, but 3e8afc8a7 fixed that, so a
# failure there is a regression, not a known pre-existing failure.
#
# Final line: S03 VERDICT=<PASS|FAIL>. PASS iff no tier recorded FAIL.
set -u

cd "$(dirname "$0")/../.." || exit 2
REPO_ROOT="$(pwd)"

LOG_DIR="${REPO_ROOT}/zig-out/local-test/resource_editor"
mkdir -p "${LOG_DIR}"
LOG="${LOG_DIR}/s03-sweep.log"
: > "${LOG}"

export BK_DEBUG_LOG=1

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

  local verdict="PASS"
  if [[ ${exit_code} -ne 0 ]]; then
    verdict="FAIL"
    # Keep the failing tier's stderr next to the log for diagnosis.
    cp "${tmp_err}" "${LOG_DIR}/${label}.stderr.log"
  fi

  printf 'TIER=%s EXIT=%d ELAPSED_MS=%d VERDICT=%s\n' \
    "${label}" "${exit_code}" "${elapsed}" "${verdict}" >> "${LOG}"

  rm -f "${tmp_err}"
}

# Base install first — required by every downstream tier.
run_step "install-game+install-map-editor" zig build install-game install-map-editor

# The S03 slice gate itself, then the S02 regression carpet.
run_step "test-resource-model" zig build test-resource-model

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

S03_VERDICT="PASS"
if grep -q 'VERDICT=FAIL' "${LOG}"; then
  S03_VERDICT="FAIL"
fi

printf 'S03 VERDICT=%s\n' "${S03_VERDICT}" >> "${LOG}"

# Mirror the final line to stdout so callers see the aggregate.
tail -n 1 "${LOG}"
