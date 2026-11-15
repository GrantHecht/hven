#!/usr/bin/env bash
# M8 AP-5 G5 (../DECLARATION.md): copied from ../../2026-11-m8-ap5-g4/legs-r4/mutations.sh, re-pointed; G5 files, ids a..v, hven_sqp_tests (filtered) added.
# Was: M8 AP-5 G4 r4 (../DECLARATION-r4.md): copied from ../legs-r3/mutations.sh with -r3 -> -r4 and g4r3 -> g4r4 in code lines only; nothing else.
# M8 AP-5 G4 r3 (../DECLARATION-r3.md): copied from ../legs/mutations.sh; changes: legs-r3 build.sh and mutate.py, ids s..y z Z added, build-g5/mut wiped before the control, raw .scratch/g4r3-mutation. Older provenance lines below are historical.
# M8 AP-5 G4 (../DECLARATION.md): the mutation table, on the pattern of ../../2026-11-m8-ap5-g2/legs-r3/mutations.sh. A scratch
# copy of the code head (git archive, dep trees copied) is built Release (build-g5/mut) for hven_sqp_seam_tests,
# hven_drivers_tests and hven_interior_tests; each mutation (mutate.py) is applied to the COPY alone (the worktree is never
# touched), the three binaries are run in full, the failing test names are the kill list, and the mutated file is restored from
# the head.
# usage: mutations.sh <code-head sha> <out-dir>     Run as: flock /tmp/box-build.lock bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/g5
D=$WT/docs/notes/data/2026-11-m8-ap5-g5
HEAD_SHA=${1:?code head sha}
OUT=${2:?out dir}
RAW=$WT/.scratch/g5-mutation
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-g5/tmp
cd "$WT" || exit 1
mkdir -p "$OUT" "$RAW"
SRC=$WT/build-g5/mut-src
# r3: the reused Release tree is wiped first. `git archive` stamps every file with the commit time, so an object built
# from a later edit of a file can look newer than the restored source and survive into the control build otherwise.
rm -rf "$WT/build-g5/mut" "$WT/build-g5/mut.configure.log"
rm -rf "$SRC"
mkdir -p "$SRC"
git archive "$HEAD_SHA" | tar -x -C "$SRC" || exit 1
rm -rf "$SRC/dep/eigen" "$SRC/dep/fmt"
cp -a dep/eigen "$SRC/dep/eigen" && cp -a dep/fmt "$SRC/dep/fmt" || exit 1
FILES="src/drivers/sqp_solver.cpp src/drivers/ipm_solver.cpp src/model/problem.cpp src/drivers/callback_rows.cpp src/drivers/solve_result.cpp include/hven/drivers/solve_result.h"
build() { SRC=$SRC bash "$D/legs/build.sh" Release mut hven_sqp_seam_tests hven_drivers_tests hven_interior_tests hven_sqp_tests > "$RAW/build-$1.log" 2>&1; }
runall() {
  local B=$WT/build-g5/mut/tests
  { timeout 900 "$B/hven_sqp_seam_tests" --gtest_brief=1 2>&1; echo "seam rc=$?";
    timeout 900 "$B/drivers/hven_drivers_tests" --gtest_brief=1 2>&1; echo "drivers rc=$?";
    timeout 900 "$B/interior/hven_interior_tests" --gtest_brief=1 2>&1; echo "interior rc=$?";
    timeout 900 "$B/sqp/hven_sqp_tests" --gtest_brief=1 --gtest_filter='F7ColdScaleSmoke.TheBestIterates*:ColumnScaling*' 2>&1; echo "sqp rc=$?"; } > "$RAW/run-$1.log"
  sed -nE 's/^\[  FAILED  \] ([A-Za-z0-9_]+\.[A-Za-z0-9_\/]+)( .*)?$/\1/p' "$RAW/run-$1.log" | sort -u
}
echo "id,mutation,build,killed_by" > "$OUT/mutation-table.csv"
declare -A DESC=(
 [a]="ipm event: kConstraints never delivered" [b]="ipm event: bound sides delivered on recovering rows"
 [c]="ipm declared_point: lower and upper sides swapped" [d]="ipm reached-step facts not reset at a phase's start"
 [e]="ipm reached step: alpha_primal not the stepped record's" [f]="ipm best: step facts not stored by the tracker"
 [g]="ipm best: row values/violation not composed (R5)" [h]="ipm reached step: line_search_trials not copied"
 [i]="sqp event: kConstraints never delivered" [j]="sqp event: z delivered without kVectors"
 [k]="sqp forwarder: depth-1 block not stamped depth 1" [l]="sqp best: engine block not completed at the push"
 [m]="sqp info: violation_l1 read from the feasibility column" [n]="sqp best: bound sides not split"
 [o]="split_bound_multipliers: sides swapped" [p]="row violation: positive part dropped"
 [q]="row violation: equality row without abs" [r]="row violation: free row NaN instead of 0"
 [s]="snapshot(): g not copied" [t]="snapshot(): ipm block not copied"
 [u]="default mask is kAll (Q9 reverted)" [v]="sqp best: inequality prices not sign-repaired before the row map (R5)")
build control; echo "control build rc=$?"
ctl=$(runall control | tr '\n' ' '); ctl="$ctl$(grep -E '^(seam|drivers|interior|sqp) rc=[1-9]' "$RAW/run-control.log" | tr '\n' ' ')"
echo "CONTROL failures: [${ctl}]"
echo "control,none,ok,${ctl:-NONE}" >> "$OUT/mutation-table.csv"
for id in a b c d e f g h i j k l m n o p q r s t u v; do
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
