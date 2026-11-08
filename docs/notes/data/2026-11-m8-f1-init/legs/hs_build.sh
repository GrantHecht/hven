#!/usr/bin/env bash
# M8 F1: install arm A (build-f1/armA) and arm B (build-f1/rel) and build hs_probe/ against each install prefix.
# Run as: flock /tmp/box-build.lock bash hs_build.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/f1
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1
rc=0
for arm in A B; do
  SRCB=$WT/build-f1/armA; [ $arm = B ] && SRCB=$WT/build-f1/rel
  P=$WT/build-f1/inst-$arm; O=$WT/build-f1/hs-$arm
  rm -rf "$P" "$O"
  cmake --install "$SRCB" --prefix "$P" > "$WT/build-f1/inst-$arm.log" 2>&1 || { echo "INSTALL_FAIL $arm"; rc=1; continue; }
  cmake -S "$WT/docs/notes/data/2026-11-m8-f1-init/hs_probe" -B "$O" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$P" > "$O.configure.log" 2>&1 || { echo "CONFIGURE_FAIL $arm"; rc=1; continue; }
  cmake --build "$O" > "$O.build.log" 2>&1 || { echo "BUILD_FAIL $arm"; tail -20 "$O.build.log"; rc=1; continue; }
  sha256sum "$P"/lib*/libhven.a "$O/hs_probe"
done
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
