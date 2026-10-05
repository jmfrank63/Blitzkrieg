#!/usr/bin/env bash
# The Resource Editor's Linux sweep: the installs, the resource tiers and every
# Map Editor tier AGENTS.md lists. It replaces run-s02-sweep.sh and
# run-s03-sweep.sh.
#
# Writes one line per tier to zig-out/local-test/resource_editor/resource-sweep.log:
#   TIER=<name> EXIT=<code> ELAPSED_MS=<n> RESULT=<PASS|FAIL>[ HINT=...]
# and a last line VERDICT=<PASS|FAIL>. Any failing tier is FAIL: nothing is
# classified as pre-existing. A failing tier's stderr is kept next to the log
# as <tier>.stderr.log. Exits 0 only on VERDICT=PASS.
#
# The scripted Map Editor runs (map-editor-smoke, -auto, -auto-m2, -m3-auto)
# drop OS mouse events and run in a hidden, unfocusable window, so the real
# pointer should not decide them. If one fails anyway, HINT= quotes the
# pointer state the editor printed (global mouse, mouse focus, OS events that
# reached the window), so the log tells whether the pointer was involved.
#
# BK_SWEEP_TIER_TIMEOUT (seconds, default 3600) bounds each tier, so a hung
# engine or window is a FAIL with EXIT=124 rather than a sweep that never ends.
# An exclusive flock on resource-sweep.lock serialises concurrent sweeps.
set -u

cd "$(dirname "$0")/../.." || exit 2
REPO_ROOT="$(pwd)"

LOG_DIR="${REPO_ROOT}/zig-out/local-test/resource_editor"
mkdir -p "${LOG_DIR}" || exit 2

# Two sweeps at once share zig-out and the log, so each makes the other's
# tiers fail. The second one waits for the first instead.
exec 9>"${LOG_DIR}/resource-sweep.lock" || exit 2
flock 9 || exit 2

LOG="${LOG_DIR}/resource-sweep.log"
: > "${LOG}" || exit 2
rm -f "${LOG_DIR}"/*.stderr.log

TIER_TIMEOUT="${BK_SWEEP_TIER_TIMEOUT:-3600}"

export BK_DEBUG_LOG=1

# The pointer state lines the scripted runs print on FAIL (smoke.zig's
# printState and printNote), flattened to one field.
pointer_hint() {
  local err_file="$1"
  local found
  found="$(grep -o -E 'global mouse [-0-9.]+,[-0-9.]+ buttons [0-9]+|mouse focus [a-z]+|[0-9]+ OS event\(s\) reached the window' "${err_file}" | sort -u | paste -sd ';' -)"
  if [[ -z "${found}" ]]; then
    printf 'no-pointer-state-printed'
  else
    printf '%s' "${found// /_}"
  fi
}

run_step() {
  local label="$1"
  shift
  local tmp_err
  tmp_err="$(mktemp)"
  local start_ms
  start_ms=$(date +%s%3N)
  local exit_code=0
  # 9>&- keeps the lock out of the tier: an orphaned tier process must not
  # hold it and block the next sweep forever.
  timeout "${TIER_TIMEOUT}" "$@" >/dev/null 2>"${tmp_err}" 9>&- || exit_code=$?
  local end_ms
  end_ms=$(date +%s%3N)
  local elapsed=$((end_ms - start_ms))

  local result="PASS"
  local hint=""
  if [[ ${exit_code} -ne 0 ]]; then
    result="FAIL"
    cp "${tmp_err}" "${LOG_DIR}/${label}.stderr.log"
    case "${label}" in
      map-editor-smoke|map-editor-auto|map-editor-auto-m2|map-editor-m3-auto)
        hint=" HINT=pointer:$(pointer_hint "${tmp_err}")" ;;
    esac
    if [[ ${exit_code} -eq 124 ]]; then
      hint="${hint} HINT=timeout-after-${TIER_TIMEOUT}s"
    fi
  fi

  printf 'TIER=%s EXIT=%d ELAPSED_MS=%d RESULT=%s%s\n' \
    "${label}" "${exit_code}" "${elapsed}" "${result}" "${hint}" >> "${LOG}"
  printf '%s %s (%d ms)\n' "${result}" "${label}" "${elapsed}"

  rm -f "${tmp_err}"
}

# The installs first: the engine tiers run from the game installation.
run_step "install-game+install-map-editor" zig build install-game install-map-editor

TIERS=(
  "test-resource-model"
  "test-resource-core"
  "test-resource-bridge"
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
  run_step "${tier}" zig build "${tier}" -Dtest-mode=run
done

# The preview captures test-resource-bridge measured (one line per capture:
# the non-black-non-magenta share, the share changed against the empty
# frame, the TGA's path), so the sweep log carries the graphics evidence too.
# The tier rewrites the file on every run; a host without a GPU device has none.
PREVIEW_LOG="${LOG_DIR}/t02/preview-scene/preview.log"
if [[ -s "${PREVIEW_LOG}" ]]; then
  sed 's/^/PREVIEW: /' "${PREVIEW_LOG}" >> "${LOG}"
else
  printf 'PREVIEW: none written (expected only on a host without a GPU device)\n' >> "${LOG}"
fi

VERDICT="PASS"
if grep -q 'RESULT=FAIL' "${LOG}"; then
  VERDICT="FAIL"
fi

printf 'VERDICT=%s\n' "${VERDICT}" >> "${LOG}"
tail -n 1 "${LOG}"

[[ "${VERDICT}" == "PASS" ]]
