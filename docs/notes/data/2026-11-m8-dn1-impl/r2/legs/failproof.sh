#!/usr/bin/env bash
# M8 DN-1 r2 (../../DECLARATION-r2.txt): copied from ../../legs/ and re-pointed (evidence r2/, arm B trees build-dn1r2/; arm A read-only from build-dn1e/).
# M8 DN-1 impl (DECLARATION P14): THE FALSIFIER. Structure copied from R11's legs/failproof.sh (2026-11-m8-r11-block-decode),
# re-pointed (worktree dn1, build-dn1e, this directory) with a DN-1 mutation. The code head's `git archive` export at
# build-dn1r2/fp-src with ONE mutation of src/linear/dense_session.cpp's dense_block_walk(): the 2x2 branch's
# decode_2x2() read (signs from det and trace, a 2x2 det of exactly 0.0 -> singular) is replaced by the
# (trace +- sqrt(trace^2 - 4 det))/2 eigenvalue read DN-0 used, with only a non-finite guard (fail closed unless the
# site occurs exactly once). Built (Release, legs/build.sh's flags, hven_tests only) at build-dn1r2/fp. Then the
# DenseArm and DenseArmBlockWalk tests run on the mutant and on the code head's own hven_tests (build-dn1r2/rel).
# Criterion: on the code head every test PASSES; on the mutant
#   DenseArm.LakesMatrixCountsTheReferenceNegatives FAILS (negative reads 72, expected 78),
#   DenseArmBlockWalk.IllScaledNegativeDeterminantCountsOneNegative FAILS,
#   DenseArmBlockWalk.ZeroDeterminantIsSingular FAILS,
#   DenseArmBlockWalk.NonFiniteIsSingular FAILS (the finite block whose unscaled trace^2 overflows reads singular),
#   and every other DenseArm / DenseArmBlockWalk test PASSES.
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
L=$WT/docs/notes/data/2026-11-m8-dn1-impl/r2/legs
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
rm -rf "$WT/build-dn1r2/fp-src" "$WT/build-dn1r2/fp"
mkdir -p "$WT/build-dn1r2/fp-src"
git archive "$HEAD_SHA" | tar -x -C "$WT/build-dn1r2/fp-src" || { echo ARCHIVE_FAIL; exit 1; }
rm -rf "$WT/build-dn1r2/fp-src/dep/eigen" "$WT/build-dn1r2/fp-src/dep/fmt"
cp -a dep/eigen "$WT/build-dn1r2/fp-src/dep/eigen" && cp -a dep/fmt "$WT/build-dn1r2/fp-src/dep/fmt" || exit 1
F=$WT/build-dn1r2/fp-src/src/linear/dense_session.cpp
python3 - "$F" <<'PY' || { echo MUTATION_FAIL; exit 1; }
import sys
p = sys.argv[1]
s = open(p).read()
site = '''            Block2x2 block;
            bool det_zero = false;
            if (!decode_2x2(a[k * n + k], a[(k + 1) * n + k], a[(k + 1) * n + (k + 1)], block,
                            det_zero) ||
                det_zero) {
                out.singular = true;
                return out;
            }
            out.negative += block.neg;
            out.positive += 2 - block.neg;
'''
old = '''            const double a11 = a[k * n + k];
            const double a21 = a[(k + 1) * n + k];
            const double a22 = a[(k + 1) * n + (k + 1)];
            const double trace = a11 + a22;
            const double det = a11 * a22 - a21 * a21;
            const double disc = std::sqrt(std::max(0.0, trace * trace - 4.0 * det));
            const double e1 = (trace + disc) / 2.0;
            const double e2 = (trace - disc) / 2.0;
            if (!std::isfinite(e1) || !std::isfinite(e2)) {
                out.singular = true;
                return out;
            }
            out.negative += (e1 < 0.0 ? 1 : 0) + (e2 < 0.0 ? 1 : 0);
            out.positive += (e1 > 0.0 ? 1 : 0) + (e2 > 0.0 ? 1 : 0);
'''
n = s.count(site)
print("mutation site count", n)
if n != 1:
    sys.exit(1)
open(p, 'w').write(s.replace(site, old))
PY
diff <(git show "$HEAD_SHA:src/linear/dense_session.cpp") "$F" | tee "$EV/failproof-mutation.diff"
SRC=$WT/build-dn1r2/fp-src bash "$L/build.sh" Release fp hven_tests > "$EV/failproof-build.log" 2>&1 || { echo FP_BUILD_FAIL; tail -20 "$EV/failproof-build.log"; exit 1; }
export MKL_NUM_THREADS=1
FILTER='DenseArm.*:DenseArmBlockWalk.*'
"$WT/build-dn1r2/rel/tests/hven_tests" --gtest_filter="$FILTER" > "$EV/run-head.txt" 2>&1; echo "head rc=$?"
"$WT/build-dn1r2/fp/tests/hven_tests" --gtest_filter="$FILTER" > "$EV/run-mutant.txt" 2>&1; echo "mutant rc=$?"
python3 - "$EV" <<'PY' | tee "$EV/failproof-verdict.txt"
import re, sys
ev = sys.argv[1]
def res(p):
    t = open(p).read()
    return {m.group(2): m.group(1) for m in re.finditer(r'^\[\s+(OK|FAILED)\s+\] (DenseArm(?:BlockWalk)?\.\w+) \(', t, re.M)}, t
h, _ = res(ev + '/run-head.txt')
m, mt = res(ev + '/run-mutant.txt')
expect_fail = {'DenseArm.LakesMatrixCountsTheReferenceNegatives',
               'DenseArmBlockWalk.IllScaledNegativeDeterminantCountsOneNegative',
               'DenseArmBlockWalk.ZeroDeterminantIsSingular',
               'DenseArmBlockWalk.NonFiniteIsSingular'}
print('head  :', len(h), 'tests', sorted(k for k, v in h.items() if v != 'OK') or 'all OK')
print('mutant:', len(m), 'tests, failed', sorted(k for k, v in m.items() if v != 'OK'))
lakes72 = re.search(r'o\.inertia\.negative\s*\n\s*Which is: 72', mt) is not None
print('mutant LAKES negative reads 72:', lakes72)
ok = (len(h) > 0 and set(h) == set(m) and all(v == 'OK' for v in h.values())
      and {k for k, v in m.items() if v == 'FAILED'} == expect_fail and lakes72)
print('FAILPROOF_AS_DECLARED' if ok else 'FAILPROOF_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
PY
rc=${PIPESTATUS[0]}
echo "FOREGROUND_END $(date -Is)"
exit $rc
