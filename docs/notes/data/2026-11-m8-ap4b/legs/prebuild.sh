#!/usr/bin/env bash
# M8 AP-4b (../DECLARATION.md): G6's legs/prebuild.sh re-pointed (worktree g7, build-ap4b/, this directory); older provenance lines are historical.
# M8 DN-3b (../DECLARATION.txt): copied from ../../2026-11-m8-dn3a/legs/ (chain.sh from chain-r2.sh, failproof.sh from failproof-r2.sh) and re-pointed in code lines only (worktree agent-a24a4442b0864282f, build-g2/, this directory, arm A ccd2e951f); older provenance lines below are historical.
# M8 DN-3a (../DECLARATION.txt): structure from ../../2026-11-m8-dn1d-singular/r3/legs/prebuild.sh, re-pointed (this
# worktree, build-dn3a/, arm A = origin/m8 b70e45cfb). `git archive` export build-dn3a/armA-src (dep/eigen bc3b3987,
# dep/fmt 407c905e copied from this worktree, the commits b70e45cfb pins), Release with legs/build.sh's flags, at
# build-dn3a/armA; its corpus binary copied to build-dn3a/armAbin/hven_sqp_corpus.
# Run as: flock /tmp/box-build.lock bash prebuild.sh <arm A commit>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/ap4b
L=$WT/docs/notes/data/2026-11-m8-ap4b/legs
ARMA=${1:?arm A commit}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export GIT_CEILING_DIRECTORIES=$WT
cd "$WT" || exit 1
mkdir -p "$WT/build-ap4b"
rm -rf "$WT/build-ap4b/armA-src" "$WT/build-ap4b/armA" "$WT/build-ap4b/armAbin"
mkdir -p "$WT/build-ap4b/armA-src"
git archive "$ARMA" | tar -x -C "$WT/build-ap4b/armA-src" || { echo ARCHIVE_FAIL; exit 1; }
# the worktree's submodules are initialized at the pinned commits (no +/- in `git submodule status`); arm A takes copies of them.
git submodule status | grep -q '^[+-U]' && { echo SUBMODULE_NOT_AT_PIN; exit 1; }
[ "$(git rev-parse "$ARMA:dep/eigen")" = "$(git -C dep/eigen rev-parse HEAD)" ] || { echo EIGEN_PIN_MISMATCH; exit 1; }
[ "$(git rev-parse "$ARMA:dep/fmt")" = "$(git -C dep/fmt rev-parse HEAD)" ] || { echo FMT_PIN_MISMATCH; exit 1; }
rm -rf "$WT/build-ap4b/armA-src/dep/eigen" "$WT/build-ap4b/armA-src/dep/fmt"
cp -a dep/eigen "$WT/build-ap4b/armA-src/dep/eigen" && cp -a dep/fmt "$WT/build-ap4b/armA-src/dep/fmt" || exit 1
SRC=$WT/build-ap4b/armA-src bash "$L/build.sh" Release armA hven_sqp_corpus > "$WT/build-ap4b/armA.build.log" 2>&1 || { echo ARMA_BUILD_FAIL; tail -20 "$WT/build-ap4b/armA.build.log"; exit 1; }
tail -3 "$WT/build-ap4b/armA.build.log"
mkdir -p "$WT/build-ap4b/armAbin"
cp -p "$WT/build-ap4b/armA/bench/hven_sqp_corpus" "$WT/build-ap4b/armAbin/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-ap4b/armAbin/hven_sqp_corpus"
echo "FOREGROUND_END $(date -Is)"
