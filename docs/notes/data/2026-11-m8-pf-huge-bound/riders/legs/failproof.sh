#!/usr/bin/env bash
# usage: failproof.sh <out dir>: R1's fail-without-recovery proof, in the dev tree build-pfhb/rel (legs/build.sh's
# flags). For each site (probe, soc) the recovery call's guard is scratch-prefixed `false &&` in
# src/drivers/ipm_solver_globalization.cpp, hven_drivers_tests + hven_interior_tests rebuilt, both R1 tests run, and the
# file restored with `git checkout`; then a restored run. Never committed: the tree must be clean at the end.
source "$(dirname "$0")/common.sh"
OUT=${1:?}; mkdir -p "$OUT"; cd "$WT" || fin 1
G=src/drivers/ipm_solver_globalization.cpp
git diff --quiet -- src include tests || { echo TREE_NOT_CLEAN; fin 1; }
F1='HugeRealBound.TheAffineProbeRecoversTheFarPairsToo'; F2='SocRecovery.AFarSlackPairsCorrectedMultiplierStepIsRecovered'
run() { # tag
  ninja -C "$WT/build-pfhb/rel" -j16 hven_drivers_tests hven_interior_tests > "$OUT/$1-build.txt" 2>&1 || { echo BUILD_FAIL; return 1; }
  "$WT/build-pfhb/rel/tests/drivers/hven_drivers_tests" "--gtest_filter=$F1" > "$OUT/$1-probe-test.txt" 2>&1; p=$?
  "$WT/build-pfhb/rel/tests/interior/hven_interior_tests" "--gtest_filter=$F2" > "$OUT/$1-soc-test.txt" 2>&1; s=$?
  echo "$1: probe-test rc=$p soc-test rc=$s"; echo "$1 $p $s" >> "$OUT/summary.txt"
}
off() { # anchor-line tag
  python3 - "$G" "$1" "$2" <<'PY'
import sys
p, anchor, tag = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(p).read()
old = '        if (ctx.inequal_cons_ > 0) {\n' + anchor
assert s.count(old) == 1, 'anchor not unique'
s = s.replace(old, '        if (false && ctx.inequal_cons_ > 0) { // SCRATCH: ' + tag + '\n' + anchor)
open(p, 'w').write(s)
PY
}
: > "$OUT/summary.txt"; rc=0
off '            KktVector v_dxsl = kkt_view(DXSL, ctx);' 'probe-site recovery off' && git diff -- $G > "$OUT/probe-off.diff" && run probe-off || rc=1
git checkout -- $G
off '            const int pv = ctx.primal_vars_;' 'SOC-site recovery off' && git diff -- $G > "$OUT/soc-off.diff" && run soc-off || rc=1
git checkout -- $G
run restored || rc=1
git diff --quiet -- src include tests && echo TREE_CLEAN || { echo TREE_DIRTY; rc=1; }
cat "$OUT/summary.txt"
# expected: probe-off 1 0 ; soc-off 0 1 ; restored 0 0
[ "$(cat "$OUT/summary.txt")" = "$(printf 'probe-off 1 0\nsoc-off 0 1\nrestored 0 0')" ] && echo FAILPROOF_AS_DECLARED || { echo FAILPROOF_NOT_AS_DECLARED; rc=1; }
fin $rc
