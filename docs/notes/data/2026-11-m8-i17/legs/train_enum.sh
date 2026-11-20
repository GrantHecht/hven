#!/usr/bin/env bash
# I17 TRAIN enumeration: DN-3's 238 roster cells x the given label pairs, serial, one cell process at a time, THREADS=1 (core 2), CAP 300 s, through cell.sh. HOLDOUT is never run.
# usage: train_enum.sh <worktree> <evidence-dir> <lane binary> <lane sha256> <label:ARMTAG>...     e.g. default:D never-default:K     (cell file = <tag>.<ARMTAG>)
#   LANE_FILTER=hven-sqp restricts the pass to that lane (the Always labels exist on the SQP lane only).
#   Run as: flock /tmp/box-build.lock setsid -w bash train_enum.sh ...      cell.sh runs in the worktree's private clone (HB).
set -uo pipefail
WT=${1:?worktree}; EV=${2:?evidence dir}; BIN=${3:?lane binary}; WANT=${4:?sha256}; shift 4
L=$(cd "$(dirname "$0")" && pwd)
ROSTER=$WT/docs/notes/data/2026-11-m8-dn3/roster.tsv
export HB=$WT/.scratch/hb TMPDIR=$WT/.scratch/tmp
CAP=300
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "FOREGROUND_START $(date -Is)"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$WANT" ]] || { echo LANE_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
grep -qi holdout "$ROSTER" && { echo "HOLDOUT_IN_ROSTER"; exit 3; }
EV=$(realpath -m "$EV"); mkdir -p "$EV/cells"
n=0
while IFS=$'\t' read -r -u 3 pop lane prob; do
  [[ -n "${LANE_FILTER:-}" && "$lane" != "$LANE_FILTER" ]] && continue
  tag="${pop}-${lane}-${prob}"
  for a in "$@"; do
    label=${a%%:*}; arm=${a#*:}
    [[ -f "$EV/cells/$tag.$arm.result" ]] || CELL_BIN=$BIN bash "$L/cell.sh" run "$lane" "$prob" "$label" 1 "$EV/cells/$tag.$arm" "$CAP" > /dev/null
  done
  n=$((n + 1))
done 3< "$ROSTER"
echo "cells run: $n labels: $* $(date -Is)"
echo "FOREGROUND_END $(date -Is)"
