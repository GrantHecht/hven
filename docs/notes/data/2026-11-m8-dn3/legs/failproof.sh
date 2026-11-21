#!/usr/bin/env bash
# M8 DN-3 (../DECLARATION-r1.md): copied from ../../2026-11-m8-dn3b/legs/ and re-pointed in code lines only (worktree agent-a4a349c1df0402cb8, build-dn3/, this directory, arm A 09718f3dd); older provenance lines below are historical.
# DN-3 P4: the mutation table, the two filters (hven_drivers_tests DenseAuto.*:DenseArmEngines.*:Limits.*; hven_sqp_tests WsRankRepair.*), the built targets and the WANT sets are this round's (../DECLARATION-r1.md P4); the mechanism is unchanged.
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
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8
L=$WT/docs/notes/data/2026-11-m8-dn3/legs
TREES=${TREES:-$WT/build-dn3}
HEADTREE=${HEADTREE:-$WT/build-dn3/rel}
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
MUTS=${MUTS:-threshold-ignored ipm-sqp-threshold ipqp-ipm-threshold sqp-auto-refused ipm-auto-refused sqp-counter-off ipm-counter-off ipqp-counter-off gram-counter-off fingerprint-no-threshold}
F_DRV='DenseAuto.*:DenseArmEngines.*:Limits.*'
F_SQP='WsRankRepair.*'
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1
run_filter() { # <build dir> <name>   (fails closed when a binary is absent)
  : > "$EV/run-$2.txt"
  for pair in "tests/drivers/hven_drivers_tests|$F_DRV" "tests/sqp/hven_sqp_tests|$F_SQP"; do
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
SEL = 'include/hven/detail/kkt/dense_selection.h'
IPM = 'src/drivers/ipm_solver.cpp'
IPMH = 'include/hven/detail/drivers/ipm_solver_impl.h'
IPQP = 'src/qp/ipqp_engine.cpp'
KC = 'include/hven/detail/kkt/kkt_calls.h'
QE = 'include/hven/detail/qp/qp_engine.h'
WS = 'src/qp/ws_independence.cpp'
table = {
    'threshold-ignored': [(SEL, '        return engine_threshold;', '        return Index{1} << 40;', 1)],
    'ipm-sqp-threshold': [(IPM, 'opts.dense_factorization, hven::detail::kIpmDenseSelectionThreshold);',
                           'opts.dense_factorization, hven::detail::kSqpDenseSelectionThreshold);', 1)],
    'ipqp-ipm-threshold': [(IPQP, 'detail::dense_selection_threshold(dense, detail::kSqpDenseSelectionThreshold);',
                            'detail::dense_selection_threshold(dense, detail::kIpmDenseSelectionThreshold);', 1)],
    'sqp-auto-refused': [('src/drivers/sqp_options.cpp',
                          'enum_in_range(opts.linear.dense_factorization, hven::linear::DensePolicy::kAuto,',
                          'enum_in_range(opts.linear.dense_factorization, hven::linear::DensePolicy::kAlways,', 1)],
    'ipm-auto-refused': [('src/drivers/ipm_options.cpp',
                          'enum_in_range(lin.dense_factorization, hven::linear::DensePolicy::kAuto,',
                          'enum_in_range(lin.dense_factorization, hven::linear::DensePolicy::kAlways,', 1)],
    'sqp-counter-off': [(KC, '        dense_ += after.dense - before_.dense;\n', '', 1)],
    'ipm-counter-off': [(IPMH, '        return retired_dense_factorizations_ + kkt_sol_.counters().dense_factorize_count +\n'
                               '               kkt_sol_.counters().dense_singular_count;',
                         '        return 0;', 1)],
    'ipqp-counter-off': [(IPQP, '    out.dense_factorizations = (after.dense_factorize_count + after.dense_singular_count) -\n'
                                '                               (before.dense_factorize_count + before.dense_singular_count);\n',
                          '', 1)],
    'gram-counter-off': [(WS, '                    out.dense_factorizations = factor.counters().dense_factorize_count +\n'
                              '                                               factor.counters().dense_singular_count;\n',
                          '', 1)],
    'fingerprint-no-threshold': [(QE, 'return policy ^ (0xC2B2AE3D27D4EB4FULL', 'return policy; (void)(0xC2B2AE3D27D4EB4FULL', 1)],
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
  SRC=$S BUILD_ROOT=$TREES bash "$L/build.sh" Release "fp-$M" hven_drivers_tests hven_sqp_tests > "$EV/build-$M.txt" 2>&1
  rc=$?; echo "build $M rc=$rc"; [ $rc -ne 0 ] && { tail -20 "$EV/build-$M.txt"; exit 1; }
  run_filter "$TREES/fp-$M" "$M"
done
run_filter "$HEADTREE" head
# The criterion (../DECLARATION-r1.md P4): the head runs HEAD_COUNT tests and fails none; each mutation fails EXACTLY its
# declared set (WANT, measured on a dev run before the declaration).
HEAD_COUNT=56
WANT_threshold_ignored="DenseAuto.AKktFactorSelectsEachPatternByItsOwnOrder DenseAuto.EachEngineHasItsOwnThreshold DenseAuto.IpmAboveItsThresholdIsThePlatformSession DenseAuto.SqpAboveItsThresholdIsThePlatformSession DenseAuto.TheIpqpTierSelectsByTheSqpThreshold DenseAuto.TheSsnTierSelectsByTheSqpThreshold DenseAuto.TheWalkSelectsByTheSqpThreshold WsRankRepair.TheGramFactorCountsItsDenseFactorization"
WANT_ipm_sqp_threshold="DenseAuto.IpmBelowItsThresholdIsTheDenseArm"
WANT_ipqp_ipm_threshold="DenseAuto.TheIpqpTierSelectsByTheSqpThreshold"
WANT_sqp_auto_refused="DenseArmEngines.SqpValidateAcceptsEveryPolicyAndNoThreshold DenseAuto.SqpAboveItsThresholdIsThePlatformSession DenseAuto.SqpBelowItsThresholdIsTheDenseArm Limits.SqpValidateRefusesTheNewCommonValues"
WANT_ipm_auto_refused="DenseArmEngines.IpmValidateAcceptsEveryPolicyAndNoThreshold DenseAuto.IpmAboveItsThresholdIsThePlatformSession DenseAuto.IpmBelowItsThresholdIsTheDenseArm DenseAuto.IpmCountsPerCall DenseAuto.IpmCountsTheDenseArmsFactorizations Limits.IpmValidateRefusesTheNewCommonValues"
WANT_sqp_counter_off="DenseAuto.AKktFactorSelectsEachPatternByItsOwnOrder DenseAuto.SqpAboveItsThresholdIsThePlatformSession DenseAuto.SqpBelowItsThresholdIsTheDenseArm DenseAuto.TheSsnTierSelectsByTheSqpThreshold DenseAuto.TheWalkSelectsByTheSqpThreshold"
WANT_ipm_counter_off="DenseAuto.IpmBelowItsThresholdIsTheDenseArm DenseAuto.IpmCountsPerCall DenseAuto.IpmCountsTheDenseArmsFactorizations"
WANT_ipqp_counter_off="DenseAuto.TheIpqpTierSelectsByTheSqpThreshold"
WANT_gram_counter_off="WsRankRepair.TheGramFactorCountsItsDenseFactorization"
WANT_fingerprint_no_threshold="DenseAuto.TheWalkKeySeparatesThePolicies"
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
