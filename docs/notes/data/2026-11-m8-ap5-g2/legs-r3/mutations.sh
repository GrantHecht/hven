#!/usr/bin/env bash
# M8 AP-5 G2 r3 (../DECLARATION-r3.md): the mutation table. A scratch source copy of HEAD (build-g2/mut-src, git-ignored) is
# built Release (build-g2/mut) for hven_drivers_tests and hven_interior_tests; each mutation is applied to the COPY (the worktree is
# never touched), both test binaries are run in full, the failing test names are the kill list, and the copy's file is restored from HEAD.
# usage: mutations.sh <code-head sha> <out-dir>     Run as: flock /tmp/box-build.lock bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a41fdd4ad89efc301
D=$WT/docs/notes/data/2026-11-m8-ap5-g2
HEAD_SHA=${1:?code head sha}
OUT=${2:?out dir}
RAW=$WT/.scratch/mutation-r3
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-g2/tmp
cd "$WT" || exit 1
mkdir -p "$OUT" "$RAW"
SRC=$WT/build-g2/mut-src
if [ ! -d "$SRC" ]; then
  mkdir -p "$SRC"
  git archive "$HEAD_SHA" | tar -x -C "$SRC" || exit 1
  rm -rf "$SRC/dep/eigen" "$SRC/dep/fmt"
  cp -a dep/eigen "$SRC/dep/eigen" && cp -a dep/fmt "$SRC/dep/fmt" || exit 1
fi
build() { SRC=$SRC bash "$D/legs/build.sh" Release mut hven_drivers_tests hven_interior_tests > "$RAW/build-$1.log" 2>&1; }
runall() {
  { timeout 900 "$WT/build-g2/mut/tests/drivers/hven_drivers_tests" --gtest_brief=1 2>&1; echo "drivers rc=$?"; timeout 900 "$WT/build-g2/mut/tests/interior/hven_interior_tests" --gtest_brief=1 2>&1; echo "interior rc=$?"; } > "$RAW/run-$1.log"
  sed -nE 's/^\[  FAILED  \] ([A-Za-z0-9_]+\.[A-Za-z0-9_\/]+)( .*)?$/\1/p' "$RAW/run-$1.log" | sort -u
}
echo "id,mutation,build,killed_by" > "$OUT/mutation-table.csv"
declare -A DESC=(
 [a]="ipm skip stash restore" [b]="ipm drop Newton-form undo" [c]="ipm drop stash-point un-contamination"
 [d]="sqp drop never-resume gate" [e]="sqp drop return_entry_point" [f]="sqp do not forward the token"
 [g]="sqp drop G1 SOC undo" [h]="sqp drop token_interrupt label" [i]="sqp drop G1 failure-step undo"
 [j]="ipm recovering flag always false" [k]="ipm stash keeps no mu" [l]="ipm stash keeps no iteration row"
 [m]="sqp keep latch on converged-phase terminal stop (R1)" [n]="sqp stopped_in_recovery keyed on parent latch (old key)"
 [o]="sqp never set returned_pre_restoration_point" [p]="ipm never set returned_pre_restoration_point")
build control; echo "control build rc=$?"
ctl=$(runall control); echo "CONTROL failures: [${ctl}]"
echo "control,none,ok,${ctl:-NONE}" >> "$OUT/mutation-table.csv"
for id in a b c d e f g h i j k l m n o p; do
  python3 "$D/legs-r3/mutate.py" "$SRC" "$id" || { echo "MUTATE_FAIL $id"; echo "$id,${DESC[$id]},APPLY_FAIL," >> "$OUT/mutation-table.csv"; continue; }
  if build "$id"; then
    killed=$(runall "$id" | tr '\n' ' ')
    killed="$killed$(grep -E '^(drivers|interior) rc=[1-9]' "$RAW/run-$id.log" | tr '\n' ' ')"
    echo "$id,${DESC[$id]},ok,${killed:-SURVIVES}" >> "$OUT/mutation-table.csv"
    echo "MUT $id: ${killed:-SURVIVES}"
  else
    echo "$id,${DESC[$id]},BUILD_FAIL," >> "$OUT/mutation-table.csv"; echo "MUT $id BUILD_FAIL"
  fi
  for f in src/drivers/sqp_solver.cpp src/drivers/ipm_solver.cpp; do git show "$HEAD_SHA:$f" > "$SRC/$f"; done
done
for f in src/drivers/sqp_solver.cpp src/drivers/ipm_solver.cpp; do git show "$HEAD_SHA:$f" | diff -q - "$SRC/$f" > /dev/null || echo "RESTORE_MISMATCH $f"; done
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench || echo "WORKTREE_MOVED"
echo "FOREGROUND_END $(date -Is)"
