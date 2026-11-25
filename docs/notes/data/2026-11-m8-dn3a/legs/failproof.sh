#!/usr/bin/env bash
# M8 DN-3a (../DECLARATION.txt P4): structure from ../../2026-11-m8-ipm-nonum-restore/legs/failproof-r2.sh (multi-site
# mutations of the code head's `git archive` export, every site its declared count or fail closed), re-pointed (this
# worktree, build-dn3a/, this directory) and rewritten for DN-3a's mutation table and three test binaries.
# Mutations:
#   thread-pin-off        dense_session.cpp: the four MklThreadScope sites take opts_.threads again (Q4 reverted);
#   check-never-refuses   kkt_calls.h: dense_solve_check_refused's final verdict is "pass";
#   refine-off            kkt_calls.h: the check's refinement takes no step;
#   refine-always         kkt_calls.h: the check refines a solve that already passes (no early return);
#   dense-only-off        kkt_calls.h: unshifted_dense_solve_refused checks platform-session answers too;
#   ssn-unwired           ssn_engine.cpp: the SSN step is not checked;
#   ipqp-unwired          ipqp_engine.cpp: the IPQP solves are not checked;
#   ipqp-response-off     ipqp_engine.cpp: a refused IPQP solve is used (all three sites);
#   gram-dense-clause-off ws_independence.h: the Gram refusal ignores the dense arm's inertia;
#   gram-unwired          ws_independence.cpp: the rank repair keys on the old perturbed-pivot test (declared EQUIVALENT
#                         on every faithful route: no real input reaches a dense Gram whose inertia is not (mr, 0)).
# Each built Release (legs/build.sh's flags; the three targets below) at <TREES>/fp-<name>; the head's binaries are
# build-dn3a/rel's. Output: the failing names per arm in <evidence-dir>/failing-<arm>.txt.
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-acfc8f7d568381148
L=$WT/docs/notes/data/2026-11-m8-dn3a/legs
TREES=${TREES:-$WT/build-dn3a}
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
MUTS=${MUTS:-thread-pin-off check-never-refuses refine-off refine-always dense-only-off ssn-unwired ipqp-unwired ipqp-response-off gram-dense-clause-off gram-unwired}
F_LIN='DenseArmThreads.*:DenseSolveCheck.*:DenseArm.*:DenseArmReport.*'
F_DRV='DenseArmEngines.*'
F_SQP='IpqpDenseSolveCheck.*'
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1
run_filter() { # <build dir> <name>   (fails closed when a binary is absent)
  : > "$EV/run-$2.txt"
  for pair in "tests/hven_tests|$F_LIN" "tests/drivers/hven_drivers_tests|$F_DRV" "tests/sqp/hven_sqp_tests|$F_SQP"; do
    b=${pair%%|*}; f=${pair#*|}
    [ -x "$1/$b" ] || { echo "BINARY_MISSING $1/$b"; exit 8; }
    "$1/$b" --gtest_filter="$f" >> "$EV/run-$2.txt" 2>&1; echo "$2 $b rc=$?"
  done
  grep -E '^\[  FAILED  \] [A-Za-z0-9/]+\.[A-Za-z0-9/]+(,| ).*\([0-9]+ ms\)$' "$EV/run-$2.txt" | awk '{print $4}' \
    | sed 's/,$//' | sort -u > "$EV/failing-$2.txt"
  echo "$2 passed=$(grep -hE '^\[  PASSED  \]' "$EV/run-$2.txt" | awk '{s+=$4} END{print s+0}') failing=$(wc -l < "$EV/failing-$2.txt"): $(tr '\n' ' ' < "$EV/failing-$2.txt")"
}
for M in $MUTS; do
  S=$TREES/fp-src-$M
  rm -rf "$S" "$TREES/fp-$M"
  mkdir -p "$S"
  git archive "$HEAD_SHA" | tar -x -C "$S" || { echo ARCHIVE_FAIL; exit 1; }
  rm -rf "$S/dep/eigen" "$S/dep/fmt"
  cp -a dep/eigen "$S/dep/eigen" && cp -a dep/fmt "$S/dep/fmt" || exit 1
  python3 - "$S" "$M" <<'PY' || { echo MUTATION_FAIL; exit 1; }
import sys
root, m = sys.argv[1], sys.argv[2]
KC = 'include/hven/detail/kkt/kkt_calls.h'
table = {
    'thread-pin-off': [('src/linear/dense_session.cpp', 'MklThreadScope threads(kDenseArmThreads);',
                        'MklThreadScope threads(opts_.threads);', 4)],
    'check-never-refuses': [(KC, '    return !(rho <= kDenseSolveResidualTol);\n}', '    return false;\n}', 1)],
    'refine-off': [(KC, 'for (Index step = 0; step < kEqpShiftRefineSteps; ++step) {',
                    'for (Index step = 0; step < 0; ++step) {', 1)],
    'refine-always': [(KC, '    double rho = dense_residual_ratio(K, rhs, y, d);\n'
                           '    if (rho <= kDenseSolveResidualTol) {\n'
                           '        return false;\n'
                           '    }\n',
                       '    double rho = dense_residual_ratio(K, rhs, y, d);\n', 1)],
    'dense-only-off': [(KC, '    if (arm != hven::linear::FactorArm::kDense || k.dense_shifted) {',
                        '    if (k.dense_shifted) {', 1)],
    'ssn-unwired': [('src/qp/ssn_engine.cpp',
                     'dw_dense_refused = detail::unshifted_dense_solve_refused(k_, rhs, kkt_, fo.arm, dw);',
                     '(void)fo;', 1)],
    'ipqp-unwired': [('src/qp/ipqp_engine.cpp',
                      '        if (kkt_.arm() == hven::linear::FactorArm::kDense) {\n            refused =',
                      '        if (false) {\n            refused =', 1)],
    'ipqp-response-off': [('src/qp/ipqp_engine.cpp', 'ok = !refused && w.sol.allFinite();', 'ok = w.sol.allFinite();', 1),
                          ('src/qp/ipqp_engine.cpp', 'if (affine_refused || !w.sol.allFinite()) {',
                           'if (!w.sol.allFinite()) {', 1),
                          ('src/qp/ipqp_engine.cpp', 'if (corrector_refused || !w.sol.allFinite()) {',
                           'if (!w.sol.allFinite()) {', 1)],
    'gram-dense-clause-off': [('include/hven/detail/qp/ws_independence.h',
                               '    if (fo.arm == hven::linear::FactorArm::kDense) {\n        return !(',
                               '    if (false) {\n        return !(', 1)],
    'gram-unwired': [('src/qp/ws_independence.cpp', '                    if (rank_gram_factor_refused(fo, mr)) {',
                      '                    if (fo.status != hven::linear::FactorizeOutcome::Status::kOk ||\n'
                      '                        (fo.inertia.perturbed_pivots.has_value() &&\n'
                      '                         *fo.inertia.perturbed_pivots != 0)) {', 1)],
}
for path, site, new, want in table[m]:
    p = f'{root}/{path}'
    s = open(p).read()
    n = s.count(site)
    if n != want:
        sys.exit(f'{m}: site in {path} occurs {n} times, expected {want}')
    s = s.replace(site, new)
    open(p, 'w').write(s)
print(f'MUTATED {m}')
PY
  SRC=$S BUILD_ROOT=$TREES bash "$L/build.sh" Release "fp-$M" hven_tests hven_drivers_tests hven_sqp_tests > "$EV/build-$M.txt" 2>&1
  rc=$?; echo "build $M rc=$rc"; [ $rc -ne 0 ] && { tail -20 "$EV/build-$M.txt"; exit 1; }
  run_filter "$TREES/fp-$M" "$M"
done
run_filter "$TREES/rel" head
# The criterion (../DECLARATION.txt P4): the head runs HEAD_COUNT tests and fails none; each mutation fails EXACTLY its
# declared set (WANT, measured on a dev run before the declaration).
HEAD_COUNT=48
WANT_thread_pin_off="DenseArmThreads.EveryDenseLapackCallRunsAtOneThread"
WANT_check_never_refuses="DenseArmEngines.TheSsnRefusesAnUnshiftedDenseNewtonStepThatMissesK DenseSolveCheck.ANonFiniteSolveIsRefusedAndRhoReadsTheMetric DenseSolveCheck.AnUnflaggedIllConditionedDenseSolveIsRefused IpqpDenseSolveCheck.ADenseStepSolveThatMissesRhoEndsTheTierNumerical"
WANT_refine_off="DenseSolveCheck.RefinementRescuesASolveThatMissesAtFirst"
WANT_refine_always="DenseSolveCheck.APassingDenseSolveIsUsedUnchanged"
WANT_dense_only_off="DenseArmEngines.TheSsnRefusesAnUnshiftedDenseNewtonStepThatMissesK DenseSolveCheck.OnlyUnshiftedDenseNumericsAreChecked"
WANT_ssn_unwired="DenseArmEngines.TheSsnRefusesAnUnshiftedDenseNewtonStepThatMissesK"
WANT_ipqp_unwired="IpqpDenseSolveCheck.ADenseStepSolveThatMissesRhoEndsTheTierNumerical"
WANT_ipqp_response_off="IpqpDenseSolveCheck.ADenseStepSolveThatMissesRhoEndsTheTierNumerical"
WANT_gram_dense_clause_off="DenseSolveCheck.TheRankGramRefusesADenseFactorThatIsNotPositiveDefinite"
WANT_gram_unwired=""
verdict=FAILPROOF_AS_DECLARED
heads=$(grep -hE '^\[  PASSED  \]' "$EV/run-head.txt" | awk '{s+=$4} END{print s+0}')
[ -s "$EV/failing-head.txt" ] && verdict=FAILPROOF_HEAD_FAILS
[ "$heads" = "$HEAD_COUNT" ] || verdict="FAILPROOF_HEAD_COUNT($heads)"
for M in $MUTS; do
  v=WANT_${M//-/_}
  got=$(tr '\n' ' ' < "$EV/failing-$M.txt" | sed 's/ $//')
  if [ "$got" != "${!v}" ]; then verdict="FAILPROOF_MISMATCH($M)"; echo "MISMATCH $M got=[$got] want=[${!v}]"; fi
done
echo "$verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = FAILPROOF_AS_DECLARED ]
