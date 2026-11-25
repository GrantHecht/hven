#!/usr/bin/env bash
# WA-1 rebase check (not a DECLARATION leg; ../../REBASE.md): the rebased head onto origin/m8 bc8ed0e12 (RS-4: include spelling and
# repo-wide clang-format). Release + Debug full suites, then the 27-cell replay x walk/ssn/ipm once, compared byte-exact against the
# run-1 B leg rows (../../replay/B-k1-<mode>.csv). Run under: flock /tmp/box-build.lock setsid -w bash rebase.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1
E=$WT/docs/notes/data/2026-11-m8-wa1
L=$E/legs
O=$L/rebase-bc8ed0e12
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
echo "FOREGROUND_START $(date -Is)"
echo "HEAD $(git -C "$WT" rev-parse HEAD)"
bash "$L/build.sh" "$WT" Release rel || exit 1
bash "$L/build.sh" "$WT" Debug dbg || exit 1
bash "$L/ctest.sh" "$WT" rel "$O/ctest-rel.log" || exit 1
bash "$L/ctest.sh" "$WT" dbg "$O/ctest-dbg.log" || exit 1
mkdir -p "$WT/build-wa/bin/R" && cp -p "$WT/build-wa/rel/bench/hven_sqp_corpus" "$WT/build-wa/bin/R/" || exit 1
bash "$L/replay.sh" "$O" R "$WT/build-wa/bin/R/hven_sqp_corpus" 1 || exit 1
worst=0
for M in walk ssn ipm; do
  python3 "$WT/scripts/compare_replay.py" "$E/replay/B-k1-$M.csv" "$O/R-k1-$M.csv" "rebased-vs-run1-B-$M" > "$O/comparison-$M.txt" 2>&1
  tail -1 "$O/comparison-$M.txt"; grep -q "differences: 0$" "$O/comparison-$M.txt" || worst=1
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
