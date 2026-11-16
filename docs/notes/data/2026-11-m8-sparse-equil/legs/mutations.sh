#!/usr/bin/env bash
# Mutation leg: control (no mutation) then M1..M6 (mutate.py), each rebuilt in build-se/rel and run on the pinning tests; the tree is
# restored after each and rebuilt at the end. A mutation is KILLED iff at least one of the pinning tests fails.
# usage: mutations.sh <out-dir>   Run under: flock /tmp/box-build.lock setsid -w bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/sparse-equil
OUT=${1:?out dir}
L=$WT/docs/notes/data/2026-11-m8-sparse-equil/legs
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-se/tmp GIT_CEILING_DIRECTORIES=$WT/build-se/tmp
cd "$WT" || exit 1
mkdir -p "$OUT"
B=$WT/build-se/rel
TESTS_SEAM='QpSparseEquil.*'
run_one() { # id
  local id=$1
  [ "$id" = control ] || python3 "$L/mutate.py" "$id" || return 9
  ninja -C "$B" -j16 hven_qp_walk_seam_tests > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate.py" restore; return 8; }
  "$B/tests/hven_qp_walk_seam_tests" --gtest_filter="$TESTS_SEAM" > "$OUT/$id.seam.log" 2>&1; local r2=$?; local r1=0
  grep -a "^\[  FAILED  \] [A-Za-z]" "$OUT/$id.seam.log" | sed 's/^.*\] //; s/ ([0-9]* ms)$//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate.py" restore
  echo "MUT $id sqp_rc=$r1 seam_rc=$r2 killers=$(wc -l < "$OUT/$id.killers.txt")"
  sed 's/^/   /' "$OUT/$id.killers.txt"
  if [ "$id" = control ]; then [ $r1 -eq 0 ] && [ $r2 -eq 0 ]; else [ $r1 -ne 0 ] || [ $r2 -ne 0 ]; fi
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in M1 M2 M3 M4 M5 M6 M7 M8; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
ninja -C "$B" -j16 hven_qp_walk_seam_tests > "$OUT/final.build.log" 2>&1
git -C "$WT" diff --quiet -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
