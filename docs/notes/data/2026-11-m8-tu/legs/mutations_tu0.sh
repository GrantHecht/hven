#!/usr/bin/env bash
# TU-0 mutation leg (../DECLARATION-TU-0.md section 5): a control, then every declared mutation (mutate_tu0.py ids), each rebuilt in the Release tree and run on the pinning tests;
# the tree is restored after each (git checkout of the mutated files) and rebuilt at the end. A mutation is KILLED iff at least one pinning test fails.
# usage: mutations_tu0.sh <out-dir>     Run as: flock /tmp/box-build.lock setsid -w bash mutations_tu0.sh ...   (env WT)
set -o pipefail
source "$(dirname "$0")/env.sh"
OUT=$(realpath -m "${1:?out dir}")
L=$E/legs
MUST=$(python3 "$L/mutate_tu0.py" ids)
box_check
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
git diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_DIRTY_BEFORE; exit 2; }
python3 "$L/mutate_tu0.py" check || exit 2
for m in $MUST; do python3 "$L/mutate_tu0.py" $m > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY $m"; python3 "$L/mutate_tu0.py" restore; exit 2; }; python3 "$L/mutate_tu0.py" restore > /dev/null; done
echo "PREFLIGHT every mutation applies and restores: [$MUST]"
mkdir -p "$OUT"
B=$WT/build
run_one() { # id
  local id=$1
  [ "$id" = control ] || python3 "$L/mutate_tu0.py" "$id" || return 9
  [ "$id" = control ] || git -C "$WT" diff -- src include > "$OUT/$id.diff"
  timeout 2400 ninja -C "$B" -j16 hven_sqp_tests hven_qp_tests hven_solver_tests > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate_tu0.py" restore; return 8; }
  timeout 900 "$B/tests/sqp/hven_sqp_tests" --gtest_filter='Tu0*:SqpSolverElasticSeed.*:SqpSolverRestorationSeed.*' > "$OUT/$id.sqp.log" 2>&1; local r1=$?
  timeout 900 "$B/tests/qp/hven_qp_tests" --gtest_filter='SsnEngineLocal.*' > "$OUT/$id.qp.log" 2>&1; local r2=$?
  timeout 900 "$B/tests/solver/hven_solver_tests" --gtest_filter='Options.*:OptionsDump.*' > "$OUT/$id.solver.log" 2>&1; local r3=$?
  cat "$OUT/$id.sqp.log" "$OUT/$id.qp.log" "$OUT/$id.solver.log" | grep -a "^\[  FAILED  \] [A-Za-z]" | sed 's/^.*\] //; s/ (.*//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate_tu0.py" restore > /dev/null
  echo "MUT $id sqp_rc=$r1 qp_rc=$r2 solver_rc=$r3 killers=$(wc -l < "$OUT/$id.killers.txt")"
  sed 's/^/   /' "$OUT/$id.killers.txt"
  if [ "$id" = control ]; then [ $r1 -eq 0 ] && [ $r2 -eq 0 ] && [ $r3 -eq 0 ]; else [ $r1 -ne 0 ] || [ $r2 -ne 0 ] || [ $r3 -ne 0 ]; fi
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in $MUST; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
timeout 2400 ninja -C "$B" -j16 > "$OUT/final.build.log" 2>&1
git -C "$WT" diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"; echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
