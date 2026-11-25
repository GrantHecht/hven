#!/usr/bin/env bash
# I18 TRAIN enumeration: DN-3's 238 roster cells x {D = default, K = never-default}, CO-RUN (CLAUDE.md section 7: counters and
# statuses only, one cell process per pinned physical core of $PHYS, MKL_NUM_THREADS=1; wall-clock informational), CAP 300 s per
# cell through cell.sh (a capped row is a wall-clock outcome: it fails train_check and is answered by a solo re-run). HOLDOUT is never run.
# usage: train_enum.sh <evidence-dir> <lane binary> <lane sha256>      run under: box-lane -- bash train_enum.sh ...
set -uo pipefail
source "$(dirname "$0")/env.sh"
EV=${1:?evidence dir}; BIN=$(realpath "${2:?lane binary}"); WANT=${3:?sha256}
ROSTER=$WT/docs/notes/data/2026-11-m8-dn3/roster.tsv
export HB=$WT/.scratch/hb CELL_BIN=$BIN
echo "FOREGROUND_START $(date -Is) lane=${BOX_LANE:-none} phys=$PHYS width=$JOBS"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$WANT" ]] || { echo LANE_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
grep -qi holdout "$ROSTER" && { echo "HOLDOUT_IN_ROSTER"; exit 3; }
EV=$(realpath -m "$EV"); mkdir -p "$EV/cells"
JOBF=$EV/jobs.txt; : > "$JOBF"
while IFS=$'\t' read -r pop lane prob; do
  for a in D:default K:never-default; do
    arm=${a%%:*}; label=${a#*:}
    [[ -f "$EV/cells/${pop}-${lane}-${prob}.$arm.result" ]] || echo "$lane $prob $label $EV/cells/${pop}-${lane}-${prob}.$arm" >> "$JOBF"
  done
done < "$ROSTER"
one() { local phys=($PHYS); CELL_CPU=${phys[$SLOT]} bash "$L/cell.sh" run "$1" "$2" "$3" 1 "$4" 300 > /dev/null; }
export -f one; export PHYS L
xargs -P "$JOBS" --process-slot-var=SLOT -L1 bash -c 'one "$@"' _ < "$JOBF"
echo "jobs: $(wc -l < "$JOBF"); results: $(ls "$EV/cells" | grep -c '\.result$'); capped: $(grep -l CAPPED "$EV"/cells/*.result 2>/dev/null | wc -l)"
echo "FOREGROUND_END $(date -Is)"
