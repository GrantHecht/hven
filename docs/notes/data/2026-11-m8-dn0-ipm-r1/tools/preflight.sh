#!/usr/bin/env bash
# M8 DN-0 IPM r1 pre-flight: refuse a leg unless the harness tree is clean at the declared commit, the pin
# stamp is 0a34c8a2, and every binary is the declared one (tools/expected.sha256, paths relative to HARNESS_ROOT).
# Usage: preflight.sh HARNESS_ROOT DECLARED_HARNESS_COMMIT
set -euo pipefail
HB="$1"; WANT="$2"
TOOLS="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
head="$(git -C "${HB}" rev-parse HEAD)"
[[ "${head}" == "${WANT}"* ]] || { echo "PREFLIGHT_REFUSED harness HEAD ${head} != ${WANT}"; exit 3; }
dirt="$(git -C "${HB}" status --porcelain --untracked-files=no)"
[[ -z "${dirt}" ]] || { echo "PREFLIGHT_REFUSED harness tree dirty: ${dirt}"; exit 3; }
(cd "${HB}" && sha256sum -c --quiet "${TOOLS}/expected.sha256") || { echo "PREFLIGHT_REFUSED binary sha mismatch"; exit 3; }
grep -q '^HVEN_PIN_SHA 0a34c8a2adba9e725351b3c9e23c14471d3fecb1$' "${HB}/.hven/install-0a34c8a2adba/HVEN_PIN.txt" \
    || { echo "PREFLIGHT_REFUSED pin stamp"; exit 3; }
echo "PREFLIGHT_OK max_sample_rate=$(cat /proc/sys/kernel/perf_event_max_sample_rate)"
