#!/usr/bin/env bash
# M8 NaN-sticky gates (../DECLARATION.txt): the falsifier. Two mutations of the code head's `git archive` export of
# include/hven/core/detail/inf_norm.h, each fail closed unless its site occurs exactly once:
#   helper-lpnorm  nan_sticky_inf_norm returns v.lpNorm<Eigen::Infinity>() (the pre-change reduction at every site);
#   max-std        nan_sticky_max is std::max (the NaN test removed; the pre-change fold).
# Each built Release (legs/build.sh's flags; three test executables) at build-nan/fp-<name>; the head's binaries are
# build-nan/rel's. Filters: hven_tests 'NanSticky*', hven_qp_walk_seam_tests 'QpNanGates.*', hven_sqp_tests
# 'SqpRestorationNanGate.*'. Output: the failing test names per arm, one per line, in <evidence-dir>/failing-<arm>.txt.
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/nan
L=$WT/docs/notes/data/2026-11-m8-nan-gates/legs
TREES=${TREES:-$WT/build-nan}
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
MUTS=${MUTS:-helper-lpnorm max-std}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1
run_filters() { # <build dir> <name>
  : > "$EV/failing-$2.txt"
  "$1/tests/hven_tests" --gtest_filter='NanSticky*' > "$EV/run-$2-hven_tests.txt" 2>&1; echo "$2 hven_tests rc=$?"
  "$1/tests/hven_qp_walk_seam_tests" --gtest_filter='QpNanGates.*' > "$EV/run-$2-walk_seam.txt" 2>&1; echo "$2 walk_seam rc=$?"
  "$1/tests/sqp/hven_sqp_tests" --gtest_filter='SqpRestorationNanGate.*' > "$EV/run-$2-sqp.txt" 2>&1; echo "$2 sqp rc=$?"
  cat "$EV/run-$2-hven_tests.txt" "$EV/run-$2-walk_seam.txt" "$EV/run-$2-sqp.txt" \
    | grep -E '^\[  FAILED  \] [A-Za-z]+\.[A-Za-z]+ \(' | awk '{print $4}' | sort -u > "$EV/failing-$2.txt"
  grep -hE '^\[  PASSED  \]' "$EV/run-$2-hven_tests.txt" "$EV/run-$2-walk_seam.txt" "$EV/run-$2-sqp.txt"
  echo "$2 failing=$(wc -l < "$EV/failing-$2.txt"): $(tr '\n' ' ' < "$EV/failing-$2.txt")"
}
for M in $MUTS; do
  S=$TREES/fp-src-$M
  rm -rf "$S" "$TREES/fp-$M"
  mkdir -p "$S"
  git archive "$HEAD_SHA" | tar -x -C "$S" || { echo ARCHIVE_FAIL; exit 1; }
  rm -rf "$S/dep/eigen" "$S/dep/fmt"
  cp -a dep/eigen "$S/dep/eigen" && cp -a dep/fmt "$S/dep/fmt" || exit 1
  python3 - "$S/include/hven/core/detail/inf_norm.h" "$M" <<'PY' || { echo MUTATION_FAIL; exit 1; }
import sys
p, m = sys.argv[1], sys.argv[2]
s = open(p).read()
site, new = {
    'helper-lpnorm': ('    return v.cwiseAbs().template maxCoeff<Eigen::PropagateNaN>();\n',
                      '    return v.template lpNorm<Eigen::Infinity>();\n'),
    'max-std': ('    if (b != b) {\n        return b;\n    }\n', ''),
}[m]
n = s.count(site)
if n != 1:
    sys.exit(f'{m}: site occurs {n} times, expected 1')
open(p, 'w').write(s.replace(site, new))
print(f'MUTATED {m}')
PY
  SRC=$S bash "$L/build.sh" Release "fp-$M" hven_tests hven_qp_walk_seam_tests hven_sqp_tests > "$EV/build-$M.txt" 2>&1
  rc=$?; echo "build $M rc=$rc"; [ $rc -ne 0 ] && { tail -20 "$EV/build-$M.txt"; exit 1; }
  run_filters "$TREES/fp-$M" "$M"
done
run_filters "$TREES/rel" head
# The criterion (../DECLARATION.txt P4): the head fails nothing and runs all 13 tests; each mutation fails EXACTLY its
# declared set (WANT, measured on the dev run build-nan/dev/fp before the declaration).
WANT_helper_lpnorm="NanStickyInfNorm.ANaNAtEveryIndexReadsNaN NanStickyInfNorm.ANaNFirstMiddleOrLastReadsNaNThroughExpressions NanStickyInfNorm.AnInfReadsInfAndANaNBesideAnInfReadsNaN QpNanGates.TheBorderedShiftedRatioReadsANonFirstNaN QpNanGates.TheShiftedRatioReadsANonFirstNaNAndTheGateRefuses QpNanGates.TheShiftedRefinementMonitorDoesNotFallToANonFirstNaN QpNanGates.TheUnregularizedPolishDoesNotFallToANonFirstNaN SqpRestorationNanGate.TheSubgradientCertificateRefusesANonFirstNaN"
WANT_max_std="NanStickyMax.ANaNInEitherArgumentIsKept QpNanGates.TheVerdictFaceMeasureKeepsANaNInALaterRow"
verdict=FAILPROOF_AS_DECLARED
heads=$(grep -hE '^\[  PASSED  \]' "$EV/run-head-hven_tests.txt" "$EV/run-head-walk_seam.txt" "$EV/run-head-sqp.txt" | awk '{s+=$4} END {print s}')
[ -s "$EV/failing-head.txt" ] && verdict=FAILPROOF_HEAD_FAILS
[ "$heads" = 13 ] || verdict="FAILPROOF_HEAD_COUNT($heads)"
for M in $MUTS; do
  v=WANT_${M//-/_}
  got=$(tr '\n' ' ' < "$EV/failing-$M.txt" | sed 's/ $//')
  if [ "$got" != "${!v}" ]; then verdict="FAILPROOF_MISMATCH($M)"; echo "MISMATCH $M got=[$got] want=[${!v}]"; fi
done
echo "$verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = FAILPROOF_AS_DECLARED ]
