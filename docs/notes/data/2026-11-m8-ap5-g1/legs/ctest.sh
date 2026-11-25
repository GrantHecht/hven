#!/usr/bin/env bash
# M8 AP-5 G1 (../DECLARATION.txt): copied from ../../2026-11-m8-dn3b/legs/ and re-pointed in code lines only (worktree agent-afb4015ceb50aa1d1, build-g1/, this directory; prebuild.sh: arm A as its first argument; TMPDIR set to the ignored build-g1/tmp); older provenance lines below are historical.
# M8 DN-3b (../DECLARATION.txt): copied from ../../2026-11-m8-dn3a/legs/ (chain.sh from chain-r2.sh, failproof.sh from failproof-r2.sh) and re-pointed in code lines only (worktree agent-a24a4442b0864282f, build-g1/, this directory, arm A ccd2e951f); older provenance lines below are historical.
# M8 DN-3a (../DECLARATION.txt): copied from ../../2026-11-m8-ipm-nonum-restore/legs/ and re-pointed in code lines only (worktree agent-acfc8f7d568381148, build-dn3a/, this directory, arm A b70e45cfb, replay outputs dn3a-*); older provenance lines below are historical.
# M8 IPM no-numerics restoration routing (../DECLARATION.txt): copied from ../../2026-11-m8-ipm-ladder-status/ (legs/, runtime/) and re-pointed in code lines only (worktree agent-a3d6ec088fa738122, build-dn3a/, this directory, arm A b9626e7e5, replay outputs dn3a-*); older provenance lines below are historical.
# M8 IPM ladder status (../DECLARATION.txt): copied from ../../2026-11-m8-nan-gates/ (legs/, runtime-r2/ -> runtime/) and re-pointed in code lines only (worktree ipmlad, build-ipmlad/, this directory, arm A); older provenance lines below are historical.
# M8 NaN-sticky gates: copied from ../../2026-11-m8-dn1d-singular/r3/legs/ and re-pointed (worktree nan, build-nan/, this directory).
# Run as: flock /tmp/box-build.lock bash ctest.sh ...   (ctest -j4 under setsid)
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-afb4015ceb50aa1d1
DIR=$WT/build-g1/${1:?dir}
LOG=${2:?log}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-g1/tmp GIT_CEILING_DIRECTORIES=$WT/build-g1
mkdir -p "$TMPDIR"
cd "$DIR" || exit 1
setsid ctest -j4 --output-on-failure "$@" > "$LOG" 2>&1
rc=$?
tail -5 "$LOG"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
