#!/usr/bin/env bash
# AP-2 B8 (B7 riders' instrument): the two settler-added checks that are not builds of their own tree.
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
  rm -rf "$WT/build-b8/pch-check"
  HVEN_PCH_NEUTRALITY_JOBS=16 HVEN_PCH_NEUTRALITY_WORKDIR=$WT/build-b8/pch-check \
    bash scripts/check_pch_neutrality.sh > "$EV/pch-neutrality.txt" 2>&1
  rc=$?; tail -6 "$EV/pch-neutrality.txt" ;;
accelerate)
  # Pass A's criterion (DECLARATION P9): OTHER 0, and the sweep EQUAL to B7 riders' pass A once both are
  # normalized by legs/accel_normalize.py (line:col dropped; hven::solvers:: read as hven::) -- B8 moves
  # lines and the namespace, nothing else. Pass B (the stub) is reported against B7 riders' the same way.
  python3 "$WT/docs/notes/data/2026-11-m8-ap2-b8/legs/accelerate_sweep.py" "$EV"
  echo "sweep rc=$?"
  PIN=$WT/docs/notes/data/2026-11-m8-ap2-b7-riders/accelerate
  N=$WT/docs/notes/data/2026-11-m8-ap2-b8/legs/accel_normalize.py
  others=$(grep -c $'\tOTHER\t' "$EV/accelerate-sweep.txt")
  echo "pass A OTHER lines: $others"
  if [ "$others" = 0 ] && cmp -s <(python3 "$N" "$PIN/accelerate-sweep.txt") <(python3 "$N" "$EV/accelerate-sweep.txt"); then echo "PASS_A_EQUAL_TO_B7R_NORMALIZED"; rc=0; else echo "PASS_A_DIFFERS"; diff <(python3 "$N" "$PIN/accelerate-sweep.txt") <(python3 "$N" "$EV/accelerate-sweep.txt") | head -20; rc=1; fi
  if cmp -s <(python3 "$N" "$PIN/accelerate-sweep-stub.txt") <(python3 "$N" "$EV/accelerate-sweep-stub.txt"); then echo "PASS_B_EQUAL_TO_B7R_NORMALIZED"; else echo "PASS_B_DIFFERS"; fi
  sha256sum "$PIN/accelerate-sweep.txt" "$EV/accelerate-sweep.txt" "$PIN/accelerate-sweep-stub.txt" "$EV/accelerate-sweep-stub.txt" ;;
*) rc=2 ;;
esac
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
