#!/usr/bin/env bash
# M7 W2 M4 box-protocol wrapper: one leg, inside the shared build lock, with the
# markers emitted INSIDE the lock and the leg's exit status propagated.
#
# NO TIMEOUT AND NO FALLBACK. `flock` without -w queues; a wrapper that gave up
# on a timeout and ran anyway is what contaminated another lane's sweep.
set -o pipefail
LOCKFILE="${HVEN_BOX_LOCK:-/tmp/box-build.lock}"
label="$1"; shift
flock "$LOCKFILE" bash -c '
  set -o pipefail
  label="$1"; shift
  echo "FOREGROUND_START ${label} $(date -Is)"
  "$@"
  rc=$?
  echo "FOREGROUND_END ${label} $(date -Is) rc=${rc}"
  exit ${rc}
' _ "$label" "$@"
rc=$?
echo "WRAPPER_RC ${label} rc=${rc}"
exit ${rc}
