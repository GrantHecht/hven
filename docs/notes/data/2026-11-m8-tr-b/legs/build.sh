#!/usr/bin/env bash
# command; the worktree's own trees configure with GIT_DIR/GIT_WORK_TREE so the bench stamp sees the head (G4 D-g).
# usage: build.sh <Release|Debug> <build-dir-name> [ninja targets...]     Run as: flock /tmp/box-build.lock bash build.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/tr-b
SRCDIR=${SRC:-$WT}
TYPE=${1:?Release|Debug}
DIR=$WT/.scratch/build-trb/${2:?dir}
shift 2
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
echo "source $SRCDIR"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/build-trb/tmp GIT_CEILING_DIRECTORIES=$WT
mkdir -p "$TMPDIR"
# The worktree's own trees re-configure on every call (cheap), so the bench stamp is re-resolved with the git environment.
if [ ! -f "$DIR/build.ninja" ] || [ "$SRCDIR" = "$WT" ]; then
  GITENV=()
  [ "$SRCDIR" = "$WT" ] && GITENV=(GIT_DIR=/home/ghecht/Projects/hven/.git/worktrees/tr-b GIT_WORK_TREE=$WT)
  env "${GITENV[@]}" cmake -S "$SRCDIR" -B "$DIR" -G Ninja -DCMAKE_BUILD_TYPE="$TYPE" -DHVEN_FP_MODE=SAFER_FAST \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/home/ghecht/Projects/hven/build/_deps/googletest-src -DHVEN_BUILD_BENCH=ON -DHVEN_BUILD_EXAMPLES=ON > "$DIR.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -30 "$DIR.configure.log"; exit 1; }
fi
setsid -w ninja -C "$DIR" -j16 -k 0 "$@"
rc=$?
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
