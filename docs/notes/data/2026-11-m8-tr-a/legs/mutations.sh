#!/usr/bin/env bash
# mutation list and one more binary (hven_sqp_tests, filtered to the suites that read the R4/trace changes). A `git archive`
# copy of the code head is built Release; each mutation (mutate.py) is applied to the COPY alone (the worktree is never
# touched), the binaries are run, the failing test names are the kill list, and the mutated file is restored from the head.
# usage: mutations.sh <code-head sha> <out-dir>     Run as: flock /tmp/box-build.lock bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-a
D=$WT/docs/notes/data/2026-11-m8-tr-a
HEAD_SHA=${1:?code head sha}
OUT=${2:?out dir}
RAW=${RAW_DIR:-$WT/.scratch/tra-mutation}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/build-tra/tmp GIT_CEILING_DIRECTORIES=$WT
cd "$WT" || exit 1
mkdir -p "$OUT" "$RAW"
SRC=$WT/.scratch/build-tra/mut-src
rm -rf "$WT/.scratch/build-tra/mut" "$WT/.scratch/build-tra/mut.configure.log" "$SRC"
mkdir -p "$SRC"
git archive "$HEAD_SHA" | tar -x -C "$SRC" || exit 1
rm -rf "$SRC/dep/eigen" "$SRC/dep/fmt"
cp -a dep/eigen "$SRC/dep/eigen" && cp -a dep/fmt "$SRC/dep/fmt" || exit 1
FILES="src/drivers/ipm_solver.cpp src/drivers/sqp_solver.cpp src/drivers/trace_writer.cpp src/drivers/console_trace_sink.cpp"
build() { SRC=$SRC bash "$D/legs/build.sh" Release mut hven_drivers_tests hven_interior_tests hven_sqp_tests > "$RAW/build-$1.log" 2>&1; }
runall() {
  local B=$WT/.scratch/build-tra/mut/tests
  {     timeout 900 "$B/drivers/hven_drivers_tests" --gtest_brief=1 --gtest_fail_fast 2>&1; echo "drivers rc=$?";
    timeout 900 "$B/interior/hven_interior_tests" --gtest_brief=1 --gtest_fail_fast 2>&1; echo "interior rc=$?";
    timeout 900 "$B/sqp/hven_sqp_tests" --gtest_brief=1 --gtest_fail_fast \
      --gtest_filter='JsonLinesTraceSink.*:SqpConsole.*' 2>&1; echo "sqp rc=$?";
    true; } > "$RAW/run-$1.log"
  sed -nE 's/^\[  FAILED  \] ([A-Za-z0-9_]+\.[A-Za-z0-9_\/]+)( .*)?$/\1/p' "$RAW/run-$1.log" | sort -u
}
echo "id,mutation,build,killed_by" > "$OUT/mutation-table.csv"
declare -A DESC=(
 [w1]="record field dropped from the writer" [w2]="wrong key (stationarity -> stationary)"
 [w3]="sid never advances" [w4]="solver_initialized ms leak onto the wire"
 [c1]="ladder rung leak: rows print at kSummary" [c2]="colour on under kNever"
 [c3]="kDetail line prints at kIterations" [o1]="off path: message builder runs before the sink test"
 [o2]="off path: SQP row built outside the guard" [v2]="IPM step norm not written"
 [v3]="IPM soc flag never set" [v4]="IPM factorizations stamp dropped at the main site"
 [t1]="SQP row without a step still reports one" [t2]="IPM row alpha is the maximum not the effective length")
build control; echo "control build rc=$?"
ctl=$(runall control | tr '\n' ' '); ctl="$ctl$(grep -E '^(seam|drivers|interior|sqp) rc=[1-9]' "$RAW/run-control.log" | tr '\n' ' ')"
echo "CONTROL failures: [${ctl}]"
echo "control,none,ok,${ctl:-NONE}" >> "$OUT/mutation-table.csv"
for id in ${MUT_IDS:-w1 w2 w3 w4 c1 c2 c3 o1 o2 v2 v3 v4 t1 t2}; do
  python3 "$D/legs/mutate.py" "$SRC" "$id" || { echo "MUTATE_FAIL $id"; echo "$id,${DESC[$id]},APPLY_FAIL," >> "$OUT/mutation-table.csv"; continue; }
  if build "$id"; then
    killed=$(runall "$id" | tr '\n' ' ')
    killed="$killed$(grep -E '^(seam|drivers|interior|sqp) rc=[1-9]' "$RAW/run-$id.log" | tr '\n' ' ')"
    echo "$id,${DESC[$id]},ok,${killed:-SURVIVES}" >> "$OUT/mutation-table.csv"
    echo "MUT $id: ${killed:-SURVIVES}"
  else
    echo "$id,${DESC[$id]},BUILD_FAIL," >> "$OUT/mutation-table.csv"; echo "MUT $id BUILD_FAIL"
  fi
  for f in $FILES; do git show "$HEAD_SHA:$f" > "$SRC/$f"; done
done
for f in $FILES; do git show "$HEAD_SHA:$f" | diff -q - "$SRC/$f" > /dev/null || echo "RESTORE_MISMATCH $f"; done
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench || echo "WORKTREE_MOVED"
echo "FOREGROUND_END $(date -Is)"
