#!/usr/bin/env bash
# TRAIN enumeration (DN-3's 238 roster cells x {default (D), never-default (K)}), serial, one cell process at a time, THREADS=1 (core 2), CAP 300 s,
# through cell.sh. HOLDOUT is never run. usage: train_enum.sh <evidence-dir> <lane binary> <lane sha256>
#   Run under: flock /tmp/box-build.lock setsid -w bash train_enum.sh ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/kopt-1x
L=$WT/docs/notes/data/2026-11-m8-kopt-1x/legs
EV=${1:?evidence dir}; BIN=${2:?lane binary}; WANT=${3:?lane binary sha256}
ROSTER=$WT/docs/notes/data/2026-11-m8-dn3/roster.tsv
CAP=300
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "FOREGROUND_START $(date -Is)"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$WANT" ]] || { echo LANE_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
grep -qi holdout "$ROSTER" && { echo "HOLDOUT_IN_ROSTER"; exit 3; }
EV=$(realpath -m "$EV"); mkdir -p "$EV/cells"
n=0
while IFS=$'\t' read -r -u 3 pop lane prob; do
  tag="${pop}-${lane}-${prob}"
  for a in D:default K:never-default; do
    arm=${a%%:*}; label=${a#*:}
    [[ -f "$EV/cells/$tag.$arm.result" ]] || CELL_BIN=$BIN bash "$L/cell.sh" run "$lane" "$prob" "$label" 1 "$EV/cells/$tag.$arm" "$CAP" > /dev/null
  done
  n=$((n + 1))
done 3< "$ROSTER"
echo "cells run: $n $(date -Is)"
echo "FOREGROUND_END $(date -Is)"
