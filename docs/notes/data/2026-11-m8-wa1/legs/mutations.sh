#!/usr/bin/env bash
# WA-1 mutation leg (../DECLARATION-WA1.md): control, then M1..M5, M8 (must-kill) and M6, M7 (recorded) via mutate.py, each rebuilt in
# build-wa/rel (hven_qp_walk_seam_tests) and run on the pinning tests; the tree is restored after each and rebuilt at the end.
# A mutation is KILLED iff at least one pinning test fails (or the run times out).
# usage: mutations.sh <out-dir>   Run under: flock /tmp/box-build.lock setsid -w bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1
OUT=${1:?out dir}
L=$WT/docs/notes/data/2026-11-m8-wa1/legs
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-wa/tmp GIT_CEILING_DIRECTORIES=$WT/build-wa/tmp
cd "$WT" || exit 1
mkdir -p "$OUT"
B=$WT/build-wa/rel
TESTS='QpWa1AscentFootprint.*:QpW1AscentGuard.*:QpYrkRefusedExchange.*'
run_one() { # id
  local id=$1
  [ "$id" = control ] || python3 "$L/mutate.py" "$id" || return 9
  [ "$id" = control ] || git -C "$WT" diff -- src include > "$OUT/$id.diff"
  timeout 1500 ninja -C "$B" -j16 hven_qp_walk_seam_tests > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate.py" restore; return 8; }
  timeout 900 "$B/tests/hven_qp_walk_seam_tests" --gtest_filter="$TESTS" > "$OUT/$id.seam.log" 2>&1; local r=$?
  grep -a "^\[  FAILED  \] [A-Za-z]" "$OUT/$id.seam.log" | sed 's/^.*\] //; s/ (.*//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate.py" restore
  echo "MUT $id rc=$r killers=$(wc -l < "$OUT/$id.killers.txt")"
  sed 's/^/   /' "$OUT/$id.killers.txt"
  if [ "$id" = control ]; then [ $r -eq 0 ]; else [ $r -ne 0 ]; fi
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in M1 M2 M3 M4 M5 M8; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
for m in M6 M7; do run_one $m > "$OUT/$m.verdict.txt" 2>&1; echo "$m rc=$? (recorded); killers: $(wc -l < "$OUT/$m.killers.txt")"; done
timeout 1500 ninja -C "$B" -j16 hven_qp_walk_seam_tests > "$OUT/final.build.log" 2>&1
git -C "$WT" diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
