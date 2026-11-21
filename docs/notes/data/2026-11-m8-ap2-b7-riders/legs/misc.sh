#!/usr/bin/env bash
# AP-2 B7 riders (B5's instrument): the two settler-added checks that are not builds of their own tree.
# usage: misc.sh pch <evidence-dir>       -- scripts/check_pch_neutrality.sh (C224)
#        misc.sh accelerate <evidence-dir> -- legs/accelerate_sweep.py (C222)
# Run as: flock /tmp/box-build.lock bash misc.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
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
  rm -rf "$WT/build-b7r/pch-check"
  HVEN_PCH_NEUTRALITY_JOBS=16 HVEN_PCH_NEUTRALITY_WORKDIR=$WT/build-b7r/pch-check \
    bash scripts/check_pch_neutrality.sh > "$EV/pch-neutrality.txt" 2>&1
  rc=$?; tail -6 "$EV/pch-neutrality.txt" ;;
accelerate)
  # Pass A's criterion: OTHER 0 and the sweep BYTE-IDENTICAL to B7's (read, never written); pass B (the
  # stub) is reported against B7's too -- its OTHER lines make the script's own rc 1, as at B4-B7.
  python3 "$WT/docs/notes/data/2026-11-m8-ap2-b7-riders/legs/accelerate_sweep.py" "$EV"
  echo "sweep rc=$?"
  PIN=$WT/docs/notes/data/2026-11-m8-ap2-b7/accelerate
  if cmp -s "$PIN/accelerate-sweep.txt" "$EV/accelerate-sweep.txt"; then echo "PASS_A_IDENTICAL_TO_B7"; rc=0; else echo "PASS_A_DIFFERS"; diff "$PIN/accelerate-sweep.txt" "$EV/accelerate-sweep.txt" | head -20; rc=1; fi
  if cmp -s "$PIN/accelerate-sweep-stub.txt" "$EV/accelerate-sweep-stub.txt"; then echo "PASS_B_IDENTICAL_TO_B7"; else echo "PASS_B_DIFFERS"; fi
  sha256sum "$PIN/accelerate-sweep.txt" "$EV/accelerate-sweep.txt" "$PIN/accelerate-sweep-stub.txt" "$EV/accelerate-sweep-stub.txt" ;;
*) rc=2 ;;
esac
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
