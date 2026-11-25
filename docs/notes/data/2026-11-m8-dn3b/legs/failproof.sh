#!/usr/bin/env bash
# M8 DN-3b (../DECLARATION.txt): copied from ../../2026-11-m8-dn3a/legs/ (chain.sh from chain-r2.sh, failproof.sh from failproof-r2.sh) and re-pointed in code lines only (worktree agent-a24a4442b0864282f, build-dn3b/, this directory, arm A ccd2e951f); older provenance lines below are historical.
# M8 DN-3b (../DECLARATION.txt P4): the structure of ../../2026-11-m8-dn3a/legs/failproof-r2.sh (multi-site mutations of
# the code head's archive export, every site at its declared count or fail closed), with DN-3b's mutation table and two
# test binaries (hven_drivers_tests 'DenseArmEngines.*', hven_sqp_seam_tests '*Ipm*').
# Mutations:
#   report-off                 ipm_solver.cpp: the IPM keeps kSparseFallback under the dense arm (DN-3b's selection removed);
#   report-under-never         ipm_solver.cpp: kReport is selected under kNever too (the default must not move);
#   init-rescue-off            ipm_solver.cpp: a dense singular report at the multiplier estimate fails the phase (pre-DN-3b);
#   dc-off-on-failed           ipm_solver.cpp: the ladder's singularity signal ignores a factorization that left no numerics,
#                              so delta_c never engages on a singular report;
#   dense-singular-hard        kkt_factorization.cpp: kDenseSingular reads as a hard backend error (InvalidInput, both
#                              backend branches), so the exhausted ladder never reaches restoration;
#   flag-as-singular           ipm_solver.cpp: a flagged (numerically_singular) factorization reads singular to the ladder.
# Each built Release (legs/build.sh's flags; the two targets) at <TREES>/fp-<name>; the head's binaries are build-dn3b/rel's.
# Output: the failing names per arm in <evidence-dir>/failing-<arm>.txt.
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a24a4442b0864282f
L=$WT/docs/notes/data/2026-11-m8-dn3b/legs
TREES=${TREES:-$WT/build-dn3b}
HEADTREE=${HEADTREE:-$WT/build-dn3b/rel}
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
MUTS=${MUTS:-report-off report-under-never init-rescue-off dc-off-on-failed dense-singular-hard flag-as-singular}
F_DRV='DenseArmEngines.*'
F_SEAM='*Ipm*'
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1
run_filter() { # <build dir> <name>   (fails closed when a binary is absent)
  : > "$EV/run-$2.txt"
  for pair in "tests/drivers/hven_drivers_tests|$F_DRV" "tests/hven_sqp_seam_tests|$F_SEAM"; do
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
IPM = 'src/drivers/ipm_solver.cpp'
KF = 'src/interior/kkt_factorization.cpp'
SEL = ('    if (opts.dense_factorization != hven::linear::DensePolicy::kNever)\n'
       '        opts.dense_singular_policy = hven::linear::DenseSingularPolicy::kReport;\n')
table = {
    'report-off': [(IPM, SEL, '', 1)],
    'report-under-never': [(IPM, SEL, '    opts.dense_singular_policy = hven::linear::DenseSingularPolicy::kReport;\n', 1)],
    'init-rescue-off': [(IPM, '    const bool dense_singular = this->kkt_sol_.info() == Eigen::NumericalIssue &&\n'
                              '                                this->kkt_sol_.arm() == hven::linear::FactorArm::kDense;',
                         '    const bool dense_singular = false;', 1)],
    'dc-off-on-failed': [(IPM, 'auto SingularitySignal = [&]() { return Singular() || IncEigs < 0; };',
                          'auto SingularitySignal = [&]() {\n'
                          '        return (Singular() || IncEigs < 0) && this->kkt_sol_.info() == Eigen::Success;\n'
                          '    };', 1)],
    'dense-singular-hard': [(KF, '    if (backend_code == hven::linear::kDenseSingular) {\n        return Eigen::NumericalIssue;',
                             '    if (backend_code == hven::linear::kDenseSingular) {\n        return Eigen::InvalidInput;', 2)],
    'flag-as-singular': [(IPM, '        return (this->kkt_sol_.neigs() + this->kkt_sol_.peigs() - this->kkt_dim_) != 0;\n    };',
                          '        return (this->kkt_sol_.neigs() + this->kkt_sol_.peigs() - this->kkt_dim_) != 0 ||\n'
                          '               this->kkt_sol_.inertia_evidence().numerically_singular;\n    };', 1)],
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
  SRC=$S BUILD_ROOT=$TREES bash "$L/build.sh" Release "fp-$M" hven_drivers_tests hven_sqp_seam_tests > "$EV/build-$M.txt" 2>&1
  rc=$?; echo "build $M rc=$rc"; [ $rc -ne 0 ] && { tail -20 "$EV/build-$M.txt"; exit 1; }
  run_filter "$TREES/fp-$M" "$M"
done
run_filter "$HEADTREE" head
# The criterion (../DECLARATION.txt P4): the head runs HEAD_COUNT tests and fails none; each mutation fails EXACTLY its
# declared set (WANT, measured on a dev run before the declaration).
HEAD_COUNT=34
WANT_report_off="BothStrategies/IpmDenseReportRestorationSeam.AnExhaustedLadderOfSingularReportsIsRecoveredThroughRestoration/L1Nested BothStrategies/IpmDenseReportRestorationSeam.AnExhaustedLadderOfSingularReportsIsRecoveredThroughRestoration/ProximalSwitch DenseArmEngines.IpmLadderAnswersAnExactlySingularDenseKktOnTheDenseArm DenseArmEngines.IpmSolvesASingularKktProblemAsTheSparseArmDoes DenseArmEngines.IpmTrustsAFlaggedDenseFactorWhoseSolveIsAccurate IpmDenseReportSeam.ALadderOfSingularReportsEndsInAStatusNeverAThrowNorTheSparsePath"
WANT_report_under_never="DenseArmEngines.IpmLadderAnswersAnExactlySingularDenseKktOnTheDenseArm DenseArmEngines.IpmResidualGuardIsNeverEnteredOnSparseFactorizations DenseArmEngines.IpmResidualGuardRecoversANumericallySingularDenseFactor DenseArmEngines.IpmResidualGuardRecoversANumericallySingularDenseFactorProximal DenseArmEngines.IpmSolvesASingularKktProblemAsTheSparseArmDoes DenseArmEngines.IpmSolvesHs071AsTheSparseArmDoes DenseArmEngines.IpmTrustsAFlaggedDenseFactorWhoseSolveIsAccurate"
WANT_init_rescue_off="DenseArmEngines.IpmLadderAnswersAnExactlySingularDenseKktOnTheDenseArm DenseArmEngines.IpmSolvesASingularKktProblemAsTheSparseArmDoes"
WANT_dc_off_on_failed="DenseArmEngines.IpmLadderAnswersAnExactlySingularDenseKktOnTheDenseArm DenseArmEngines.IpmSolvesASingularKktProblemAsTheSparseArmDoes"
WANT_dense_singular_hard="BothStrategies/IpmDenseReportRestorationSeam.AnExhaustedLadderOfSingularReportsIsRecoveredThroughRestoration/L1Nested BothStrategies/IpmDenseReportRestorationSeam.AnExhaustedLadderOfSingularReportsIsRecoveredThroughRestoration/ProximalSwitch DenseArmEngines.IpmLadderAnswersAnExactlySingularDenseKktOnTheDenseArm DenseArmEngines.IpmSolvesASingularKktProblemAsTheSparseArmDoes IpmDenseReportSeam.ALadderOfSingularReportsEndsInAStatusNeverAThrowNorTheSparsePath"
WANT_flag_as_singular="BothStrategies/IpmDenseReportRestorationSeam.AnExhaustedLadderOfSingularReportsIsRecoveredThroughRestoration/L1Nested DenseArmEngines.IpmTrustsAFlaggedDenseFactorWhoseSolveIsAccurate"
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
