#!/usr/bin/env bash
# I19-2a mutation leg (../DECLARATION-I19-2a.md section 8): RF-1c's mutations_rf1c_r3.sh with the table mutate_i19.py and the
# I19-2a test filters. A control, then each mutation, rebuilt in the Release tree and run on the pinning tests; the tree is
# restored after each. KILLED iff a pinning test fails. Every id of the table is a must-kill unless the declaration marks it
# argued: ARGUED below (M18, M21; declaration section 8, ruling P2) is EXPECTED TO SURVIVE, and a kill of one is reported
# (KILLED_ARGUED), not a failure. The bar: every must-kill is killed by at least one test; the declared killer is reported
# informationally, never required. Paths are derived from this script's own location and the tree argument (no copied absolute paths).
# usage: mutations_i19.sh <out> <tree> <build dir> [ids...]   (no ids: every id of the table)
# Run under: ~/.local/bin/box-lane -- setsid -w bash mutations_i19.sh ...
set -o pipefail
OUT=${1:?out dir}; TREE=$(realpath "${2:?tree}"); B=$(realpath "${3:?build dir}"); shift 3
L=$(cd "$(dirname "$0")" && pwd)
M="python3 $L/mutate_i19.py"
export MUT_TREE=$TREE MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$TREE/.scratch/tmp GIT_CEILING_DIRECTORIES=$TREE/.scratch/tmp
mkdir -p "$TMPDIR"; OUT=$(realpath -m "$OUT"); mkdir -p "$OUT"
IDS="$*"; [ -n "$IDS" ] || IDS=$($M ids)
echo "FOREGROUND_START $(date -Is) tree=$TREE head=$(git -C "$TREE" rev-parse HEAD)"
cd "$TREE" || exit 1
git diff --quiet --ignore-submodules=all -- src include tests bench || { echo TREE_DIRTY_BEFORE; exit 2; }
$M check > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY"; $M restore; exit 2; }
echo "PREFLIGHT every mutation applies and restores: [$IDS]"
TARGETS="hven_sqp_tests hven_solver_tests hven_bench_tests"
SQP_FILTER='I19*:Tu0*:SqpSolverNonfiniteRollback.*:SqpSolverQpFailure.*:IterationTelemetry.*:CallerStepLimit*'
run_one() { local id=$1 r1 r2 r3
  [ "$id" = control ] || $M "$id" > /dev/null || return 9
  [ "$id" = control ] || git diff -- src include > "$OUT/$id.diff"
  timeout 3600 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/$id.build.log" 2>&1 || { echo "MUT $id BUILD_FAIL"; $M restore > /dev/null; return 8; }
  { timeout 1200 "$B/tests/sqp/hven_sqp_tests" --gtest_filter="$SQP_FILTER"; r1=$?
    timeout 600 "$B/tests/solver/hven_solver_tests" --gtest_filter='Options.*:OptionsDump*'; r2=$?
    timeout 600 "$B/tests/bench/hven_bench_tests" --gtest_filter='I19*'; r3=$?; } > "$OUT/$id.tests.log" 2>&1
  local r=$((r1 + r2 + r3))
  grep -a "^\[  FAILED  \] [A-Za-z]" "$OUT/$id.tests.log" | sed 's/^.*\] //; s/ (.*//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || $M restore > /dev/null
  echo "MUT $id rc=$r killers=$(wc -l < "$OUT/$id.killers.txt")"; sed 's/^/   /' "$OUT/$id.killers.txt"
  case "$id" in control) [ $r -eq 0 ] ;; *) [ $r -ne 0 ] ;; esac
}
ARGUED="M18 M21"
is_argued() { case " $ARGUED " in *" $1 "*) return 0 ;; esac; return 1; }
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in $IDS; do
  if run_one "$m"; then
    is_argued "$m" && echo "KILLED_ARGUED $m (an argued survivor was killed: reported, not a failure)"
  else
    rc=$?
    if [ $rc -ge 8 ]; then echo "MUT $m DID_NOT_RUN rc=$rc"; verdict="MUTATION_DID_NOT_RUN"
    elif is_argued "$m"; then echo "SURVIVED_AS_ARGUED $m"
    else echo "SURVIVED $m"; verdict="MUTATION_SURVIVED"; fi
  fi
done
timeout 3600 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/final.build.log" 2>&1
git diff --quiet --ignore-submodules=all -- src include tests bench || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"; echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
