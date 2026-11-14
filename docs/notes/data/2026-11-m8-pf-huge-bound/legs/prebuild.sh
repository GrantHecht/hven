#!/usr/bin/env bash
# M8 PF-HB: arm A, built once before the DECLARATION: 6d10c70f (origin/m8, the B8 riders landed) from its
# `git archive` export build-pfe/armA-src (dep/eigen bc3b3987, dep/fmt 407c905e copied from this worktree, the
# commits 6d10c70f pins), Release with legs/build.sh's flags, at build-pfe/armA; its corpus binary and compile
# database copied to build-pfe/armAbin/hven_sqp_corpus and build-pfe/armA-compile_commands.json.
# Run as: flock /tmp/box-build.lock bash prebuild.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-ab44eb20498de1642
L=$WT/docs/notes/data/2026-11-m8-pf-huge-bound/legs
ARMA=6d10c70f952fa39cd416e291ff2596ad4133b012
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/build-pfe"
rm -rf "$WT/build-pfe/armA-src" "$WT/build-pfe/armA" "$WT/build-pfe/armAbin"
mkdir -p "$WT/build-pfe/armA-src"
git archive "$ARMA" | tar -x -C "$WT/build-pfe/armA-src" || { echo ARCHIVE_FAIL; exit 1; }
echo "eigen pinned $(git rev-parse "$ARMA:dep/eigen") here $(git -C dep/eigen rev-parse HEAD)"
echo "fmt   pinned $(git rev-parse "$ARMA:dep/fmt") here $(git -C dep/fmt rev-parse HEAD)"
[ "$(git rev-parse "$ARMA:dep/eigen")" = "$(git -C dep/eigen rev-parse HEAD)" ] || { echo EIGEN_PIN_MISMATCH; exit 1; }
[ "$(git rev-parse "$ARMA:dep/fmt")" = "$(git -C dep/fmt rev-parse HEAD)" ] || { echo FMT_PIN_MISMATCH; exit 1; }
rm -rf "$WT/build-pfe/armA-src/dep/eigen" "$WT/build-pfe/armA-src/dep/fmt"
cp -a dep/eigen "$WT/build-pfe/armA-src/dep/eigen" && cp -a dep/fmt "$WT/build-pfe/armA-src/dep/fmt" || exit 1
SRC=$WT/build-pfe/armA-src bash "$L/build.sh" Release armA > "$WT/build-pfe/armA.build.log" 2>&1 || { echo ARMA_BUILD_FAIL; tail -20 "$WT/build-pfe/armA.build.log"; exit 1; }
tail -3 "$WT/build-pfe/armA.build.log"
mkdir -p "$WT/build-pfe/armAbin"
cp -p "$WT/build-pfe/armA/bench/hven_sqp_corpus" "$WT/build-pfe/armAbin/hven_sqp_corpus" || exit 1
cp -p "$WT/build-pfe/armA/compile_commands.json" "$WT/build-pfe/armA-compile_commands.json" || exit 1
sha256sum "$WT/build-pfe/armAbin/hven_sqp_corpus" "$WT/build-pfe/armA-compile_commands.json"
echo "FOREGROUND_END $(date -Is)"
