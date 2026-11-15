# M8 AP-5 G4 r4 (../DECLARATION-r4.md): copied from ../legs-r3/prebuild.sh with -r3 -> -r4 and g4r3 -> g4r4 in code lines only; nothing else.
# M8 AP-5 G4 r3 (../DECLARATION-r3.md): copied from ../legs/prebuild.sh; changes: L points at legs-r3. Older provenance lines below are historical.
# M8 AP-5 G4 (../DECLARATION.md): copied from ../../2026-11-m8-ap5-g2/ and re-pointed in code lines only (this worktree, build-g4/, this directory); older provenance lines below are historical.
# M8 AP-5 G2 (../DECLARATION.md): copied from ../../2026-11-m8-ap5-g1/ (legs-r3 / runtime-r3) and re-pointed in code lines only (worktree agent-a41fdd4ad89efc301, build-g2/, this directory); replay.sh, interior.sh and chain.sh are changed as the declaration says; older provenance lines below are historical.
#!/usr/bin/env bash
# M8 AP-5 G1 (../DECLARATION.txt): copied from ../../2026-11-m8-dn3b/legs/ and re-pointed in code lines only (worktree agent-a41fdd4ad89efc301, build-g2/, this directory; prebuild.sh: arm A as its first argument; TMPDIR set to the ignored build-g2/tmp); older provenance lines below are historical.
# M8 DN-3b (../DECLARATION.txt): copied from ../../2026-11-m8-dn3a/legs/ (chain.sh from chain-r2.sh, failproof.sh from failproof-r2.sh) and re-pointed in code lines only (worktree agent-a24a4442b0864282f, build-g2/, this directory, arm A ccd2e951f); older provenance lines below are historical.
# M8 DN-3a (../DECLARATION.txt): structure from ../../2026-11-m8-dn1d-singular/r3/legs/prebuild.sh, re-pointed (this
# worktree, build-dn3a/, arm A = origin/m8 b70e45cfb). `git archive` export build-dn3a/armA-src (dep/eigen bc3b3987,
# dep/fmt 407c905e copied from this worktree, the commits b70e45cfb pins), Release with legs/build.sh's flags, at
# build-dn3a/armA; its corpus binary copied to build-dn3a/armAbin/hven_sqp_corpus.
# Run as: flock /tmp/box-build.lock bash prebuild.sh <arm A commit>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a2559003d3a33fac4
L=$WT/docs/notes/data/2026-11-m8-ap5-g4/legs-r4
ARMA=${1:?arm A commit}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/build-g4"
rm -rf "$WT/build-g4/armA-src" "$WT/build-g4/armA" "$WT/build-g4/armAbin"
mkdir -p "$WT/build-g4/armA-src"
git archive "$ARMA" | tar -x -C "$WT/build-g4/armA-src" || { echo ARCHIVE_FAIL; exit 1; }
# G1: this worktree's dep/eigen and dep/fmt are plain copies of the main checkout's submodule trees (no .git of
# their own), so the pin is checked against the main checkout's submodule HEADs, from which they were copied.
MODS=/home/ghecht/Projects/hven/.git/modules/dep
[ "$(git rev-parse "$ARMA:dep/eigen")" = "$(cat "$MODS/eigen/HEAD")" ] || { echo EIGEN_PIN_MISMATCH; exit 1; }
[ "$(git rev-parse "$ARMA:dep/fmt")" = "$(cat "$MODS/fmt/HEAD")" ] || { echo FMT_PIN_MISMATCH; exit 1; }
diff -rq dep/eigen /home/ghecht/Projects/hven/dep/eigen -x .git > /dev/null || { echo EIGEN_COPY_DIFFERS; exit 1; }
diff -rq dep/fmt /home/ghecht/Projects/hven/dep/fmt -x .git > /dev/null || { echo FMT_COPY_DIFFERS; exit 1; }
rm -rf "$WT/build-g4/armA-src/dep/eigen" "$WT/build-g4/armA-src/dep/fmt"
cp -a dep/eigen "$WT/build-g4/armA-src/dep/eigen" && cp -a dep/fmt "$WT/build-g4/armA-src/dep/fmt" || exit 1
SRC=$WT/build-g4/armA-src bash "$L/build.sh" Release armA hven_sqp_corpus > "$WT/build-g4/armA.build.log" 2>&1 || { echo ARMA_BUILD_FAIL; tail -20 "$WT/build-g4/armA.build.log"; exit 1; }
tail -3 "$WT/build-g4/armA.build.log"
mkdir -p "$WT/build-g4/armAbin"
cp -p "$WT/build-g4/armA/bench/hven_sqp_corpus" "$WT/build-g4/armAbin/hven_sqp_corpus" || exit 1
sha256sum "$WT/build-g4/armAbin/hven_sqp_corpus"
echo "FOREGROUND_END $(date -Is)"
