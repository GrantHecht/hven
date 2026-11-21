#!/usr/bin/env bash
# M8 DN-3b (../DECLARATION.txt): copied from ../../2026-11-m8-dn3a/legs/ (chain.sh from chain-r2.sh, failproof.sh from failproof-r2.sh) and re-pointed in code lines only (worktree agent-a24a4442b0864282f, build-dn3b/, this directory, arm A ccd2e951f); older provenance lines below are historical.
# M8 DN-3a (../DECLARATION.txt): copied from ../../2026-11-m8-ipm-nonum-restore/legs/ and re-pointed in code lines only (worktree agent-acfc8f7d568381148, build-dn3a/, this directory, arm A b70e45cfb, replay outputs dn3a-*); older provenance lines below are historical.
# M8 IPM no-numerics restoration routing (../DECLARATION.txt): copied from ../../2026-11-m8-ipm-ladder-status/ (legs/, runtime/) and re-pointed in code lines only (worktree agent-a3d6ec088fa738122, build-dn3a/, this directory, arm A b9626e7e5, replay outputs dn3a-*); older provenance lines below are historical.
# M8 IPM ladder status (../DECLARATION.txt): copied from ../../2026-11-m8-nan-gates/ (legs/, runtime-r2/ -> runtime/) and re-pointed in code lines only (worktree ipmlad, build-ipmlad/, this directory, arm A); older provenance lines below are historical.
# M8 NaN-sticky gates: copied from ../../2026-11-m8-dn1d-singular/r3/legs/ and re-pointed (worktree nan, build-nan/, this directory).
# usage: build.sh <Release|Debug> <build-dir-name> [ninja targets...]
# Run as: flock /tmp/box-build.lock bash build.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a24a4442b0864282f
SRCDIR=${SRC:-$WT}
TYPE=${1:?Release|Debug}
DIR=${BUILD_ROOT:-$WT/build-dn3b}/${2:?dir}   # BUILD_ROOT: the dev falsifier only (scratch)
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
echo "source $SRCDIR"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1
mkdir -p "${BUILD_ROOT:-$WT/build-dn3b}"
if [ ! -f "$DIR/build.ninja" ]; then
  cmake -S "$SRCDIR" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
    -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
ninja -C "$DIR" -j16 -k 0 "$@"
rc=$?
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
