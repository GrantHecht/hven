#!/usr/bin/env bash
# M8 DN-1b (../DECLARATION.txt P14): THE FALSIFIER. Structure from ../../2026-11-m8-dn1-impl/r2/legs/failproof.sh, with
# DN-1b's two mutations of the code head's `git archive` export (src/drivers/ipm_solver.cpp only; fail closed unless
# each site occurs exactly as counted):
#   off      both guard gates `if (this->kkt_sol_.arm() == hven::linear::FactorArm::kDense) [[unlikely]] {` (step
#            solve, multiplier solve; 2 sites) become `if (false) {` -- the guard disabled;
#   ungated  the same 2 gates become `if (true) {` and the guard loop's arm break (1 site) is removed -- the guard
#            entered on sparse factorizations too.
# Each built (Release, legs/build.sh's flags, hven_drivers_tests only) at build-dn1be/fp-<name>. The DenseArmEngines
# tests run on the code head's own hven_drivers_tests (build-dn1be/rel) and on each mutant. Criterion:
#   head     every DenseArmEngines test PASSES;
#   off      exactly DenseArmEngines.IpmResidualGuardRecoversANumericallySingularDenseFactor FAILS, with the dense
#            solve's status differing from the sparse one's;
#   ungated  exactly DenseArmEngines.IpmResidualGuardIsNeverEnteredOnSparseFactorizations FAILS (UNGATED_FAIL).
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1b
L=$WT/docs/notes/data/2026-11-m8-dn1b-guard/legs
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
UNGATED_FAIL='DenseArmEngines.IpmResidualGuardIsNeverEnteredOnSparseFactorizations'
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1
for M in off ungated; do
  S=$WT/build-dn1be/fp-src-$M
  rm -rf "$S" "$WT/build-dn1be/fp-$M"
  mkdir -p "$S"
  git archive "$HEAD_SHA" | tar -x -C "$S" || { echo ARCHIVE_FAIL; exit 1; }
  rm -rf "$S/dep/eigen" "$S/dep/fmt"
  cp -a dep/eigen "$S/dep/eigen" && cp -a dep/fmt "$S/dep/fmt" || exit 1
  python3 - "$S/src/drivers/ipm_solver.cpp" "$M" <<'PY' || { echo MUTATION_FAIL; exit 1; }
import sys
p, m = sys.argv[1], sys.argv[2]
s = open(p).read()
gate = 'if (this->kkt_sol_.arm() == hven::linear::FactorArm::kDense) [[unlikely]] {'
brk = '''        if (this->kkt_sol_.arm() != hven::linear::FactorArm::kDense)
            break;
'''
print('gate sites', s.count(gate), 'loop arm-break sites', s.count(brk))
if s.count(gate) != 2 or s.count(brk) != 1:
    sys.exit(1)
if m == 'off':
    s = s.replace(gate, 'if (false) {')
else:
    s = s.replace(gate, 'if (true) {').replace(brk, '')
open(p, 'w').write(s)
PY
  diff <(git show "$HEAD_SHA:src/drivers/ipm_solver.cpp") "$S/src/drivers/ipm_solver.cpp" > "$EV/failproof-mutation-$M.diff"
  cat "$EV/failproof-mutation-$M.diff"
  SRC=$S bash "$L/build.sh" Release fp-$M hven_drivers_tests > "$EV/failproof-build-$M.log" 2>&1 || { echo "FP_BUILD_FAIL $M"; tail -20 "$EV/failproof-build-$M.log"; exit 1; }
  "$WT/build-dn1be/fp-$M/tests/drivers/hven_drivers_tests" --gtest_filter='DenseArmEngines.*' > "$EV/run-$M.txt" 2>&1; echo "$M rc=$?"
done
"$WT/build-dn1be/rel/tests/drivers/hven_drivers_tests" --gtest_filter='DenseArmEngines.*' > "$EV/run-head.txt" 2>&1; echo "head rc=$?"
python3 - "$EV" "$UNGATED_FAIL" <<'PY' | tee "$EV/failproof-verdict.txt"
import re, sys
ev, ungated = sys.argv[1], set(sys.argv[2].split(','))
def res(name):
    t = open(f'{ev}/run-{name}.txt').read()
    return {m.group(2): m.group(1) for m in re.finditer(r'^\[\s+(OK|FAILED)\s+\] (DenseArmEngines\.\w+) \(', t, re.M)}, t
h, _ = res('head'); o, ot = res('off'); u, _ = res('ungated')
failed = lambda d: {k for k, v in d.items() if v == 'FAILED'}
print('head   :', len(h), 'tests, failed', sorted(failed(h)) or 'none')
print('off    :', len(o), 'tests, failed', sorted(failed(o)))
print('ungated:', len(u), 'tests, failed', sorted(failed(u)))
status_moved = re.search(r'dense\.status\n\s+Which is:.*\n\s+sparse\.status', ot) is not None
print('off: dense status differs from sparse:', status_moved)
ok = (len(h) > 0 and set(h) == set(o) == set(u) and not failed(h)
      and failed(o) == {'DenseArmEngines.IpmResidualGuardRecoversANumericallySingularDenseFactor'} and status_moved
      and failed(u) == ungated
      and 'DenseArmEngines.IpmResidualGuardIsNeverEnteredOnSparseFactorizations' in ungated)
print('FAILPROOF_AS_DECLARED' if ok else 'FAILPROOF_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
PY
rc=${PIPESTATUS[0]}
echo "FOREGROUND_END $(date -Is)"
exit $rc
