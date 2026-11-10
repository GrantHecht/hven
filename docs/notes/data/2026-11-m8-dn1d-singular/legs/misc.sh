#!/usr/bin/env bash
# M8 DN-1d (../DECLARATION.txt): copied from ../../2026-11-m8-dn1c-sqp-check/ and re-pointed (worktree dn1d, evidence this directory, arm A and arm B trees build-dn1de/, arm A 4776aa2ff rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1 r2 (../../DECLARATION-r2.txt): copied from ../../legs/ and re-pointed (evidence r2/, arm B trees build-dn1r2/; arm A read-only from build-dn1e/).
# M8 DN-1 impl: copied from R11 (2026-11-m8-r11-block-decode) and re-pointed: worktree dn1, build-dn1e, this directory, arm A 02065dac.
# M8 R11: copied from F1 (2026-11-m8-f1-init; runtime from its runtime-r2, period 400000; census legs from PF-1) and re-pointed: worktree r11, build-r11e, this directory, arm A a8c540cf.
# M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1 (PF-HB's instrument, re-pointed) (B8 riders' instrument, re-pointed): the two settler-added checks that are not builds of their own tree.
# usage: misc.sh pch <evidence-dir>       -- scripts/check_pch_neutrality.sh (C224)
#        misc.sh accelerate <evidence-dir> -- legs/accelerate_sweep.py (C222)
# Run as: flock /tmp/box-build.lock bash misc.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1d
WHAT=${1:?pch|accelerate}
EV=${2:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
cd "$WT" || exit 1
mkdir -p "$EV"
case $WHAT in
pch)
  rm -rf "$WT/build-dn1de/pch-check"
  HVEN_PCH_NEUTRALITY_JOBS=16 HVEN_PCH_NEUTRALITY_WORKDIR=$WT/build-dn1de/pch-check \
    bash scripts/check_pch_neutrality.sh > "$EV/pch-neutrality.txt" 2>&1
  rc=$?; tail -6 "$EV/pch-neutrality.txt" ;;
accelerate)
  # Pass A's criterion (DECLARATION P9): OTHER 0, and per file the same classes as the B8 riders' pass A
  # (../../2026-11-m8-ap2-b8-riders/accelerate/accelerate-sweep.txt); legs/accel_compare.py states it.
  # Pass B reported the same way.
  python3 "$WT/docs/notes/data/2026-11-m8-dn1d-singular/legs/accelerate_sweep.py" "$EV"
  echo "sweep rc=$?"
  PIN=$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/accelerate
  C=$WT/docs/notes/data/2026-11-m8-dn1d-singular/legs/accel_compare.py
  python3 "$C" "$PIN/accelerate-sweep.txt" "$EV/accelerate-sweep.txt" A; rc=$?
  python3 "$C" "$PIN/accelerate-sweep-stub.txt" "$EV/accelerate-sweep-stub.txt" B || true
  sha256sum "$PIN/accelerate-sweep.txt" "$EV/accelerate-sweep.txt" "$PIN/accelerate-sweep-stub.txt" "$EV/accelerate-sweep-stub.txt" ;;
*) rc=2 ;;
esac
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
