#!/usr/bin/env bash
# I17 r2 mutation leg (../DECLARATION-I17-r2.md section 5): mutations.sh with mutate_i17_r2.py and the rider ids; a control, then every declared mutation, each rebuilt in the Release tree and run on the pinning tests;
# the tree is restored after each (git checkout of the mutated files) and rebuilt at the end. A mutation is KILLED iff at least one pinning test fails.
# usage: mutations_r2.sh <out-dir>     Run as: flock /tmp/box-build.lock setsid -w bash mutations.sh ...   (env WT)
set -o pipefail
source "$(dirname "$0")/env.sh"
OUT=$(realpath -m "${1:?out dir}")
L=$E/legs
MUST="L1 L2 L3 L4 L5 L6 L7 S1 S2 S3 S4 C1 C2 D1 D2 D3 D4 I1a I1b I2 I4a I4b I5a I5b I5c I6"
box_check
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
git diff --quiet --ignore-submodules=all -- src include || { echo TREE_DIRTY_BEFORE; exit 2; }
python3 "$L/mutate_i17_r2.py" check || exit 2
for m in $MUST; do python3 "$L/mutate_i17_r2.py" $m > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY $m"; python3 "$L/mutate_i17_r2.py" restore; exit 2; }; python3 "$L/mutate_i17_r2.py" restore > /dev/null; done
echo "PREFLIGHT every mutation applies and restores: [$MUST]"
mkdir -p "$OUT"
B=$WT/build
SQP='SqpSolverGlobalization.*:SqpSolverCertifiedFallback.*:SqpSolverW2Fixtures.*:CallerStepLimit.*:CallerStepLimitSolve.*:ColumnScalingFactorMax.*:QpDump.*:ColumnScalingDriver.*:ScaledConvergenceGuard.*'
run_one() { # id
  local id=$1
  [ "$id" = control ] || python3 "$L/mutate_i17_r2.py" "$id" || return 9
  [ "$id" = control ] || git -C "$WT" diff -- src include > "$OUT/$id.diff"
  timeout 1500 ninja -C "$B" -j16 hven_sqp_tests hven_trace_tests > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate_i17_r2.py" restore; return 8; }
  timeout 900 "$B/tests/sqp/hven_sqp_tests" --gtest_filter="$SQP" > "$OUT/$id.sqp.log" 2>&1; local r1=$?
  timeout 900 "$B/tests/trace/hven_trace_tests" > "$OUT/$id.trace.log" 2>&1; local r2=$?
  cat "$OUT/$id.sqp.log" "$OUT/$id.trace.log" | grep -a "^\[  FAILED  \] [A-Za-z]" | sed 's/^.*\] //; s/ (.*//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate_i17_r2.py" restore > /dev/null
  echo "MUT $id sqp_rc=$r1 trace_rc=$r2 killers=$(wc -l < "$OUT/$id.killers.txt")"
  sed 's/^/   /' "$OUT/$id.killers.txt"
  if [ "$id" = control ]; then [ $r1 -eq 0 ] && [ $r2 -eq 0 ]; else [ $r1 -ne 0 ] || [ $r2 -ne 0 ]; fi
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in $MUST; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
timeout 1500 ninja -C "$B" -j16 > "$OUT/final.build.log" 2>&1
git -C "$WT" diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"; echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
