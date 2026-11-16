#!/usr/bin/env bash
# copy of the code head (git archive, dep trees copied) is built Release (build-g6/mut) for hven_sqp_seam_tests,
# hven_drivers_tests and hven_interior_tests; each mutation (mutate.py) is applied to the COPY alone (the worktree is never
# touched), the three binaries are run in full, the failing test names are the kill list, and the mutated file is restored from
# the head.
# usage: mutations.sh <code-head sha> <out-dir>     Run as: flock /tmp/box-build.lock bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g6
D=$WT/docs/notes/data/2026-11-m8-ap5-g6
HEAD_SHA=${1:?code head sha}
OUT=${2:?out dir}
RAW=$WT/.scratch/g6-mutation
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-g6/tmp
cd "$WT" || exit 1
mkdir -p "$OUT" "$RAW"
SRC=$WT/build-g6/mut-src
# r3: the reused Release tree is wiped first. `git archive` stamps every file with the commit time, so an object built
# from a later edit of a file can look newer than the restored source and survive into the control build otherwise.
rm -rf "$WT/build-g6/mut" "$WT/build-g6/mut.configure.log"
rm -rf "$SRC"
mkdir -p "$SRC"
git archive "$HEAD_SHA" | tar -x -C "$SRC" || exit 1
rm -rf "$SRC/dep/eigen" "$SRC/dep/fmt"
cp -a dep/eigen "$SRC/dep/eigen" && cp -a dep/fmt "$SRC/dep/fmt" || exit 1
FILES="src/drivers/ipm_solver.cpp src/drivers/ipm_solver_globalization.cpp include/hven/detail/globalization/restoration.h include/hven/core/solver_counters.h include/hven/drivers/ipm_solver_types.h src/drivers/trace_writer.cpp"
build() { SRC=$SRC bash "$D/legs/build.sh" Release mut hven_sqp_seam_tests hven_drivers_tests hven_interior_tests > "$RAW/build-$1.log" 2>&1; }
runall() {
  local B=$WT/build-g6/mut/tests
  { timeout 900 "$B/hven_sqp_seam_tests" --gtest_brief=1 --gtest_fail_fast 2>&1; echo "seam rc=$?";
    timeout 900 "$B/drivers/hven_drivers_tests" --gtest_brief=1 --gtest_fail_fast 2>&1; echo "drivers rc=$?";
    timeout 900 "$B/interior/hven_interior_tests" --gtest_brief=1 --gtest_fail_fast 2>&1; echo "interior rc=$?";
    true; } > "$RAW/run-$1.log"
  sed -nE 's/^\[  FAILED  \] ([A-Za-z0-9_]+\.[A-Za-z0-9_\/]+)( .*)?$/\1/p' "$RAW/run-$1.log" | sort -u
}
echo "id,mutation,build,killed_by" > "$OUT/mutation-table.csv"
declare -A DESC=(
 [m1]="X-macro: dense_factorizations entry deleted" [m2]="X-macro: entries out of declaration order"
 [c1]="counters.factorizations not set (solve paths)" [c1b]="counters.factorizations not set (interrupted path; REPORTED)"
 [c2]="counters.symbolic_analyses not set" [c3]="soc_steps and watchdog_activations crossed at the recovery call"
 [c4]="watchdog_activations dropped" [c5]="recovery_depth_histogram dropped" [c6]="nonfinite_step_rollbacks dropped"
 [c7]="polish_ignored dropped" [c8]="payload_ignored dropped" [c9]="dense_solve_residual_refusals dropped"
 [c10]="dense_factorizations dropped" [c11]="restoration_entries sum dropped" [c12]="restoration_iters sum dropped"
 [c13]="restoration_entries last phase instead of the sum (REPORTED)"
 [p1]="phase funnel_width not written" [p2]="phase filter_size not written" [p3]="phase filter_resets not written"
 [p4]="phase monotone_switches not written" [p5]="phase monotone_iterations not written"
 [p6]="phase restoration_entries not written" [p7]="phase restoration_iters not written"
 [p8]="phase proximal primal not copied" [p9]="phase proximal dual not copied" [p10]="phase kkt_factor_status not copied"
 [p11]="acceptance append_diagnostics not called" [p12]="governor append_diagnostics not called"
 [p13]="restoration append_diagnostics not called" [p14]="last_ran_phase ignores ran"
 [p15]="IpmSolveRecord::counters not set" [p16]="ipm.phase.exit key still last_kkt_info")
build control; echo "control build rc=$?"
ctl=$(runall control | tr '\n' ' '); ctl="$ctl$(grep -E '^(seam|drivers|interior) rc=[1-9]' "$RAW/run-control.log" | tr '\n' ' ')"
echo "CONTROL failures: [${ctl}]"
echo "control,none,ok,${ctl:-NONE}" >> "$OUT/mutation-table.csv"
for id in m1 m2 c1 c1b c2 c3 c4 c5 c6 c7 c8 c9 c10 c11 c12 c13 p1 p2 p3 p4 p5 p6 p7 p8 p9 p10 p11 p12 p13 p14 p15 p16; do
  python3 "$D/legs/mutate.py" "$SRC" "$id" || { echo "MUTATE_FAIL $id"; echo "$id,${DESC[$id]},APPLY_FAIL," >> "$OUT/mutation-table.csv"; continue; }
  if build "$id"; then
    killed=$(runall "$id" | tr '\n' ' ')
    killed="$killed$(grep -E '^(seam|drivers|interior) rc=[1-9]' "$RAW/run-$id.log" | tr '\n' ' ')"
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
