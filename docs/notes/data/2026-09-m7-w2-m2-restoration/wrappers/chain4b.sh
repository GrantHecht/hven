#!/usr/bin/env bash
# M7 W2 M2 chain 4b: the STEENBRC budget-ladder CONTROL, with the pair that
# produced the A-SQP read's own rows -- and with NO CMake of this task's own.
#
#   B'  configure the git-archived harness at 348932f (the sha run-A's counters
#       header names) OUT OF TREE against the harness's OWN pinned install for
#       hven 3904a477, build its own hven_bench_cell target, copy it aside
#   C   the six control legs, k in {2, 3, 6, 12, 17, 0}
#
# WHY THIS SHAPE. The first attempt wrote a CMakeLists naming `lanes/roster.cpp`
# -- a file that does not exist at 348932f, because H4 created it later. The
# archived tree carries its own CMakeLists and its own cmake/hven_pin.cmake, so
# the faithful control needs neither a CMake of mine nor an hven build of mine:
# the harness's pinned prefix for 3904a477 is still on the box and the re-pin
# task verified it unmutated. The binary is then the harness's own, at that sha,
# with the harness's own stamp -- the same binary SHAPE that produced run-A.
#
# THE PREFIX IS PASSED EXPLICITLY. `hven_pin.cmake` derives HVEN_PIN_PREFIX from
# its own directory, which in the ARCHIVE would resolve to a `.hven` that the
# archive does not carry (it is git-ignored). The harness CMakeLists takes
# CMAKE_PREFIX_PATH when one is given, and the resolved `hven_DIR` is printed
# AND ASSERTED to lie inside that prefix.
#
# NOTHING IN THE HARNESS TREE IS WRITTEN. The archive was produced by
# `git archive`, which reads the object database; this reads its pinned install.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
PINNED=/home/ghecht/Projects/hven-bench/.hven/install-3904a4777715
LOG=$S/logs/chain4b.log
mkdir -p "$S/logs" "$S/out/control"
exec >"$LOG" 2>&1
echo "CHAIN4B_START $(date -Is)"

# The throwaway hven worktree the superseded plan created is not needed: no hven
# is built here. Removed before anything else, and said out loud.
echo "--- REMOVING THE THROWAWAY WORKTREE (no hven is built by this chain)"
git -C "$ROOT" worktree remove --force "$S/wt-read" 2>&1 || echo "  (already absent)"
rm -rf "$S/rel-read" "$S/install-read" "$S/control-src"
git -C "$ROOT" worktree list

flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
PINNED=/home/ghecht/Projects/hven-bench/.hven/install-3904a4777715
echo "FOREGROUND_START chain4bB $(date -Is)"
echo "--- THE PINNED INSTALL THIS CONTROL LINKS (read-only; the re-pin task verified it unmutated)"
cat "$PINNED/HVEN_PIN.txt"
sha256sum "$PINNED/lib64/libhven.a" 2>/dev/null || sha256sum "$PINNED/lib/libhven.a"
rm -rf "$S/bench-read"
cmake -S "$S/harness-read" -B "$S/bench-read" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -Uhven_DIR \
  -DCMAKE_PREFIX_PATH="$PINNED" || exit 92
resolved=$(grep -m1 "^hven_DIR:" "$S/bench-read/CMakeCache.txt")
echo "--- $resolved"
case "$resolved" in
  *"$PINNED"*) echo "    RESOLVED INSIDE THE PINNED PREFIX -- ok" ;;
  *) echo "    REFUSED: hven_DIR is NOT inside $PINNED"; exit 94 ;;
esac
cmake --build "$S/bench-read" --target hven_bench_cell -j "$(nproc)" || exit 92
cp "$S/bench-read/lanes/hven/hven_bench_cell" "$S/hven_bench_cell_read" || exit 92
sha256sum "$S/hven_bench_cell_read"
echo "FOREGROUND_END chain4bB $(date -Is) rc=0"
'
rcB=$?
echo "WRAPPER_RC chain4bB rc=${rcB}"
[ $rcB -eq 0 ] || { echo "CHAIN4B STOPPED: the control build failed"; exit $rcB; }

flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
echo "FOREGROUND_START chain4bC $(date -Is)"
cd /home/ghecht/Projects/hven-bench || exit 90
worst=0
for k in 2 3 6 12 17 0; do
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  if [ -n "$foreign" ]; then
    echo "ABORTED before b$k: a foreign cell binary is alive under my lock: $foreign"
    exit 95
  fi
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
    "$S/hven_bench_cell_read" --lane hven-sqp --problem STEENBRC \
    --options default --budget-iters "$k" > "$S/out/control/STEENBRC-b$k.txt" 2>&1
  rc=$?
  echo "CONTROL LEG b$k rc=$rc"
  grep -E "^(hven_pin_sha|status|outcome|iterations|factorizations|objective|kkt_primal_eq|side.major_iters|side.restoration_iters)=" \
    "$S/out/control/STEENBRC-b$k.txt" | sed "s/^/    /"
  [ "$rc" = 0 ] || worst=1
done
echo "FOREGROUND_END chain4bC $(date -Is) rc=$worst"
exit $worst
'
rcC=$?
echo "WRAPPER_RC chain4bC rc=${rcC}"
echo "CHAIN4B_END $(date -Is)"
exit $rcC
