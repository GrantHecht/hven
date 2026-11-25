#!/usr/bin/env bash
# M8 dense gate (../DECLARATION.md, I4): DN-3 r4's sl/flip_enum.sh re-pointed to the dense-gate lane binary, the same two
# labels: `default` (arm D: the library default kAuto) and `never-default` (arm K: kNever explicit). Every roster cell
# (DN-3's 238 TRAIN cells, ../../2026-11-m8-dn3/roster.tsv; HOLDOUT is never run), D then K, through DN-3's unchanged
# ../../2026-11-m8-dn3/sl/cell.sh: serial, one cell process at a time, alone on the box under /tmp/box-build.lock,
# THREADS=1 (MKL_NUM_THREADS=1 OMP_NUM_THREADS=1, core 2), CAP 300 s (cell.sh's own watchdog). Then tally.py.
# usage: enum.sh <evidence-dir> <lane binary sha256> <expected-movers file> [dev-run dir: the rows must reproduce]
#   Run as: flock /tmp/box-build.lock bash enum.sh ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a44d4824b0beb0ea7
D3=$WT/docs/notes/data/2026-11-m8-dn3
DG=$WT/docs/notes/data/2026-11-m8-dense-gate
EV=${1:?evidence dir}
WANT=${2:?lane binary sha256}
MOVERS=${3:?expected movers file}
DEVDIR=${4:-}
EV=$(realpath -m "$EV"); MOVERS=$(realpath -m "$MOVERS")  # cell.sh runs from the lane clone
BIN=$WT/build-dg/dgbin/hven_bench_cell
ROSTER=$D3/roster.tsv
CAP=300
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "FOREGROUND_START $(date -Is)"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$WANT" ]] || { echo LANE_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
grep -qi holdout "$ROSTER" && { echo "HOLDOUT_IN_ROSTER"; exit 3; }
mkdir -p "$EV/cells"
n=0
while IFS=$'\t' read -r -u 3 pop lane prob; do
  tag="${pop}-${lane}-${prob}"
  for a in D:default K:never-default; do
    arm=${a%%:*}; label=${a#*:}
    [[ -f "$EV/cells/$tag.$arm.result" ]] || CELL_BIN=$BIN bash "$D3/sl/cell.sh" run "$lane" "$prob" "$label" 1 "$EV/cells/$tag.$arm" "$CAP" > /dev/null
  done
  n=$((n + 1))
done 3< "$ROSTER"
echo "cells run: $n $(date -Is)"
python3 "$DG/sl/tally.py" "$EV" "$D3/r5/identity" "$ROSTER" "$MOVERS" $DEVDIR > "$EV/tally.txt" 2>&1; rc=$?
cat "$EV/tally.txt" | tail -60
echo "tally rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
