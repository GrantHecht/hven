#!/usr/bin/env bash
# AP-2 B7: the interior-point engine on the 27 replay cells (the corpus binary's --engine interior, the
# three fixed-variable treatments), arm A (build-b7e/armAbin, the B7 base) against the B7 head
# (build-b7e/armBbin, frozen by run_all.sh or here), twice:
#   default -- the declared default event: interior_check.py --check against ../interior/interior-declared.txt
#              (the complete declared (row, column) set exactly; non-float values exactly);
#   pin     -- arm B under HVEN_LEG_PRE_B7_INFINITE_BOUND=1 (bench/ipm_corpus_leg.cpp's pre-event pin):
#              scripts/compare_replay.py EXACT, 0 differences (the pre-B7 reading is reachable, bit for bit).
# usage: interior.sh <evidence-dir> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash interior.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
EV=${1:?evidence dir}
EXPECT=${2:?expected arm B sha256}
DECL=$WT/docs/notes/data/2026-11-m8-ap2-b7/interior/interior-declared.txt
A=$WT/build-b7e/armAbin/hven_sqp_corpus
B=$WT/build-b7e/armBbin/hven_sqp_corpus
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
if [ ! -f "$B" ]; then mkdir -p "$(dirname "$B")"; cp -p "$WT/build-b7e/rel/bench/hven_sqp_corpus" "$B" || exit 1; fi
GOT=$(sha256sum "$B" | cut -d' ' -f1)
sha256sum "$A" "$B"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
unset HVEN_LEG_PRE_B7_INFINITE_BOUND
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
worst=0
"$A" --engine interior --cells "$CELLS" --csv "$EV/interior-A.csv" > "$EV/interior-run-A.txt" 2>&1
rc=$?; echo "arm A rc=$rc"; [ $rc -gt $worst ] && worst=$rc
"$B" --engine interior --cells "$CELLS" --csv "$EV/interior-B.csv" > "$EV/interior-run-B.txt" 2>&1
rc=$?; echo "arm B (default) rc=$rc"; [ $rc -gt $worst ] && worst=$rc
HVEN_LEG_PRE_B7_INFINITE_BOUND=1 "$B" --engine interior --cells "$CELLS" --csv "$EV/interior-B-pin.csv" > "$EV/interior-run-B-pin.txt" 2>&1
rc=$?; echo "arm B (pre-B7 pin) rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$EV/interior-A.csv" "$EV/interior-B.csv" "interior-default" > "$EV/interior-comparison-default.txt" 2>&1
echo "compare default rc=$?"; head -1 "$EV/interior-comparison-default.txt"
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b7/legs/interior_check.py" --list "$EV/interior-A.csv" "$EV/interior-B.csv" > "$EV/interior-differences.txt" 2>&1
python3 "$WT/docs/notes/data/2026-11-m8-ap2-b7/legs/interior_check.py" --check "$DECL" "$EV/interior-A.csv" "$EV/interior-B.csv" > "$EV/interior-check.txt" 2>&1
rc=$?; tail -1 "$EV/interior-check.txt"; echo "check rc=$rc"; [ $rc -gt $worst ] && worst=$rc
python3 scripts/compare_replay.py "$EV/interior-A.csv" "$EV/interior-B-pin.csv" "interior-pin" > "$EV/interior-comparison-pin.txt" 2>&1
rc=$?; tail -1 "$EV/interior-comparison-pin.txt"; echo "compare pin rc=$rc"; [ $rc -gt $worst ] && worst=$rc
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
