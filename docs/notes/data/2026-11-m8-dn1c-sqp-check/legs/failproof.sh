#!/usr/bin/env bash
# M8 DN-1c (../DECLARATION.txt P14): THE FALSIFIER. Structure from ../../2026-11-m8-dn1b-guard/legs/failproof.sh. Three
# mutations of the code head's `git archive` export, each fail closed unless its site occurs exactly as counted:
#   guard-off  src/drivers/ipm_solver.cpp: both DN-1b guard gates
#              `if (this->kkt_sol_.arm() == hven::linear::FactorArm::kDense) [[unlikely]] {` (2 sites) -> `if (false) {`;
#   check-off  src/linear/dense_session.cpp: `if (opts_.dense_condition_check && !condition_ok(A)) {` (1 site) ->
#              `if (false) {` -- the condition check never refuses;
#   unwired    include/hven/detail/kkt/kkt_calls.h: `o.dense_condition_check = dense != hven::linear::DensePolicy::kNever;`
#              (1 site) -> `o.dense_condition_check = false;` -- the SQP's factors never ask for it.
# Each built (Release, legs/build.sh's flags, hven_tests, hven_qp_walk_seam_tests and hven_drivers_tests) at
# build-dn1ce/fp-<name>; the code head's own binaries are build-dn1ce/rel's. Filters: hven_tests 'DenseArm*',
# hven_qp_walk_seam_tests 'QpDenseArm.*', hven_drivers_tests 'DenseArmEngines.*'. Criterion (every listed set EXACT):
#   head       every test of both filters PASSES;
#   guard-off  fails exactly IpmResidualGuardRecoversANumericallySingularDenseFactor and ...DenseFactorProximal
#              (DenseArmEngines), each with the dense status differing from the sparse one's;
#   check-off  fails exactly DenseArmConditionCheck.SingularToWorkingPrecisionFallsBack,
#              DenseArmConditionCheck.RefusedUnderNeverAndCarriedByAFallbackHandle and
#              DenseArmEngines.SqpConditionCheckRetakesASingularToWorkingPrecisionKkt;
#   unwired    fails exactly QpDenseArm.SqpKktOptionsRunTheConditionCheckUnderADensePolicyOnly and
#              DenseArmEngines.SqpConditionCheckRetakesASingularToWorkingPrecisionKkt.
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1c
L=$WT/docs/notes/data/2026-11-m8-dn1c-sqp-check/legs
TREES=$WT/build-dn1ce
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1
run_filters() { # <build dir> <name>
  "$1/tests/hven_tests" --gtest_filter='DenseArm*' > "$EV/run-$2-hven_tests.txt" 2>&1; echo "$2 hven_tests rc=$?"
  "$1/tests/hven_qp_walk_seam_tests" --gtest_filter='QpDenseArm.*' > "$EV/run-$2-walk_seam.txt" 2>&1; echo "$2 walk_seam rc=$?"
  "$1/tests/drivers/hven_drivers_tests" --gtest_filter='DenseArmEngines.*' > "$EV/run-$2-drivers.txt" 2>&1; echo "$2 drivers rc=$?"
}
for M in guard-off check-off unwired; do
  S=$TREES/fp-src-$M
  rm -rf "$S" "$TREES/fp-$M"
  mkdir -p "$S"
  git archive "$HEAD_SHA" | tar -x -C "$S" || { echo ARCHIVE_FAIL; exit 1; }
  rm -rf "$S/dep/eigen" "$S/dep/fmt"
  cp -a dep/eigen "$S/dep/eigen" && cp -a dep/fmt "$S/dep/fmt" || exit 1
  case $M in
    guard-off) F=src/drivers/ipm_solver.cpp ;;
    check-off) F=src/linear/dense_session.cpp ;;
    unwired)   F=include/hven/detail/kkt/kkt_calls.h ;;
  esac
  python3 - "$S/$F" "$M" <<'PY' || { echo MUTATION_FAIL; exit 1; }
import sys
p, m = sys.argv[1], sys.argv[2]
s = open(p).read()
site, new, count = {
    'guard-off': ('if (this->kkt_sol_.arm() == hven::linear::FactorArm::kDense) [[unlikely]] {', 'if (false) {', 2),
    'check-off': ('if (opts_.dense_condition_check && !condition_ok(A)) {', 'if (false) {', 1),
    'unwired': ('o.dense_condition_check = dense != hven::linear::DensePolicy::kNever;',
                'o.dense_condition_check = false;', 1),
}[m]
print(m, 'sites', s.count(site), 'expected', count)
if s.count(site) != count:
    sys.exit(1)
open(p, 'w').write(s.replace(site, new))
PY
  diff <(git show "$HEAD_SHA:$F") "$S/$F" > "$EV/failproof-mutation-$M.diff"
  cat "$EV/failproof-mutation-$M.diff"
  SRC=$S bash "$L/build.sh" Release fp-$M hven_tests hven_qp_walk_seam_tests hven_drivers_tests > "$EV/failproof-build-$M.log" 2>&1 || { echo "FP_BUILD_FAIL $M"; tail -20 "$EV/failproof-build-$M.log"; exit 1; }
  run_filters "$TREES/fp-$M" "$M"
done
run_filters "$TREES/rel" head
python3 - "$EV" <<'PY' | tee "$EV/failproof-verdict.txt"
import re, sys
ev = sys.argv[1]
def res(name):
    out, txt = {}, ''
    for part in ('hven_tests', 'walk_seam', 'drivers'):
        t = open(f'{ev}/run-{name}-{part}.txt').read()
        txt += t
        out.update({m.group(2): m.group(1) for m in re.finditer(r'^\[\s+(OK|FAILED)\s+\] (\w+\.\w+) \(', t, re.M)})
    return out, txt
failed = lambda d: {k for k, v in d.items() if v == 'FAILED'}
h, _ = res('head')
want = {
    'guard-off': {'DenseArmEngines.IpmResidualGuardRecoversANumericallySingularDenseFactor',
                  'DenseArmEngines.IpmResidualGuardRecoversANumericallySingularDenseFactorProximal'},
    'check-off': {'DenseArmConditionCheck.SingularToWorkingPrecisionFallsBack',
                  'DenseArmConditionCheck.RefusedUnderNeverAndCarriedByAFallbackHandle',
                  'DenseArmEngines.SqpConditionCheckRetakesASingularToWorkingPrecisionKkt'},
    'unwired': {'QpDenseArm.SqpKktOptionsRunTheConditionCheckUnderADensePolicyOnly',
                'DenseArmEngines.SqpConditionCheckRetakesASingularToWorkingPrecisionKkt'},
}
ok = len(h) > 0 and not failed(h)
print('head     :', len(h), 'tests, failed', sorted(failed(h)) or 'none')
for m, w in want.items():
    r, t = res(m)
    moved = len(re.findall(r'dense\.status\n\s+Which is:.*\n\s+sparse\.status', t))
    good = set(r) == set(h) and failed(r) == w and (m != 'guard-off' or moved == 2)
    print(f'{m:9s}:', len(r), 'tests, failed', sorted(failed(r)), '| status-differs failures', moved, '|', 'OK' if good else 'NOT AS DECLARED')
    ok = ok and good
print('FAILPROOF_AS_DECLARED' if ok else 'FAILPROOF_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
PY
rc=${PIPESTATUS[0]}
echo "FOREGROUND_END $(date -Is)"
exit $rc
