#!/usr/bin/env bash
# M8 walk cap (../DECLARATION.md, I4): ../../2026-11-m8-dn3/r4/sl/flip_enum.sh re-pointed to this fix's lane binary
# (build-wc/bin/hven_bench_cell), the same two labels: `default` (arm D: the library default, kAuto) and
# `never-default` (arm K: kNever set explicitly by DN-3's patch j). Every roster cell (DN-3's roster.tsv, 238 TRAIN
# cells; HOLDOUT is never run), `run` D then K, through DN-3's unchanged sl/cell.sh: serial, one cell process at a time,
# alone on the box under /tmp/box-build.lock, THREADS=1 (MKL_NUM_THREADS=1 OMP_NUM_THREADS=1, core 2), CAP 300 s. Then
# train_check.py against DN-3 r5's rows (the base, ../../2026-11-m8-dn3/r5/identity/cells) and, when given, the dev rows.
# usage: train_enum.sh <evidence-dir> <lane binary sha256> [<dev-dir> <movers.txt>]
#   Run as: flock /tmp/box-build.lock bash train_enum.sh ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
D3=$WT/docs/notes/data/2026-11-m8-dn3
L=$WT/docs/notes/data/2026-11-m8-walk-cap/legs
EV=${1:?evidence dir}
WANT=${2:?lane binary sha256}
DEV=${3:-}
MOV=${4:-}
BIN=$WT/build-wc/bin/hven_bench_cell
ROSTER=$D3/roster.tsv
CAP=300
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "FOREGROUND_START $(date -Is)"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$WANT" ]] || { echo LANE_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
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
if [[ -n "$DEV" ]]; then
  python3 "$L/train_check.py" "$EV" "$D3" "$DEV" "$MOV" > "$EV/train_check.txt" 2>&1; rc=$?
else
  python3 "$L/train_check.py" "$EV" "$D3" > "$EV/train_check.txt" 2>&1; rc=$?
fi
grep -E '^(SUMMARY|MOVER|  FAIL|I4)' "$EV/train_check.txt" | head -60
echo "train_check rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
