#!/usr/bin/env bash
# M8 IPM no-numerics restoration routing (../DECLARATION.txt): the falsifier. Modelled on
# ../../2026-11-m8-ipm-ladder-status/legs/failproof.sh (re-pointed; the mutation table, the filter, the failing-name
# parser -- which now also reads parameterized names -- and the declared sets are this round's).
# Eight mutations of the code head's `git archive` export, each fail closed unless its site occurs exactly once
# (revert-all replaces the whole file):
#   revert-all          src/drivers/ipm_solver.cpp := the base's (b9626e7e5) -- the routing reverted, the tests kept;
#   no-route            a numeric refusal is not routed (kkt_no_numerics_numeric := false: every no-numerics exit is
#                       the hard-error termination) (F1);
#   no-barrier-tail     the governor's non-solving tail is not run on a factorization without numerics (F3);
#   no-acceptable-exit  the no-numerics abort never stops at an acceptable iterate (F7);
#   no-guard-site       the dense step guard's failed re-run is not read as "no numerics" (F2's site);
#   no-probe-guard      the governor runs on a factorization that left no numerics (the PROBE predictor's site);
#   diverging-override  the !GoodStep DIVERGING override applies to a factorization without numerics too;
#   hard-continues      a hard backend error is not terminal (F1's split).
# Each built Release (legs/build.sh's flags; hven_sqp_seam_tests only) at build-nonum/fp-<name>; the head's binary is
# build-nonum/rel's. Filter: the 14 tests below (FILTER). Output: the failing test names per arm, one per line, in
# <evidence-dir>/failing-<arm>.txt.
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a3d6ec088fa738122
L=$WT/docs/notes/data/2026-11-m8-ipm-nonum-restore/legs
TREES=${TREES:-$WT/build-nonum}
BASE_SHA=b9626e7e5
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
MUTS=${MUTS:-revert-all no-route no-barrier-tail no-acceptable-exit no-guard-site no-probe-guard diverging-override hard-continues}
FILTER='IpmDenseGuardSeam.*:IpmNoNumericsSeam.*:*/IpmNoNumericsRestorationSeam.*:BackendErrorSeam.*'
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1
run_filter() { # <build dir> <name>   (fails closed when the binary is absent)
  [ -x "$1/tests/hven_sqp_seam_tests" ] || { echo "BINARY_MISSING $1/tests/hven_sqp_seam_tests"; exit 8; }
  "$1/tests/hven_sqp_seam_tests" --gtest_filter="$FILTER" > "$EV/run-$2.txt" 2>&1; echo "$2 rc=$?"
  # A failure line carries its time: "[  FAILED  ] Suite.Name (N ms)" or, parameterized,
  # "[  FAILED  ] Prefix/Suite.Name/Param, where GetParam() = ... (N ms)".
  grep -E '^\[  FAILED  \] [A-Za-z0-9/]+\.[A-Za-z0-9/]+(,| ).*\([0-9]+ ms\)$' "$EV/run-$2.txt" | awk '{print $4}' \
    | sed 's/,$//' | sort -u > "$EV/failing-$2.txt"
  grep -hE '^\[  PASSED  \]|^\[==========\] [0-9]+ tests' "$EV/run-$2.txt"
  echo "$2 failing=$(wc -l < "$EV/failing-$2.txt"): $(tr '\n' ' ' < "$EV/failing-$2.txt")"
}
for M in $MUTS; do
  S=$TREES/fp-src-$M
  rm -rf "$S" "$TREES/fp-$M"
  mkdir -p "$S"
  git archive "$HEAD_SHA" | tar -x -C "$S" || { echo ARCHIVE_FAIL; exit 1; }
  rm -rf "$S/dep/eigen" "$S/dep/fmt"
  cp -a dep/eigen "$S/dep/eigen" && cp -a dep/fmt "$S/dep/fmt" || exit 1
  if [ "$M" = revert-all ]; then
    git show "$BASE_SHA:src/drivers/ipm_solver.cpp" > "$S/src/drivers/ipm_solver.cpp" || { echo MUTATION_FAIL; exit 1; }
    echo "MUTATED $M"
  else
    python3 - "$S/src/drivers/ipm_solver.cpp" "$M" <<'PY' || { echo MUTATION_FAIL; exit 1; }
import sys
p, m = sys.argv[1], sys.argv[2]
s = open(p).read()
site, new = {
    'no-route': ('            kkt_no_numerics && this->kkt_sol_.info() == Eigen::NumericalIssue;\n',
                 '            false;\n'),
    'no-barrier-tail': ('        } else if (kkt_no_numerics && (this->inequal_cons_ > 0 || this->bounds_)) [[unlikely]] {\n',
                        '        } else if (false) {\n'),
    'no-acceptable-exit': ('                    if (interior_point_iterate_acceptable(Citer, opts_))\n'
                           '                        exit_at_acceptable = true;\n',
                           '                    if (false)\n'
                           '                        exit_at_acceptable = true;\n'),
    'no-guard-site': ('                if (this->kkt_sol_.info() != Eigen::Success) [[unlikely]] {\n'
                      '                    kkt_no_numerics = true;\n',
                      '                if (false) {\n'
                      '                    kkt_no_numerics = true;\n'),
    'no-probe-guard': ('        if (!kkt_no_numerics && (this->inequal_cons_ > 0 || this->bounds_)) {\n',
                       '        if (this->inequal_cons_ > 0 || this->bounds_) {\n'),
    'diverging-override': ('        if (!GoodStep && !kkt_no_numerics)\n',
                           '        if (!GoodStep)\n'),
    'hard-continues': ('        if (kkt_no_numerics_hard || no_numerics_abort) [[unlikely]] {\n',
                       '        if (no_numerics_abort) [[unlikely]] {\n'),
}[m]
n = s.count(site)
if n != 1:
    sys.exit(f'{m}: site occurs {n} times, expected 1')
open(p, 'w').write(s.replace(site, new))
print(f'MUTATED {m}')
PY
  fi
  SRC=$S BUILD_ROOT=$TREES bash "$L/build.sh" Release "fp-$M" hven_sqp_seam_tests > "$EV/build-$M.txt" 2>&1
  rc=$?; echo "build $M rc=$rc"; [ $rc -ne 0 ] && { tail -20 "$EV/build-$M.txt"; exit 1; }
  run_filter "$TREES/fp-$M" "$M"
done
run_filter "$TREES/rel" head
# The pass search's route on this box (the test records it as a property): scale 1.76, pass 3, as before the change.
"$TREES/rel/tests/hven_sqp_seam_tests" --gtest_filter='IpmDenseGuardSeam.*' --gtest_output="xml:$EV/route-head.xml" > /dev/null 2>&1
grep -o 'name="route" value="[^"]*"' "$EV/route-head.xml"
# The criterion (../DECLARATION.txt): the head runs the 14 tests and fails none; each mutation fails EXACTLY its declared
# set (WANT, measured on a dev run before the declaration).
WANT_revert_all="BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/ProximalSwitch BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/ProximalSwitch IpmNoNumericsSeam.ALadderEndingWithoutNumericsAtAnAcceptableIterateStopsThere IpmNoNumericsSeam.ALadderEndingWithoutNumericsIsAStatusAtTheFirstIteration IpmNoNumericsSeam.ALadderEndingWithoutNumericsIsAStatusBeforeTheProbePredictor IpmNoNumericsSeam.TheAbortedIterationsRowCarriesItsBarrierObjective"
WANT_no_route="BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/ProximalSwitch BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/ProximalSwitch IpmDenseGuardSeam.AFailedGuardReRunEndsTheSolveWithoutItsStep IpmNoNumericsSeam.ALadderEndingWithoutNumericsAtAnAcceptableIterateStopsThere IpmNoNumericsSeam.ALadderEndingWithoutNumericsIsAStatusAtTheFirstIteration IpmNoNumericsSeam.ALadderEndingWithoutNumericsIsAStatusBeforeTheProbePredictor"
WANT_no_barrier_tail="IpmNoNumericsSeam.TheAbortedIterationsRowCarriesItsBarrierObjective"
WANT_no_acceptable_exit="IpmNoNumericsSeam.ALadderEndingWithoutNumericsAtAnAcceptableIterateStopsThere"
WANT_no_guard_site="IpmDenseGuardSeam.AFailedGuardReRunEndsTheSolveWithoutItsStep"
WANT_no_probe_guard="IpmNoNumericsSeam.ALadderEndingWithoutNumericsIsAStatusBeforeTheProbePredictor IpmNoNumericsSeam.TheAbortedIterationsRowCarriesItsBarrierObjective"
WANT_diverging_override="BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/ProximalSwitch BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/ProximalSwitch IpmNoNumericsSeam.ALadderEndingWithoutNumericsAtAnAcceptableIterateStopsThere"
WANT_hard_continues="IpmNoNumericsSeam.AHardBackendErrorEndsTheSolveWithoutRestoration"
verdict=FAILPROOF_AS_DECLARED
heads=$(grep -hE '^\[  PASSED  \]' "$EV/run-head.txt" | awk '{print $4}')
[ -s "$EV/failing-head.txt" ] && verdict=FAILPROOF_HEAD_FAILS
[ "$heads" = 14 ] || verdict="FAILPROOF_HEAD_COUNT($heads)"
grep -q 'name="route" value="scale 1.76 pass 3"' "$EV/route-head.xml" || verdict=FAILPROOF_ROUTE_MOVED
for M in $MUTS; do
  v=WANT_${M//-/_}
  got=$(tr '\n' ' ' < "$EV/failing-$M.txt" | sed 's/ $//')
  if [ "$got" != "${!v}" ]; then verdict="FAILPROOF_MISMATCH($M)"; echo "MISMATCH $M got=[$got] want=[${!v}]"; fi
done
echo "$verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = FAILPROOF_AS_DECLARED ]
