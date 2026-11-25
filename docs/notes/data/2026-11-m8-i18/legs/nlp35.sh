#!/usr/bin/env bash
# I18 nlp-35 leg: cutest-nlp-35 v1 TRAIN, 23 problems x {hven-ipm, hven-sqp}, label `default` (nlp35-cells.csv, PF-3's cells.csv
# without its hs rows and without four hven-sqp cells, MSS2, MSS3, READING8, TAX213322, which run past the 300 s cap inside five majors
# and so cannot be counter rows; HOLDOUT is not named). `full` cells run to their own end (--budget-iters 0); the other 8 `trunc30` cells (deadline
# class: past 60 s at PF-3's frozen baseline) run to a FIXED budget of 5 majors (--budget-iters 5; at 30 hven-sqp TAX53322 reached the cap under co-run), which is deterministic, so every
# row is a counter row and the leg co-runs (one cell per pinned physical core of $PHYS, MKL_NUM_THREADS=1), CAP 300 s through cell.sh.
# usage: nlp35.sh <evidence-dir> <lane binary> <lane sha256>      run under: box-lane -- bash nlp35.sh ...
set -uo pipefail
source "$(dirname "$0")/env.sh"
EV=${1:?evidence dir}; BIN=$(realpath "${2:?lane binary}"); WANT=${3:?sha256}
CELLS=$L/nlp35-cells.csv
export HB=$WT/.scratch/hb CELL_BIN=$BIN
echo "FOREGROUND_START $(date -Is) lane=${BOX_LANE:-none} phys=$PHYS width=$JOBS"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$WANT" ]] || { echo LANE_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$CELLS") -eq 42 ]] || { echo CELL_COUNT; exit 4; }
for p in $(cut -d, -f2 "$CELLS" | sort -u); do
  [[ -d "$HB/.scratch/h3/decoded/$p" ]] || { echo "NOT_DECODED $p (nlp35_setup.sh)"; exit 4; }
done
EV=$(realpath -m "$EV"); mkdir -p "$EV/cells"
JOBF=$EV/jobs.txt; : > "$JOBF"
while IFS=, read -r lane prob mode; do
  b=0; [[ $mode == trunc30 ]] && b=5
  [[ -f "$EV/cells/$lane-$prob.result" ]] || echo "$lane $prob $b $EV/cells/$lane-$prob" >> "$JOBF"
done < "$CELLS"
one() { local phys=($PHYS); CELL_CPU=${phys[$SLOT]} CELL_BUDGET=$3 bash "$L/cell.sh" run "$1" "$2" default 1 "$4" 300 > /dev/null; }
export -f one; export PHYS L
xargs -P "$JOBS" --process-slot-var=SLOT -L1 bash -c 'one "$@"' _ < "$JOBF"
echo "jobs: $(wc -l < "$JOBF"); results: $(ls "$EV/cells" | grep -c '\.result$'); capped: $(grep -l CAPPED "$EV"/cells/*.result 2>/dev/null | wc -l)"
echo "FOREGROUND_END $(date -Is)"
