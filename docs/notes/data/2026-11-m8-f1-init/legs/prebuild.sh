#!/usr/bin/env bash
# M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1 (PF-HB's instrument, re-pointed): arm A, built once before the DECLARATION: bdd797e3 (origin/m8) from its
# `git archive` export build-f1/armA-src (dep/eigen bc3b3987, dep/fmt 407c905e copied from this worktree, the
# commits bdd797e3 pins), Release with legs/build.sh's flags, at build-f1/armA; its corpus binary and compile
# database copied to build-f1/armAbin/hven_sqp_corpus and build-f1/armA-compile_commands.json.
# Run as: flock /tmp/box-build.lock bash prebuild.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/f1
L=$WT/docs/notes/data/2026-11-m8-f1-init/legs
ARMA=2340b95da4992b2d53c307d83aac4be81b2853f3
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/build-f1"
rm -rf "$WT/build-f1/armA-src" "$WT/build-f1/armA" "$WT/build-f1/armAbin"
mkdir -p "$WT/build-f1/armA-src"
git archive "$ARMA" | tar -x -C "$WT/build-f1/armA-src" || { echo ARCHIVE_FAIL; exit 1; }
echo "eigen pinned $(git rev-parse "$ARMA:dep/eigen") here $(git -C dep/eigen rev-parse HEAD)"
echo "fmt   pinned $(git rev-parse "$ARMA:dep/fmt") here $(git -C dep/fmt rev-parse HEAD)"
[ "$(git rev-parse "$ARMA:dep/eigen")" = "$(git -C dep/eigen rev-parse HEAD)" ] || { echo EIGEN_PIN_MISMATCH; exit 1; }
[ "$(git rev-parse "$ARMA:dep/fmt")" = "$(git -C dep/fmt rev-parse HEAD)" ] || { echo FMT_PIN_MISMATCH; exit 1; }
rm -rf "$WT/build-f1/armA-src/dep/eigen" "$WT/build-f1/armA-src/dep/fmt"
cp -a dep/eigen "$WT/build-f1/armA-src/dep/eigen" && cp -a dep/fmt "$WT/build-f1/armA-src/dep/fmt" || exit 1
SRC=$WT/build-f1/armA-src bash "$L/build.sh" Release armA > "$WT/build-f1/armA.build.log" 2>&1 || { echo ARMA_BUILD_FAIL; tail -20 "$WT/build-f1/armA.build.log"; exit 1; }
tail -3 "$WT/build-f1/armA.build.log"
mkdir -p "$WT/build-f1/armAbin"
cp -p "$WT/build-f1/armA/bench/hven_sqp_corpus" "$WT/build-f1/armAbin/hven_sqp_corpus" || exit 1
cp -p "$WT/build-f1/armA/compile_commands.json" "$WT/build-f1/armA-compile_commands.json" || exit 1
sha256sum "$WT/build-f1/armAbin/hven_sqp_corpus" "$WT/build-f1/armA-compile_commands.json"
echo "FOREGROUND_END $(date -Is)"
