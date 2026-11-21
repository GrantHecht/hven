#!/usr/bin/env bash
# M8 PF-1 (PF-HB's instrument, re-pointed): arm A, built once before the DECLARATION: bdd797e3 (origin/m8) from its
# `git archive` export build-pf1e/armA-src (dep/eigen bc3b3987, dep/fmt 407c905e copied from this worktree, the
# commits bdd797e3 pins), Release with legs/build.sh's flags, at build-pf1e/armA; its corpus binary and compile
# database copied to build-pf1e/armAbin/hven_sqp_corpus and build-pf1e/armA-compile_commands.json.
# Run as: flock /tmp/box-build.lock bash prebuild.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa2c5c3a798557d0e
L=$WT/docs/notes/data/2026-11-m8-pf1/legs
ARMA=bdd797e342d793ef7713256be04c67647caaad03
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/build-pf1e"
rm -rf "$WT/build-pf1e/armA-src" "$WT/build-pf1e/armA" "$WT/build-pf1e/armAbin"
mkdir -p "$WT/build-pf1e/armA-src"
git archive "$ARMA" | tar -x -C "$WT/build-pf1e/armA-src" || { echo ARCHIVE_FAIL; exit 1; }
echo "eigen pinned $(git rev-parse "$ARMA:dep/eigen") here $(git -C dep/eigen rev-parse HEAD)"
echo "fmt   pinned $(git rev-parse "$ARMA:dep/fmt") here $(git -C dep/fmt rev-parse HEAD)"
[ "$(git rev-parse "$ARMA:dep/eigen")" = "$(git -C dep/eigen rev-parse HEAD)" ] || { echo EIGEN_PIN_MISMATCH; exit 1; }
[ "$(git rev-parse "$ARMA:dep/fmt")" = "$(git -C dep/fmt rev-parse HEAD)" ] || { echo FMT_PIN_MISMATCH; exit 1; }
rm -rf "$WT/build-pf1e/armA-src/dep/eigen" "$WT/build-pf1e/armA-src/dep/fmt"
cp -a dep/eigen "$WT/build-pf1e/armA-src/dep/eigen" && cp -a dep/fmt "$WT/build-pf1e/armA-src/dep/fmt" || exit 1
SRC=$WT/build-pf1e/armA-src bash "$L/build.sh" Release armA > "$WT/build-pf1e/armA.build.log" 2>&1 || { echo ARMA_BUILD_FAIL; tail -20 "$WT/build-pf1e/armA.build.log"; exit 1; }
tail -3 "$WT/build-pf1e/armA.build.log"
mkdir -p "$WT/build-pf1e/armAbin"
cp -p "$WT/build-pf1e/armA/bench/hven_sqp_corpus" "$WT/build-pf1e/armAbin/hven_sqp_corpus" || exit 1
cp -p "$WT/build-pf1e/armA/compile_commands.json" "$WT/build-pf1e/armA-compile_commands.json" || exit 1
sha256sum "$WT/build-pf1e/armAbin/hven_sqp_corpus" "$WT/build-pf1e/armA-compile_commands.json"
echo "FOREGROUND_END $(date -Is)"
