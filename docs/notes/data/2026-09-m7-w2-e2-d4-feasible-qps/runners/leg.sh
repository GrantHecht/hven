#!/usr/bin/env bash
# E2-D4 box leg: flock (NO timeout, never wrapped), in-lock pgrep, markers, pipefail, rc.
set -o pipefail
label="$1"; shift
exec flock /tmp/box-build.lock bash -c '
set -o pipefail
label="$1"; shift
if pgrep -a -x hven_bench_cell; then echo "PGREP_NOT_EMPTY ${label}"; exit 97; fi
echo "PGREP_EMPTY ${label} $(date -Is)"
echo "FOREGROUND_START ${label} $(date -Is)"
"$@"; rc=$?
echo "FOREGROUND_END ${label} $(date -Is) rc=${rc}"
exit ${rc}
' _ "$label" "$@"
