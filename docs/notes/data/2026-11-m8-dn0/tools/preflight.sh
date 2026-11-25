#!/usr/bin/env bash
# M8 DN-0 pre-flight: refuse a leg unless the harness tree is clean at the declared commit and
# every binary is the declared one (tools/expected.sha256, paths relative to HARNESS_ROOT).
# Usage: preflight.sh HARNESS_ROOT DECLARED_HARNESS_COMMIT
set -euo pipefail
HB="$1"; WANT="$2"
TOOLS="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
head="$(git -C "${HB}" rev-parse HEAD)"
[[ "${head}" == "${WANT}"* ]] || { echo "PREFLIGHT_REFUSED harness HEAD ${head} != ${WANT}"; exit 3; }
dirt="$(git -C "${HB}" status --porcelain --untracked-files=no)"
[[ -z "${dirt}" ]] || { echo "PREFLIGHT_REFUSED harness tree dirty: ${dirt}"; exit 3; }
(cd "${HB}" && sha256sum -c --quiet "${TOOLS}/expected.sha256") || { echo "PREFLIGHT_REFUSED binary sha mismatch"; exit 3; }
grep -q '^HVEN_PIN_SHA 58dc14bc54a4d4225c6903ff73e3e9722e23ae87$' "${HB}/.hven/install-58dc14bc54a4/HVEN_PIN.txt" \
    || { echo "PREFLIGHT_REFUSED pin stamp"; exit 3; }
echo PREFLIGHT_OK
