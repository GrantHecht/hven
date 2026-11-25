#!/usr/bin/env bash
# M8 PF-1 (DECLARATION P15): THE M1-REVERTED ARM. The brief's discharge reads the cell "with qp_failure_budget = 1"
# (M1 3f2f6e35 raised it to 3; today SqpAdvancedOptions::max_consecutive_qp_failures). The code head's `git archive`
# export at build-pf1e/m1-src with that ONE default set back to 1 (fail closed unless exactly one line changes),
# Release with legs/build.sh's flags at build-pf1e/m1arm, then the one cell f7_n1000_path_corrupted on its corpus
# binary. Criterion: its 13 asserted columns equal the declared mover row (census-movers.csv) EXACTLY, i.e. the
# budget is never consulted.
# usage: m1arm.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash m1arm.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa2c5c3a798557d0e
L=$WT/docs/notes/data/2026-11-m8-pf1/legs
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
rm -rf "$WT/build-pf1e/m1-src" "$WT/build-pf1e/m1arm"
mkdir -p "$WT/build-pf1e/m1-src"
git archive "$HEAD_SHA" | tar -x -C "$WT/build-pf1e/m1-src" || { echo ARCHIVE_FAIL; exit 1; }
rm -rf "$WT/build-pf1e/m1-src/dep/eigen" "$WT/build-pf1e/m1-src/dep/fmt"
cp -a dep/eigen "$WT/build-pf1e/m1-src/dep/eigen" && cp -a dep/fmt "$WT/build-pf1e/m1-src/dep/fmt" || exit 1
H=$WT/build-pf1e/m1-src/include/hven/drivers/sqp_options.h
n=$(grep -c '^    Index max_consecutive_qp_failures = 3;$' "$H")
[ "$n" = 1 ] || { echo "M1_PATCH_SITE_COUNT $n"; exit 1; }
sed -i 's/^    Index max_consecutive_qp_failures = 3;$/    Index max_consecutive_qp_failures = 1;/' "$H"
diff <(git show "$HEAD_SHA:include/hven/drivers/sqp_options.h") "$H" | tee "$EV/m1-patch.diff"
[ "$(diff <(git show "$HEAD_SHA:include/hven/drivers/sqp_options.h") "$H" | grep -c '^[<>]')" = 2 ] || { echo M1_PATCH_NOT_ONE_LINE; exit 1; }
SRC=$WT/build-pf1e/m1-src bash "$L/build.sh" Release m1arm hven_sqp_corpus > "$EV/m1-build.log" 2>&1 || { echo M1_BUILD_FAIL; tail -20 "$EV/m1-build.log"; exit 1; }
export MKL_NUM_THREADS=1
"$WT/build-pf1e/m1arm/bench/hven_sqp_corpus" --engine walk --cells f7_n1000_path_corrupted --csv "$EV/m1-row.csv" > "$EV/m1-run.txt" 2>&1
echo "run rc=$?"
got=$(grep -v '^#' "$EV/m1-row.csv" | grep '^f7_n1000_path_corrupted,' | cut -d, -f1-13)
want=$(grep -v '^#' "$WT/docs/notes/data/2026-11-m8-pf1/census-movers.csv" | grep '^f7_n1000_path_corrupted,' | cut -d, -f1-13)
echo "got : $got"; echo "want: $want"
rc=0
if [ -n "$got" ] && [ "$got" = "$want" ]; then echo M1ARM_AS_DECLARED; else echo M1ARM_NOT_AS_DECLARED; rc=1; fi
echo "FOREGROUND_END $(date -Is)"
exit $rc
