#!/usr/bin/env bash
# Mutation leg: control (no mutation) then Ma, Mb, Mc, Md, Me1 (must-kill) and Me2 (must-terminate) (mutate.py), each rebuilt in build-k/rel and run on the pinning tests; the tree is
# restored after each and rebuilt at the end. A mutation is KILLED iff at least one of the pinning tests fails.
# usage: mutations.sh <out-dir>   Run under: flock /tmp/box-build.lock setsid -w bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/kopt-1x-r2
OUT=${1:?out dir}
L=$WT/docs/notes/data/2026-11-m8-kopt-1x-r2/legs
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-k/tmp GIT_CEILING_DIRECTORIES=$WT/build-k/tmp
cd "$WT" || exit 1
mkdir -p "$OUT"
B=$WT/build-k/rel
TESTS_SQP='QpRowContract.*:SqpDriverCertifiedFallback.T7TheVerdictRefineCostAtScale*:HsSweepCorpus.Corpus'
TESTS_SEAM='QpRowContractPolish.*:QpCertificateSign.AnOptimalExitPastOneFloorUnitIsCountedAGrayExit:QpRdvRankRepair.Yorknet*'
run_one() { # id
  local id=$1
  [ "$id" = control ] || python3 "$L/mutate.py" "$id" || return 9
  timeout 1500 ninja -C "$B" -j16 hven_sqp_tests hven_qp_walk_seam_tests > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate.py" restore; return 8; }
  timeout 900 "$B/tests/sqp/hven_sqp_tests" --gtest_filter="$TESTS_SQP" > "$OUT/$id.sqp.log" 2>&1; local r1=$?
  timeout 900 "$B/tests/hven_qp_walk_seam_tests" --gtest_filter="$TESTS_SEAM" > "$OUT/$id.seam.log" 2>&1; local r2=$?
  grep -a "^\[  FAILED  \] [A-Za-z]" "$OUT/$id.sqp.log" "$OUT/$id.seam.log" | sed 's/^.*\] //' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate.py" restore
  echo "MUT $id sqp_rc=$r1 seam_rc=$r2 killers=$(wc -l < "$OUT/$id.killers.txt")"
  sed 's/^/   /' "$OUT/$id.killers.txt"
  if [ "$id" = control ]; then [ $r1 -eq 0 ] && [ $r2 -eq 0 ]; else [ $r1 -ne 0 ] || [ $r2 -ne 0 ]; fi
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in Ma Mb Mc Mg; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
# Me1 (bound removed), Me2 (bound doubled), Me3 (bound and progress rule removed) are must-TERMINATE checks, not must-kill: each
# run is under `timeout`, and its outcome (killers, exit codes) is recorded.
for m in Me1 Me2 Me3; do run_one $m > "$OUT/$m.verdict.txt" 2>&1; echo "$m rc=$? (terminated); killers: $(wc -l < "$OUT/$m.killers.txt")"; done
timeout 1500 ninja -C "$B" -j16 hven_sqp_tests hven_qp_walk_seam_tests > "$OUT/final.build.log" 2>&1
git -C "$WT" diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
