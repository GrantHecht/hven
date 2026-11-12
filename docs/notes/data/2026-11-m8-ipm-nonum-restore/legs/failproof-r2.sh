#!/usr/bin/env bash
# M8 IPM no-numerics restoration routing r2 (../DECLARATION-r2.txt): failproof.sh (r1, kept) copied and re-pointed for the
# review round's code head: the mutation table (the r1 eight re-checked against the new head -- no-acceptable-exit's site
# moved with the acceptable exit -- plus six for R1, R2, R3 and R4), multi-site mutations (every site exactly once, fail
# closed), the declared sets and the head count (19). Older provenance below is r1's and historical.
# Mutations of the code head's `git archive` export:
#   revert-all                     src/drivers/ipm_solver.cpp := the base's (b9626e7e5);
#   no-route                       kkt_no_numerics_numeric := false (F1);
#   no-barrier-tail                the governor's non-solving tail is not run (F3);
#   no-acceptable-exit             the no-numerics acceptable exit never fires (F7, its new site);
#   no-guard-site                  the dense step guard's failed re-run is not read as "no numerics" (F2);
#   no-probe-guard                 the governor runs on a factorization without numerics;
#   diverging-override             the !GoodStep DIVERGING override applies to no numerics too;
#   hard-continues                 a hard backend error is not terminal;
#   restoration-acceptable         R1: the no-numerics acceptable exit fires inside restoration too;
#   uneval-restoration-acceptable  R1: the un-evaluable dispatch's acceptable exit fires inside restoration too;
#   acceptable-last                R2: the acceptable exit tested at the abort instead of first (632ba77cc's order);
#   no-recentre                    R3(c): a no-numerics iteration skips the nested elastic re-centre;
#   keep-dxsl                      R3(a): the no-numerics DXSL.setZero() deleted (alpha = 0 kept);
#   no-acceptable-detail           R4: the acceptable stop's stop_detail_ is not written.
# Each built Release (legs/build.sh's flags; hven_sqp_seam_tests only) at <TREES>/fp-r2-<name>; the head's binary is
# build-nonum/rel's. Filter: FILTER (19 tests at the head). Output: the failing names per arm in <evidence-dir>/failing-<arm>.txt.
# usage: failproof-r2.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof-r2.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a3d6ec088fa738122
L=$WT/docs/notes/data/2026-11-m8-ipm-nonum-restore/legs
TREES=${TREES:-$WT/build-nonum}
BASE_SHA=b9626e7e5
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
MUTS=${MUTS:-revert-all no-route no-barrier-tail no-acceptable-exit no-guard-site no-probe-guard diverging-override hard-continues restoration-acceptable uneval-restoration-acceptable acceptable-last no-recentre keep-dxsl no-acceptable-detail}
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
  S=$TREES/fp-r2-src-$M
  rm -rf "$S" "$TREES/fp-r2-$M"
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
A = '                interior_point_iterate_acceptable(Citer, opts_)) {\n' \
    '                // Stop at the acceptable iterate: no step, nothing resolved.\n'
table = {
    'no-route': [('            kkt_no_numerics && this->kkt_sol_.info() == Eigen::NumericalIssue;\n',
                  '            false;\n')],
    'no-barrier-tail': [('        } else if (kkt_no_numerics && (this->inequal_cons_ > 0 || this->bounds_)) [[unlikely]] {\n',
                         '        } else if (false) {\n')],
    'no-acceptable-exit': [(A, A.replace('interior_point_iterate_acceptable(Citer, opts_))', 'false)'))],
    'no-guard-site': [('                if (this->kkt_sol_.info() != Eigen::Success) [[unlikely]] {\n'
                       '                    kkt_no_numerics = true;\n',
                       '                if (false) {\n'
                       '                    kkt_no_numerics = true;\n')],
    'no-probe-guard': [('        if (!kkt_no_numerics && (this->inequal_cons_ > 0 || this->bounds_)) {\n',
                        '        if (this->inequal_cons_ > 0 || this->bounds_) {\n')],
    'diverging-override': [('        if (!GoodStep && !kkt_no_numerics)\n',
                            '        if (!GoodStep)\n')],
    'hard-continues': [('        if (kkt_no_numerics_hard || no_numerics_abort) [[unlikely]] {\n',
                        '        if (no_numerics_abort) [[unlikely]] {\n')],
    'restoration-acceptable': [('            if (kkt_no_numerics_numeric &&\n'
                                '                !(this->restoration_ && this->restoration_->is_active()) &&\n',
                                '            if (kkt_no_numerics_numeric &&\n')],
    'uneval-restoration-acceptable': [('                    if (!(this->restoration_ && this->restoration_->is_active()) &&\n'
                                       '                        interior_point_iterate_acceptable(Citer, opts_)) {\n',
                                       '                    if (interior_point_iterate_acceptable(Citer, opts_)) {\n')],
    'acceptable-last': [(A, A.replace('interior_point_iterate_acceptable(Citer, opts_))', 'false)')),
                        ('                    alpha = 0.0;\n'
                         '                    no_numerics_abort = true;\n',
                         '                    alpha = 0.0;\n'
                         '                    if (interior_point_iterate_acceptable(Citer, opts_)) {\n'
                         '                        exit_at_acceptable = true;\n'
                         '                        no_numerics_acceptable = true;\n'
                         '                    } else {\n'
                         '                        no_numerics_abort = true;\n'
                         '                    }\n')],
    'no-recentre': [('            } else if (nested_active && this->try_recenter_elastics(step_mu)) {\n',
                     '            } else if (nested_active && !kkt_no_numerics_numeric &&\n'
                     '                       this->try_recenter_elastics(step_mu)) {\n')],
    'keep-dxsl': [('        if (kkt_no_numerics) [[unlikely]] {\n'
                   '            DXSL.setZero();\n'
                   '            alpha = 0.0;\n',
                   '        if (kkt_no_numerics) [[unlikely]] {\n'
                   '            alpha = 0.0;\n')],
    'no-acceptable-detail': [('        if (no_numerics_acceptable && ExitCode == SolveStatus::kAcceptable) [[unlikely]]\n',
                              '        if (false)\n')],
}
for site, new in table[m]:
    n = s.count(site)
    if n != 1:
        sys.exit(f'{m}: site occurs {n} times, expected 1')
    s = s.replace(site, new)
open(p, 'w').write(s)
print(f'MUTATED {m}')
PY
  fi
  SRC=$S BUILD_ROOT=$TREES bash "$L/build.sh" Release "fp-r2-$M" hven_sqp_seam_tests > "$EV/build-$M.txt" 2>&1
  rc=$?; echo "build $M rc=$rc"; [ $rc -ne 0 ] && { tail -20 "$EV/build-$M.txt"; exit 1; }
  run_filter "$TREES/fp-r2-$M" "$M"
done
run_filter "$TREES/rel" head
# The pass search's route on this box (the test records it as a property): scale 1.76, pass 3, as before the change.
"$TREES/rel/tests/hven_sqp_seam_tests" --gtest_filter='IpmDenseGuardSeam.*' --gtest_output="xml:$EV/route-head.xml" > /dev/null 2>&1
grep -o 'name="route" value="[^"]*"' "$EV/route-head.xml"
# The criterion (../DECLARATION-r2.txt): the head runs the 19 tests and fails none; each mutation fails EXACTLY its declared
# set (WANT, measured on a dev run before the declaration). keep-dxsl is declared to fail NOTHING (an equivalent
# mutant on every faithful route: see ../DECLARATION-r2.txt P4); it stays in the table so a future reader of DXSL shows up.
WANT_revert_all="BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/ProximalSwitch BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/ProximalSwitch BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/ProximalSwitch IpmDenseGuardSeam.AFailedGuardReRunUnderRestorationDiscardsItsStep IpmNoNumericsSeam.ALadderEndingWithoutNumericsAtAnAcceptableIterateStopsThere IpmNoNumericsSeam.ALadderEndingWithoutNumericsIsAStatusAtTheFirstIteration IpmNoNumericsSeam.ALadderEndingWithoutNumericsIsAStatusBeforeTheProbePredictor IpmNoNumericsSeam.AnAcceptableRestorationRowIsNeverAnAcceptableStop IpmNoNumericsSeam.AnUnevaluableStepInsideRestorationIsNeverAnAcceptableStop IpmNoNumericsSeam.TheAbortedIterationsRowCarriesItsBarrierObjective"
WANT_no_route="BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/ProximalSwitch BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/ProximalSwitch BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/ProximalSwitch IpmDenseGuardSeam.AFailedGuardReRunEndsTheSolveWithoutItsStep IpmDenseGuardSeam.AFailedGuardReRunUnderRestorationDiscardsItsStep IpmNoNumericsSeam.ALadderEndingWithoutNumericsAtAnAcceptableIterateStopsThere IpmNoNumericsSeam.ALadderEndingWithoutNumericsIsAStatusAtTheFirstIteration IpmNoNumericsSeam.ALadderEndingWithoutNumericsIsAStatusBeforeTheProbePredictor IpmNoNumericsSeam.AnAcceptableRestorationRowIsNeverAnAcceptableStop IpmNoNumericsSeam.AnUnevaluableStepInsideRestorationIsNeverAnAcceptableStop"
WANT_no_barrier_tail="IpmNoNumericsSeam.TheAbortedIterationsRowCarriesItsBarrierObjective"
WANT_no_acceptable_exit="BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/ProximalSwitch IpmNoNumericsSeam.ALadderEndingWithoutNumericsAtAnAcceptableIterateStopsThere"
WANT_no_guard_site="IpmDenseGuardSeam.AFailedGuardReRunEndsTheSolveWithoutItsStep IpmDenseGuardSeam.AFailedGuardReRunUnderRestorationDiscardsItsStep"
WANT_no_probe_guard="IpmNoNumericsSeam.ALadderEndingWithoutNumericsIsAStatusBeforeTheProbePredictor IpmNoNumericsSeam.AnAcceptableRestorationRowIsNeverAnAcceptableStop IpmNoNumericsSeam.TheAbortedIterationsRowCarriesItsBarrierObjective"
WANT_diverging_override="BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/ProximalSwitch BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/ProximalSwitch BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalIsRecoveredThroughRestoration/ProximalSwitch IpmDenseGuardSeam.AFailedGuardReRunUnderRestorationDiscardsItsStep IpmNoNumericsSeam.ALadderEndingWithoutNumericsAtAnAcceptableIterateStopsThere IpmNoNumericsSeam.AnAcceptableRestorationRowIsNeverAnAcceptableStop IpmNoNumericsSeam.AnUnevaluableStepInsideRestorationIsNeverAnAcceptableStop"
WANT_hard_continues="IpmNoNumericsSeam.AHardBackendErrorEndsTheSolveWithoutRestoration"
WANT_restoration_acceptable="IpmNoNumericsSeam.AnAcceptableRestorationRowIsNeverAnAcceptableStop"
WANT_uneval_restoration_acceptable="IpmNoNumericsSeam.AnUnevaluableStepInsideRestorationIsNeverAnAcceptableStop"
WANT_acceptable_last="BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/ProximalSwitch IpmNoNumericsSeam.AnAcceptableRestorationRowIsNeverAnAcceptableStop"
WANT_no_recentre="BothStrategies/IpmNoNumericsRestorationSeam.ANumericRefusalEntersRestoration/L1Nested"
WANT_keep_dxsl=""
WANT_no_acceptable_detail="BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/L1Nested BothStrategies/IpmNoNumericsRestorationSeam.AnAcceptableIterateStopsBeforeRestorationIsEntered/ProximalSwitch IpmNoNumericsSeam.ALadderEndingWithoutNumericsAtAnAcceptableIterateStopsThere"
verdict=FAILPROOF_AS_DECLARED
heads=$(grep -hE '^\[  PASSED  \]' "$EV/run-head.txt" | awk '{print $4}')
[ -s "$EV/failing-head.txt" ] && verdict=FAILPROOF_HEAD_FAILS
[ "$heads" = 19 ] || verdict="FAILPROOF_HEAD_COUNT($heads)"
grep -q 'name="route" value="scale 1.76 pass 3"' "$EV/route-head.xml" || verdict=FAILPROOF_ROUTE_MOVED
for M in $MUTS; do
  v=WANT_${M//-/_}
  got=$(tr '\n' ' ' < "$EV/failing-$M.txt" | sed 's/ $//')
  if [ "$got" != "${!v}" ]; then verdict="FAILPROOF_MISMATCH($M)"; echo "MISMATCH $M got=[$got] want=[${!v}]"; fi
done
echo "$verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = FAILPROOF_AS_DECLARED ]
