#!/usr/bin/env bash
# M8 AP-5 G3 (../DECLARATION-r4.md): the mutation table. A scratch source copy of the code head (build-g3/mut-src-r4, git-ignored) is
# built Release (build-g3/mut) for hven_drivers_tests, hven_interior_tests and hven_sqp_tests; each mutation (legs/mutate.py) is applied
# to the COPY (the worktree is never touched), hven_drivers_tests and hven_interior_tests are run in full (mutations r and y also run
# hven_sqp_tests ColumnScalingDriver.*), the failing test names are the kill list, and the copy's files are restored from the head.
# usage: mutations.sh <code-head sha> <out-dir>     Run as: flock /tmp/box-build.lock bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ac15f1e91e3a3b6b2
D=$WT/docs/notes/data/2026-11-m8-ap5-g3
HEAD_SHA=${1:?code head sha}
OUT=${2:?out dir}
RAW=$WT/.scratch/g3-mutation-r4
FILES="src/drivers/sqp_solver.cpp src/drivers/ipm_solver.cpp include/hven/detail/drivers/ipm_solver_impl.h src/model/problem.cpp"
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-g3/tmp
cd "$WT" || exit 1
mkdir -p "$OUT" "$RAW"
SRC=$WT/build-g3/mut-src-r4
if [ ! -d "$SRC" ]; then
  mkdir -p "$SRC"
  git archive "$HEAD_SHA" | tar -x -C "$SRC" || exit 1
  rm -rf "$SRC/dep/eigen" "$SRC/dep/fmt"
  cp -a dep/eigen "$SRC/dep/eigen" && cp -a dep/fmt "$SRC/dep/fmt" || exit 1
fi
build() { SRC=$SRC BUILD_ROOT=$WT/build-g3 bash "$D/legs/build.sh" Release mut-r4 hven_drivers_tests hven_interior_tests hven_sqp_tests > "$RAW/build-$1.log" 2>&1; }
runall() {
  { timeout 900 "$WT/build-g3/mut-r4/tests/drivers/hven_drivers_tests" --gtest_brief=1 2>&1; echo "drivers rc=$?"; timeout 900 "$WT/build-g3/mut-r4/tests/interior/hven_interior_tests" --gtest_brief=1 2>&1; echo "interior rc=$?"; } > "$RAW/run-$1.log"
  if [ "$1" = r ]; then
    { timeout 900 "$WT/build-g3/mut-r4/tests/sqp/hven_sqp_tests" --gtest_filter='ColumnScalingDriver.*' --gtest_brief=1 2>&1; echo "sqp rc=$?"; } >> "$RAW/run-$1.log"
  fi
  if [ "$1" = y ] || [ "$1" = control ]; then
    { timeout 900 "$WT/build-g3/mut-r4/tests/sqp/hven_sqp_tests" --gtest_filter='F7ColdScaleSmoke.TheBestIterates*' --gtest_brief=1 2>&1; echo "sqp rc=$?"; } >> "$RAW/run-$1.log"
  fi
  sed -nE 's/^\[  FAILED  \] ([A-Za-z0-9_]+\.[A-Za-z0-9_\/]+)( .*)?$/\1/p' "$RAW/run-$1.log" | sort -u
}
echo "id,mutation,build,killed_by" > "$OUT/mutation-table.csv"
declare -A DESC=(
 [a]="ipm arming ignores callback and token" [b]="ipm no stop-path tracking" [c]="ipm no kStopReturnBest substitution"
 [d]="ipm callback action never sets stop_return_best" [e]="ipm best published on an optimal exit" [f]="ipm best step_norm zeroed"
 [g]="ipm best ce/ci rows not published" [h]="ipm best-return in restoration keeps the restoration-flagged exit" [i]="ipm ordinary tracking only under the option"
 [j]="ipm step norm only under a callback" [k]="ipm best iteration index zeroed"
 [l]="sqp tracker arming ignores callback and token" [m]="sqp post-push poll ignores kStopReturnBest" [n]="sqp best step_norm never filled"
 [o]="sqp best kept on an optimal exit" [p]="sqp restoration forwarder drops the best request" [q]="sqp best exit leaves returned_pre_restoration_point"
 [r]="sqp caller_point drops the column map" [s]="sqp kFinishCurrent ignores kStopReturnBest" [t]="sqp restoration-seed exit ignores kStopReturnBest"
 [u]="sqp restoration-qp exit ignores kStopReturnBest" [v]="problem does not map best_iterate's row blocks"
 [w]="ipm best not published on a substituted kAcceptable exit" [x]="ipm recorded flag keeps the 1e10 sentinel" [y]="sqp best_iterate inequality prices not sign-swept"
 [z]="ipm tracker armed-ness read live, not latched" [A]="ipm non-finite incumbent never yields to a finite criterion" [B]="sqp tracker armed-ness read live, not latched")
build control; echo "control build rc=$?"
ctl=$(runall control); echo "CONTROL failures: [${ctl}]"
echo "control,none,ok,${ctl:-NONE}" >> "$OUT/mutation-table.csv"
for id in ${MUT_IDS:-a b c d e f g h i j k l m n o p q r s t u v w x y z A B}; do
  python3 "$D/legs-r4/mutate.py" "$SRC" "$id" || { echo "MUTATE_FAIL $id"; echo "$id,${DESC[$id]},APPLY_FAIL," >> "$OUT/mutation-table.csv"; continue; }
  if build "$id"; then
    killed=$(runall "$id" | tr '\n' ' ')
    killed="$killed$(grep -E '^(drivers|interior|sqp) rc=[1-9]' "$RAW/run-$id.log" | tr '\n' ' ')"
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
