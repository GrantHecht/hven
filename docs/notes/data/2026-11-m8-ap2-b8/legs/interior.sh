#!/usr/bin/env bash
# AP-2 B8 (B7 riders' instrument, its third check -- B7's declared promotion -- dropped): the interior-point engine (the corpus binary's --engine interior, the three fixed-variable
# treatments), three comparisons:
#   unmoved   -- arm A (8089104c, build-b8/armAbin) vs arm B (the rider head, build-b8/armBbin) on the 27
#                replay cells at the defaults: scripts/compare_replay.py EXACT, 0 differences (the riders move
#                no interior row);
#   promoted  -- arm B under the PROMOTED baseline's own invocation (its 11-cell list) against
#                bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv: compare_replay.py EXACT, 0
#                differences (the committed baseline is reproduced by the evidence binary);
#   declared  -- the frozen predecessor bench/baselines/2026-09-t8-ipm-leg/interior_baseline.csv against the
#                promoted file: legs/interior_check.py --check (B7's instrument, byte copy) against B7's
#                declared list (2026-11-m8-ap2-b7/interior/interior-declared.txt, read, never written):
#                exactly B7's 237 (row, column) pairs and the 36 non-float values (the promotion is B7's
#                declared event and nothing else).
# usage: interior.sh <evidence-dir> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash interior.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
EV=${1:?evidence dir}
EXPECT=${2:?expected arm B sha256}
PROMOTED=$WT/bench/baselines/2026-11-m8-b7-ipm-leg/interior_baseline.csv
A=$WT/build-b8/armAbin/hven_sqp_corpus
B=$WT/build-b8/armBbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
if [ ! -f "$B" ]; then mkdir -p "$(dirname "$B")"; cp -p "$WT/build-b8/rel/bench/hven_sqp_corpus" "$B" || exit 1; fi
GOT=$(sha256sum "$B" | cut -d' ' -f1)
sha256sum "$A" "$B" "$PROMOTED"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
unset HVEN_LEG_PRE_B7_INFINITE_BOUND
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
BASE_CELLS=$(grep -m1 '^# invocation:' "$PROMOTED" | sed -E 's/.*--cells ([^ ]+).*/\1/')
echo "baseline cells: $BASE_CELLS"
worst=0
"$A" --engine interior --cells "$CELLS" --csv "$EV/interior-A.csv" > "$EV/interior-run-A.txt" 2>&1
rc=$?; echo "arm A rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$CELLS" --csv "$EV/interior-B.csv" > "$EV/interior-run-B.txt" 2>&1
rc=$?; echo "arm B rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$BASE_CELLS" --csv "$EV/interior-B-baseline.csv" > "$EV/interior-run-B-baseline.txt" 2>&1
rc=$?; echo "arm B (baseline invocation) rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$EV/interior-A.csv" "$EV/interior-B.csv" "interior-unmoved" > "$EV/interior-comparison-unmoved.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-unmoved.txt"; echo "compare unmoved rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$PROMOTED" "$EV/interior-B-baseline.csv" "interior-promoted" > "$EV/interior-comparison-promoted.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-promoted.txt"; echo "compare promoted rc=$rc"; [ $rc -gt $worst ] && worst=$rc
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
