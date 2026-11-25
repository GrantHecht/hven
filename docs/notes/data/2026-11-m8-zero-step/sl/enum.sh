#!/usr/bin/env bash
# I4 (one arm): every roster cell (DN-3's 238 TRAIN cells; HOLDOUT is never run) x {default, never-default} through cell.sh:
# serial, one cell process at a time, THREADS=1, core 2, CAP 300 s.
# usage: enum.sh <A|B> <evidence dir> <lane binary sha256>   Run as: flock /tmp/box-build.lock bash enum.sh ...
set -uo pipefail
. "$(dirname "$0")/../legs/env.sh"
ARM=${1:?A|B}; EV=$(realpath -m "${2:?evidence dir}"); WANT=${3:?lane binary sha256}
BIN=$B/lane$ARM/hven_bench_cell
CAP=300
pgrep_guard
echo "FOREGROUND_START $(date -Is)"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$WANT" ]] || { echo LANE_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
grep -qi holdout "$ROSTER" && { echo HOLDOUT_IN_ROSTER; exit 3; }
mkdir -p "$EV/cells"
n=0
while IFS=$'\t' read -r -u 3 pop lane prob; do
  tag="${pop}-${lane}-${prob}"
  for a in D:default K:never-default; do
    arm=${a%%:*}; label=${a#*:}
    [[ -f "$EV/cells/$tag.$arm.result" ]] || CELL_BIN=$BIN bash "$E/sl/cell.sh" run "$lane" "$prob" "$label" 1 "$EV/cells/$tag.$arm" "$CAP" > /dev/null
  done
  n=$((n + 1))
done 3< "$ROSTER"
echo "cells run: $n $(date -Is)"
echo "FOREGROUND_END $(date -Is)"
