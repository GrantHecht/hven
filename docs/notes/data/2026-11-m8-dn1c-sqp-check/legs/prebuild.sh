#!/usr/bin/env bash
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1 impl: copied from R11 (2026-11-m8-r11-block-decode) and re-pointed: worktree dn1, build-dn1e, this directory, arm A 02065dac.
# M8 R11: copied from F1 (2026-11-m8-f1-init; runtime from its runtime-r2, period 400000; census legs from PF-1) and re-pointed: worktree r11, build-r11e, this directory, arm A a8c540cf.
# M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1 (PF-HB's instrument, re-pointed): arm A, built once before the DECLARATION: bdd797e3 (origin/m8) from its
# `git archive` export build-r11e/armA-src (dep/eigen bc3b3987, dep/fmt 407c905e copied from this worktree, the
# commits bdd797e3 pins), Release with legs/build.sh's flags, at build-r11e/armA; its corpus binary and compile
# database copied to build-r11e/armAbin/hven_sqp_corpus and build-r11e/armA-compile_commands.json.
# Run as: flock /tmp/box-build.lock bash prebuild.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1c
L=$WT/docs/notes/data/2026-11-m8-dn1c-sqp-check/legs
ARMA=2199d1a2fa96e279afc5a4e9a51c56f49333ad16
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/build-dn1ce"
rm -rf "$WT/build-dn1ce/armA-src" "$WT/build-dn1ce/armA" "$WT/build-dn1ce/armAbin"
mkdir -p "$WT/build-dn1ce/armA-src"
git archive "$ARMA" | tar -x -C "$WT/build-dn1ce/armA-src" || { echo ARCHIVE_FAIL; exit 1; }
echo "eigen pinned $(git rev-parse "$ARMA:dep/eigen") here $(git -C dep/eigen rev-parse HEAD)"
echo "fmt   pinned $(git rev-parse "$ARMA:dep/fmt") here $(git -C dep/fmt rev-parse HEAD)"
[ "$(git rev-parse "$ARMA:dep/eigen")" = "$(git -C dep/eigen rev-parse HEAD)" ] || { echo EIGEN_PIN_MISMATCH; exit 1; }
[ "$(git rev-parse "$ARMA:dep/fmt")" = "$(git -C dep/fmt rev-parse HEAD)" ] || { echo FMT_PIN_MISMATCH; exit 1; }
rm -rf "$WT/build-dn1ce/armA-src/dep/eigen" "$WT/build-dn1ce/armA-src/dep/fmt"
cp -a dep/eigen "$WT/build-dn1ce/armA-src/dep/eigen" && cp -a dep/fmt "$WT/build-dn1ce/armA-src/dep/fmt" || exit 1
SRC=$WT/build-dn1ce/armA-src bash "$L/build.sh" Release armA > "$WT/build-dn1ce/armA.build.log" 2>&1 || { echo ARMA_BUILD_FAIL; tail -20 "$WT/build-dn1ce/armA.build.log"; exit 1; }
tail -3 "$WT/build-dn1ce/armA.build.log"
mkdir -p "$WT/build-dn1ce/armAbin"
cp -p "$WT/build-dn1ce/armA/bench/hven_sqp_corpus" "$WT/build-dn1ce/armAbin/hven_sqp_corpus" || exit 1
cp -p "$WT/build-dn1ce/armA/compile_commands.json" "$WT/build-dn1ce/armA-compile_commands.json" || exit 1
sha256sum "$WT/build-dn1ce/armAbin/hven_sqp_corpus" "$WT/build-dn1ce/armA-compile_commands.json"
echo "FOREGROUND_END $(date -Is)"
