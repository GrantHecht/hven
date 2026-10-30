#!/usr/bin/env bash
# M7 W2 M2 chain 4, in THREE separate lock segments so the harness lane can
# interleave between them (its 1800 s tier holds the lock per cell; a single
# hour-long hold of mine would starve it):
#
#   A  rebuild the probe with the corrected x_k rule and RE-TAKE all fourteen
#      ladder legs, so the whole ladder comes from ONE binary
#   B  a throwaway worktree at hven 3904a477 -- the A-SQP read's own pin -- with
#      its submodules initialised, installed, and the harness's cell binary
#      sources built against it
#   C  the STEENBRC budget-ladder CONTROL at that pin, at the read's own k values
#
# WHY B EXISTS AT ALL. The read's STEENBRC ladder was measured at
# qp_failure_budget = 1; M7 W2 M1 then SHIPPED 3, which moves that cell
# (32 137 -> 42 946 factorizations, M1's own table). "Reproduced byte for byte"
# is therefore only meaningful at the READ's pin, and the difference at HEAD is
# M1's effect and is declared as such.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
PIN=3904a477771524b85b34678fb7d2ff55dd4768f1
LOG=$S/logs/chain4.log
mkdir -p "$S/logs" "$S/out/ladder" "$S/out/ladder-off" "$S/out/control"
exec >"$LOG" 2>&1
echo "CHAIN4_START $(date -Is)"

# --- SEGMENT A -------------------------------------------------------------
flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
echo "FOREGROUND_START chain4A $(date -Is)"
cmake --build "$S/probebuild" --target m2_probe -j "$(nproc)" || exit 91
echo "probe sha256: $(sha256sum "$S/probebuild/m2_probe")"
cd /home/ghecht/Projects/hven-bench || exit 90
worst=0
for cell in STEENBRB STEENBRC STEENBRG ZAMB2-8 YORKNET BRITGAS READING6; do
  for arm in on off; do
    foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
    if [ -n "$foreign" ]; then
      echo "ABORTED before $cell/$arm: a foreign cell binary is alive under my lock: $foreign"
      exit 95
    fi
    dir=$S/out/ladder; [ "$arm" = off ] && dir=$S/out/ladder-off
    MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
      "$S/probebuild/m2_probe" --problem "$cell" --callback "$arm" \
      > "$dir/$cell.txt" 2>"$dir/$cell.stderr"
    rc=$?
    echo "LEG $cell $arm rc=$rc"
    [ "$rc" = 0 ] || worst=1
  done
done
echo "FOREGROUND_END chain4A $(date -Is) rc=$worst"
exit $worst
'
rcA=$?
echo "WRAPPER_RC chain4A rc=${rcA}"
echo "--- SUMMARISE after A (no solver: pure file folding)"
python3 -B "$S/summarise.py" > "$S/logs/summarise-A.txt" 2>&1
echo "SUMMARISE_RC=$?"
tail -5 "$S/logs/summarise-A.txt"
[ $rcA -eq 0 ] || { echo "CHAIN4 STOPPED: segment A failed"; exit $rcA; }

# --- SEGMENT B -------------------------------------------------------------
# The worktree is THROWAWAY and lives under the git-ignored scratch parent. Its
# submodules must be initialised by hand: `git worktree add` leaves dep/eigen
# and dep/fmt empty (found by M7 W2 M1's own fix round).
flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
PIN=3904a477771524b85b34678fb7d2ff55dd4768f1
echo "FOREGROUND_START chain4B $(date -Is)"
cd "$ROOT" || exit 90
rm -rf "$S/wt-read" "$S/rel-read" "$S/install-read" "$S/bench-read" "$S/harness-read"
# THE HARNESS AT THE READS OWN SHA, exported READ-ONLY. The run-A counters file
# names it: harness.sha 348932f. Todays harness compiles against an hven
# far ahead of 3904a477 and need not build against it at all, and the faithful
# control is the pair that produced the rows anyway. `git archive` READS the
# object database and writes nothing in that tree -- a worktree would write
# `.git/worktrees/` metadata there, and the re-pin implementer is its writer.
mkdir -p "$S/harness-read" || exit 92
git -C /home/ghecht/Projects/hven-bench archive 348932f6ca02fbda61a338ffbb59796f50c784e3 \
  | tar -x -C "$S/harness-read" || exit 92
echo "--- HARNESS SOURCE EXPORTED AT 348932f (read-only export, no write to that tree)"
ls "$S/harness-read" | head
git worktree add --detach "$S/wt-read" "$PIN" || exit 92
cd "$S/wt-read" || exit 92
git submodule update --init || exit 92
echo "--- THE WORKTREE IS AT"
git rev-parse HEAD
git -C dep/eigen rev-parse HEAD
git -C dep/fmt rev-parse HEAD
echo "--- AND THE GITLINKS IT MUST MATCH"
git ls-tree "$PIN" dep/
cmake -S "$S/wt-read" -B "$S/rel-read" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_INSTALL_PREFIX="$S/install-read" \
  -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=OFF || exit 92
cmake --build "$S/rel-read" -j "$(nproc)" || exit 92
cmake --install "$S/rel-read" || exit 92
sha256sum "$S/install-read/lib64/libhven.a"
# The control binary is the HARNESS CELL, not the probe: the read measured with
# hven_bench_cell and "reproduced byte for byte" means that binary. It is built
# the same way the probe is -- the harness sources compiled by a CMake project
# of this task own, never the harness CMake, which refuses an unpinned prefix.
cmake -S "$S/control-src" -B "$S/bench-read" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DHVEN_BENCH_ROOT="$S/harness-read" \
  -DCMAKE_PREFIX_PATH="$S/install-read" || exit 93
cmake --build "$S/bench-read" --target hven_bench_cell_read -j "$(nproc)" || exit 93
sha256sum "$S/bench-read/hven_bench_cell_read"
echo "FOREGROUND_END chain4B $(date -Is) rc=0"
'
rcB=$?
echo "WRAPPER_RC chain4B rc=${rcB}"
[ $rcB -eq 0 ] || { echo "CHAIN4 STOPPED: segment B failed"; exit $rcB; }

# --- SEGMENT C -------------------------------------------------------------
flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
echo "FOREGROUND_START chain4C $(date -Is)"
cd /home/ghecht/Projects/hven-bench || exit 90
worst=0
for k in 2 3 6 12 17 0; do
  foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
  if [ -n "$foreign" ]; then
    echo "ABORTED before b$k: a foreign cell binary is alive under my lock: $foreign"
    exit 95
  fi
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
    "$S/bench-read/hven_bench_cell_read" --lane hven-sqp --problem STEENBRC \
    --options default --budget-iters "$k" > "$S/out/control/STEENBRC-b$k.txt" 2>&1
  rc=$?
  echo "CONTROL LEG b$k rc=$rc"
  grep -E "^(status|outcome|iterations|factorizations|objective|kkt_primal_eq)=" \
    "$S/out/control/STEENBRC-b$k.txt" | sed "s/^/    /"
  [ "$rc" = 0 ] || worst=1
done
echo "FOREGROUND_END chain4C $(date -Is) rc=$worst"
exit $worst
'
rcC=$?
echo "WRAPPER_RC chain4C rc=${rcC}"
echo "CHAIN4_END $(date -Is)"
exit $rcC
