#!/usr/bin/env bash
# M8 DN-3a (../DECLARATION.txt): structure from ../../2026-11-m8-dn1d-singular/r3/legs/prebuild.sh, re-pointed (this
# worktree, build-dn3a/, arm A = origin/m8 b70e45cfb). `git archive` export build-dn3a/armA-src (dep/eigen bc3b3987,
# dep/fmt 407c905e copied from this worktree, the commits b70e45cfb pins), Release with legs/build.sh's flags, at
# build-dn3a/armA; its corpus binary copied to build-dn3a/armAbin/hven_sqp_corpus.
# Run as: flock /tmp/box-build.lock bash prebuild.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-acfc8f7d568381148
L=$WT/docs/notes/data/2026-11-m8-dn3a/legs
ARMA=b70e45cfb05e50a05852a1092351f5518fe268a8
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/build-dn3a"
rm -rf "$WT/build-dn3a/armA-src" "$WT/build-dn3a/armA" "$WT/build-dn3a/armAbin"
mkdir -p "$WT/build-dn3a/armA-src"
git archive "$ARMA" | tar -x -C "$WT/build-dn3a/armA-src" || { echo ARCHIVE_FAIL; exit 1; }
[ "$(git rev-parse "$ARMA:dep/eigen")" = "$(git -C dep/eigen rev-parse HEAD)" ] || { echo EIGEN_PIN_MISMATCH; exit 1; }
[ "$(git rev-parse "$ARMA:dep/fmt")" = "$(git -C dep/fmt rev-parse HEAD)" ] || { echo FMT_PIN_MISMATCH; exit 1; }
rm -rf "$WT/build-dn3a/armA-src/dep/eigen" "$WT/build-dn3a/armA-src/dep/fmt"
cp -a dep/eigen "$WT/build-dn3a/armA-src/dep/eigen" && cp -a dep/fmt "$WT/build-dn3a/armA-src/dep/fmt" || exit 1
SRC=$WT/build-dn3a/armA-src bash "$L/build.sh" Release armA hven_sqp_corpus > "$WT/build-dn3a/armA.build.log" 2>&1 || { echo ARMA_BUILD_FAIL; tail -20 "$WT/build-dn3a/armA.build.log"; exit 1; }
tail -3 "$WT/build-dn3a/armA.build.log"
mkdir -p "$WT/build-dn3a/armAbin"
cp -p "$WT/build-dn3a/armA/bench/hven_sqp_corpus" "$WT/build-dn3a/armAbin/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-dn3a/armAbin/hven_sqp_corpus"
echo "FOREGROUND_END $(date -Is)"
