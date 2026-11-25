#!/usr/bin/env bash
# AP-2 B7: the TRAIN count (../count/count_absent.cpp): every corpus cell's declared variable-bound values the
# default threshold reads as absent. Installs the B7-head Release build (build-b7e/rel), builds the counter
# against it with the source tree's bench/ and tests/sqp/ on the include path, runs it, and compares the output
# BYTE-FOR-BYTE with the count the DECLARATION was written from (../count/count-declared.txt).
# usage: count.sh <evidence-dir>    Run as: flock /tmp/box-build.lock bash count.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a00603e2048c1c37b
EV=${1:?evidence dir}
SRC=$WT/docs/notes/data/2026-11-m8-ap2-b7/count
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
mkdir -p "$EV"
PREFIX=$WT/build-b7e/inst-count
rm -rf "$PREFIX" "$WT/build-b7e/count-build"
cmake --install "$WT/build-b7e/rel" --prefix "$PREFIX" > "$WT/build-b7e/install-count.log" 2>&1 || { echo INSTALL_FAIL; exit 1; }
cmake -S "$SRC" -B "$WT/build-b7e/count-build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_PREFIX_PATH="$PREFIX" -DHVEN_SOURCE_ROOT="$WT" \
  > "$WT/build-b7e/count-build.configure.log" 2>&1 || { echo CONFIGURE_FAIL; tail -20 "$WT/build-b7e/count-build.configure.log"; exit 1; }
cmake --build "$WT/build-b7e/count-build" || { echo BUILD_FAIL; exit 1; }
"$WT/build-b7e/count-build/count_absent" "$EV/count-b7.txt"
rc=$?
echo "count rc=$rc"
if cmp -s "$SRC/count-declared.txt" "$EV/count-b7.txt"; then echo "COUNT_IDENTICAL_TO_DECLARED"; else echo "COUNT_DIFFERS"; diff "$SRC/count-declared.txt" "$EV/count-b7.txt" | head -20; rc=1; fi
sha256sum "$SRC/count-declared.txt" "$EV/count-b7.txt"
echo "FOREGROUND_END $(date -Is)"
exit $rc
