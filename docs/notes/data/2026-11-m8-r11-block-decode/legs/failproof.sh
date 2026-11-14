#!/usr/bin/env bash
# M8 R11 (DECLARATION P14): THE FALSIFIER. The code head's `git archive` export at build-r11e/fp-src with the OLD
# 2x2 decode restored by ONE mutation of src/linear/dense_symmetric_factor.cpp's block_evidence(): the decode_2x2()
# call and its three uses are replaced by arm A's (trace +- sqrt(trace^2 - 4 det))/2 lines (fail closed unless the
# site occurs exactly once). Built (Release, legs/build.sh's flags, hven_tests only) at build-r11e/fp. Then the
# three R11 tests run on the mutant and on the code head's own hven_tests (build-r11e/rel).
# Criterion: on the code head all three PASS; on the mutant
#   EvidenceCountsTheNegativeOfAnIllScaled2x2Block FAILS (its neg_eigs check reads 0, expected 1),
#   EvidenceKeepsTheSmallMagnitudeOfAnIllScaled2x2Block FAILS (smallest |eig| 0, expected 1e-10),
#   EvidenceIsAbsentWhenDCarriesANonFiniteEntry PASSES (the 1x1 non-finite guard is not mutated).
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/r11
L=$WT/docs/notes/data/2026-11-m8-r11-block-decode/legs
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
rm -rf "$WT/build-r11e/fp-src" "$WT/build-r11e/fp"
mkdir -p "$WT/build-r11e/fp-src"
git archive "$HEAD_SHA" | tar -x -C "$WT/build-r11e/fp-src" || { echo ARCHIVE_FAIL; exit 1; }
rm -rf "$WT/build-r11e/fp-src/dep/eigen" "$WT/build-r11e/fp-src/dep/fmt"
cp -a dep/eigen "$WT/build-r11e/fp-src/dep/eigen" && cp -a dep/fmt "$WT/build-r11e/fp-src/dep/fmt" || exit 1
F=$WT/build-r11e/fp-src/src/linear/dense_symmetric_factor.cpp
python3 - "$F" <<'PY' || { echo MUTATION_FAIL; exit 1; }
import sys
p = sys.argv[1]
s = open(p).read()
site = '''            Block2x2 block;
            if (!decode_2x2(a11, a21, a22, block)) {
                return std::nullopt;
            }
            evidence.block_abs_eigs.push_back(block.abs_first);
            evidence.block_abs_eigs.push_back(block.abs_second);
            evidence.neg_eigs += block.neg;
'''
old = '''            const double trace = a11 + a22;
            const double det = a11 * a22 - a21 * a21;
            const double disc = std::sqrt(std::max(0.0, trace * trace - 4.0 * det));
            const double e1 = (trace + disc) / 2.0;
            const double e2 = (trace - disc) / 2.0;
            evidence.block_abs_eigs.push_back(std::abs(e1));
            evidence.block_abs_eigs.push_back(std::abs(e2));
            if (e1 < 0.0) {
                ++evidence.neg_eigs;
            }
            if (e2 < 0.0) {
                ++evidence.neg_eigs;
            }
'''
n = s.count(site)
print("mutation site count", n)
if n != 1:
    sys.exit(1)
open(p, 'w').write(s.replace(site, old))
PY
diff <(git show "$HEAD_SHA:src/linear/dense_symmetric_factor.cpp") "$F" | tee "$EV/failproof-mutation.diff"
SRC=$WT/build-r11e/fp-src bash "$L/build.sh" Release fp hven_tests > "$EV/failproof-build.log" 2>&1 || { echo FP_BUILD_FAIL; tail -20 "$EV/failproof-build.log"; exit 1; }
export MKL_NUM_THREADS=1
FILTER='DenseSymmetricFactorGrowth.EvidenceCountsTheNegativeOfAnIllScaled2x2Block:DenseSymmetricFactorGrowth.EvidenceKeepsTheSmallMagnitudeOfAnIllScaled2x2Block:DenseSymmetricFactorGrowth.EvidenceIsAbsentWhenDCarriesANonFiniteEntry'
"$WT/build-r11e/rel/tests/hven_tests" --gtest_filter="$FILTER" > "$EV/run-head.txt" 2>&1; echo "head rc=$?"
"$WT/build-r11e/fp/tests/hven_tests" --gtest_filter="$FILTER" > "$EV/run-mutant.txt" 2>&1; echo "mutant rc=$?"
python3 - "$EV" <<'PY' | tee "$EV/failproof-verdict.txt"
import re, sys
ev = sys.argv[1]
def res(p):
    t = open(p).read()
    return {m.group(2): m.group(1) for m in re.finditer(r'^\[\s+(OK|FAILED)\s+\] DenseSymmetricFactorGrowth\.(\w+) \(', t, re.M)}, t
h, _ = res(ev + '/run-head.txt')
m, mt = res(ev + '/run-mutant.txt')
N, S, A = 'EvidenceCountsTheNegativeOfAnIllScaled2x2Block', 'EvidenceKeepsTheSmallMagnitudeOfAnIllScaled2x2Block', 'EvidenceIsAbsentWhenDCarriesANonFiniteEntry'
print('head  :', h)
print('mutant:', m)
neg0 = re.search(r'ev->neg_eigs\s*\n\s*Which is: 0', mt) is not None
print('mutant neg_eigs reads 0:', neg0)
ok = all(h.get(k) == 'OK' for k in (N, S, A)) and m.get(N) == 'FAILED' and m.get(S) == 'FAILED' and m.get(A) == 'OK' and neg0
print('FAILPROOF_AS_DECLARED' if ok else 'FAILPROOF_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
PY
rc=${PIPESTATUS[0]}
echo "FOREGROUND_END $(date -Is)"
exit $rc
