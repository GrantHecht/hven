#!/usr/bin/env bash
# sparse-equil v4 r2 (../../DECLARATION-SEQ4-r2.md): copied from ../../legs/ (or ../../runtime/), re-pointed to r2/ and build-k/bin-r2/; older provenance lines below are historical.
# Mutation leg (sparse-equil v4): control (no mutation) then Mt1-3, Me1-2, Mm1-4 (all must-kill; mutate.py), each rebuilt in build-k/rel and run on the pinning tests; the tree is
# restored after each and rebuilt at the end. A mutation is KILLED iff at least one of the pinning tests fails.
# usage: mutations.sh <out-dir>   Run under: flock /tmp/box-build.lock setsid -w bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/seq4
OUT=${1:?out dir}
L=$WT/docs/notes/data/2026-11-m8-sparse-equil-v4/r2/legs
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-k/tmp GIT_CEILING_DIRECTORIES=$WT/build-k/tmp
cd "$WT" || exit 1
mkdir -p "$OUT"
B=$WT/build-k/rel
TESTS_UNIT='MaxProductMatching.*:SymmetricMatchingScaling.*'
TESTS_SEAM='QpPlatformRetake.*:QpNc4ConvexCertificate.BritgasRestorationLpQp4:QpYrkRefusedExchange.*'
run_one() { # id
  local id=$1
  [ "$id" = control ] || python3 "$L/mutate.py" "$id" || return 9
  timeout 1500 ninja -C "$B" -j16 hven_tests hven_qp_walk_seam_tests > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate.py" restore; return 8; }
  timeout 900 "$B/tests/hven_tests" --gtest_filter="$TESTS_UNIT" > "$OUT/$id.unit.log" 2>&1; local r1=$?
  timeout 900 "$B/tests/hven_qp_walk_seam_tests" --gtest_filter="$TESTS_SEAM" > "$OUT/$id.seam.log" 2>&1; local r2=$?
  grep -a "^\[  FAILED  \] [A-Za-z]" "$OUT/$id.unit.log" "$OUT/$id.seam.log" | sed 's/^.*\] //' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate.py" restore
  echo "MUT $id unit_rc=$r1 seam_rc=$r2 killers=$(wc -l < "$OUT/$id.killers.txt")"
  sed 's/^/   /' "$OUT/$id.killers.txt"
  if [ "$id" = control ]; then [ $r1 -eq 0 ] && [ $r2 -eq 0 ]; else [ $r1 -ne 0 ] || [ $r2 -ne 0 ]; fi
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in Mt1 Mt2 Mt3 Me1 Me2 Mm1 Mm2 Mm3 Mm4 Mi1 Mr1 Mr2 Mb1 Md1 Md2; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
# r2 (declaration section 6): Me1 must also be killed by the integration fixture.
grep -qE '^QpPlatformRetake.LakesQp0EscalatesOnTheEngineRefusal( |$)' "$OUT/Me1.killers.txt" || { echo ME1_INTEGRATION_NOT_KILLED; verdict=ME1_INTEGRATION_NOT_KILLED; }
timeout 1500 ninja -C "$B" -j16 hven_tests hven_qp_walk_seam_tests > "$OUT/final.build.log" 2>&1
git -C "$WT" diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
