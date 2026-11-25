#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md, I4): the enumeration leg of ../../sl/adopt_run.sh (leg E) re-pointed to the flip
# lane binary, with two labels: `default` (arm D: the lane sets no dense policy, so the flipped library default reads)
# and `never-default` (arm K: kNever set explicitly by hb_patch_j.py). Every roster cell (../../roster.tsv, DN-3's 238
# TRAIN cells; HOLDOUT is never run), `run` D then K, through DN-3's unchanged ../../sl/cell.sh: serial, one cell process
# at a time, alone on the box under /tmp/box-build.lock, THREADS=1 (MKL_NUM_THREADS=1 OMP_NUM_THREADS=1, core 2),
# CAP 300 s (cell.sh's own watchdog). Then flip_identity.py -> EV/identity.txt.
# usage: flip_enum.sh <evidence-dir> <lane binary sha256>   Run as: flock /tmp/box-build.lock bash flip_enum.sh ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
D3=$WT/docs/notes/data/2026-11-m8-dn3
EV=${1:?evidence dir}
WANT=${2:?lane binary sha256}
BIN=$WT/build-flip/flipbin/hven_bench_cell
ROSTER=$D3/roster.tsv
CAP=300
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "FOREGROUND_START $(date -Is)"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$WANT" ]] || { echo LANE_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
cmp -s "$ROSTER" "$WT/docs/notes/data/2026-11-m8-dn0/raw/roster.tsv" || { echo ROSTER_DIFFERS_FROM_DN0; exit 4; }
grep -qi holdout "$ROSTER" && { echo "HOLDOUT_IN_ROSTER (S-H)"; exit 3; }
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
python3 "$D3/r4/sl/flip_identity.py" "$EV" "$D3" > "$EV/identity.txt" 2>&1; rc=$?
sed -n '/^I4a (flip/,$p' "$EV/identity.txt" | head -40
echo "identity rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
