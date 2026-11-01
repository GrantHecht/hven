#!/bin/bash
# G1 (U0 OFF) and U-1 (U0 ON): the 27-cell three-arm replay at this head, each engine one leg under
# the box lock, compared EXACT (no gate) against the SCW gates record. Usage: u0.sh off|on
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-a05b3c395d8efca12
S=$W/.scratch/a7
ARM=$1
case "$ARM" in
    off) LEVER="" ;;
    on) LEVER="--ws-algebra-rule measured-cost" ;;
    *) echo "bad arm"; exit 2 ;;
esac
D=$S/u0-$ARM
mkdir -p "$D"
BIN=$S/build-rel/bench/hven_sqp_corpus
CELLS="$(grep -v '^#' "$W/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
REC=$W/docs/notes/data/2026-10-m7-scw-gates/gates/replay
echo "U0 $ARM START $(date -Is) HEAD $(git -C "$W" rev-parse HEAD) DIRTY=$(git -C "$W" status --porcelain --untracked-files=no | wc -l)" | tee "$D/summary.txt"
worst=0
for eng in walk ssn ipm; do
    if pgrep -a hven_bench_cell; then echo "PGREP_NONEMPTY $eng" | tee -a "$D/summary.txt"; else echo "PGREP_EMPTY $eng" | tee -a "$D/summary.txt"; fi
    bash "$S/leg.sh" "u0-$ARM-$eng" taskset -c 2 "$BIN" --engine "$eng" --cells "$CELLS" $LEVER --csv "$D/head-$eng.csv"
    rc=$?
    cp "$S/logs/u0-$ARM-$eng.log" "$D/run-$eng.log"
    echo "ran head-$eng rc=$rc" | tee -a "$D/summary.txt"
    [ $rc -eq 0 ] || worst=1
done
: > "$D/comparison.txt"
for eng in walk ssn ipm; do
    python3 -B "$W/scripts/compare_replay.py" "$REC/head-$eng.csv" "$D/head-$eng.csv" \
        "SCW GATES RECORD HEAD-$eng vs A7 HEAD-$eng ($ARM, EXACT)" | tee -a "$D/comparison.txt"
    erc=${PIPESTATUS[0]}
    echo "EXACT_RC_$eng=$erc" | tee -a "$D/summary.txt"
    [ $erc -eq 0 ] || worst=1
done
echo "fired lines: $(grep -c 'ws_algebra_rule fired' "$D"/run-*.log | tr '\n' ' ')" | tee -a "$D/summary.txt"
echo "U0 $ARM END worst=$worst $(date -Is)" | tee -a "$D/summary.txt"
exit $worst
