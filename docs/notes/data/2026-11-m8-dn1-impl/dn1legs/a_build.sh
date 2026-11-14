#!/usr/bin/env bash
# M8 DN-1 A legs (../DECLARATION-dn1legs.txt): install arm B's Release tree (build-dn1r2/rel, the code head
# 2171f8dd4) and build the replay tool against the install (find_package). Then freeze the tool.
# Run as: flock /tmp/box-build.lock bash a_build.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
D=$WT/docs/notes/data/2026-11-m8-dn1-impl/dn1legs
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
git diff --quiet 2171f8dd4233b1bc21682fc250060294e7d42a3d -- src include cmake CMakeLists.txt || { echo SOURCE_NOT_CODE_HEAD; exit 4; }
P=$WT/build-dn1r2/inst-dn1
rm -rf "$P" "$WT/build-dn1r2/dn1tools"
cmake --install "$WT/build-dn1r2/rel" --prefix "$P" > "$WT/build-dn1r2/inst-dn1.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$D/tools" -B "$WT/build-dn1r2/dn1tools" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$P" > "$WT/build-dn1r2/dn1tools.configure.log" 2>&1 \
  || { echo CONFIGURE_FAIL; tail -20 "$WT/build-dn1r2/dn1tools.configure.log"; exit 1; }
cmake --build "$WT/build-dn1r2/dn1tools" || { echo BUILD_FAIL; exit 1; }
mkdir -p "$WT/build-dn1r2/dn1bin"
cp -p "$WT/build-dn1r2/dn1tools/dn1_replay" "$WT/build-dn1r2/dn1bin/dn1_replay" || exit 1
sha256sum "$WT/build-dn1r2/dn1bin/dn1_replay" "$P"/lib*/libhven.a "$WT/build-dn1r2/rel/libhven.a"
echo "FOREGROUND_END $(date -Is)"
