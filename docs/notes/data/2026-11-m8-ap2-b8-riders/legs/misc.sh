#!/usr/bin/env bash
# AP-2 B8 riders (B7 riders' instrument): the two settler-added checks that are not builds of their own tree.
# usage: misc.sh pch <evidence-dir>       -- scripts/check_pch_neutrality.sh (C224)
#        misc.sh accelerate <evidence-dir> -- legs/accelerate_sweep.py (C222)
# Run as: flock /tmp/box-build.lock bash misc.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
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
  rm -rf "$WT/build-b8r/pch-check"
  HVEN_PCH_NEUTRALITY_JOBS=16 HVEN_PCH_NEUTRALITY_WORKDIR=$WT/build-b8r/pch-check \
    bash scripts/check_pch_neutrality.sh > "$EV/pch-neutrality.txt" 2>&1
  rc=$?; tail -6 "$EV/pch-neutrality.txt" ;;
accelerate)
  # Pass A's criterion (DECLARATION P9): OTHER 0, and per file the same class as B8's pass A
  # (../../2026-11-m8-ap2-b8/accelerate/accelerate-sweep.txt), except the declared delta: B8's two
  # KNOWN-SIZEOF lines (tests/drivers/test_options_dump.cpp) are gone -- `accelerate_zero_tol` is declared on
  # every platform, so the Linux layout pin holds under USE_ACCELERATE_SPARSE -- and a file that no longer
  # mentions the macro leaves the sweep (listed). legs/accel_compare.py states it. Pass B reported the same way.
  python3 "$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/legs/accelerate_sweep.py" "$EV"
  echo "sweep rc=$?"
  PIN=$WT/docs/notes/data/2026-11-m8-ap2-b8/accelerate
  C=$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/legs/accel_compare.py
  python3 "$C" "$PIN/accelerate-sweep.txt" "$EV/accelerate-sweep.txt" A; rc=$?
  python3 "$C" "$PIN/accelerate-sweep-stub.txt" "$EV/accelerate-sweep-stub.txt" B || true
  sha256sum "$PIN/accelerate-sweep.txt" "$EV/accelerate-sweep.txt" "$PIN/accelerate-sweep-stub.txt" "$EV/accelerate-sweep-stub.txt" ;;
*) rc=2 ;;
esac
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
