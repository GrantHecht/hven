#!/usr/bin/env bash
# AP-2 B8 riders: the two builds made BEFORE ANY RIDER EDIT (run once, at the merge of origin/m8 cf164dcf
# into m8-b8, before the first rider edit):
#   1. build-b8r/psym  -- the P-SYM before arm: a fresh Release configure+build of THIS worktree at the merge
#      head (build.sh's flags, CCACHE_DISABLE=1), captured to build-b8r/psym-before. The after arm rebuilds
#      the same directory in place at the rider code head (legs/psym.sh).
#   2. build-b8r/armA  -- arm A = cf164dcf (origin/m8), built from its `git archive` export
#      build-b8r/armA-src (submodules dep/eigen bc3b3987, dep/fmt 407c905e copied from this worktree, the
#      commits cf164dcf pins); its corpus binary and compile database copied to build-b8r/armAbin/ and
#      build-b8r/armA-compile_commands.json.
# Run as: flock /tmp/box-build.lock bash prebuild.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
L=$WT/docs/notes/data/2026-11-m8-ap2-b8-riders/legs
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
echo "worktree HEAD $(git rev-parse HEAD) status: $(git status --porcelain --untracked-files=no | wc -l) modified tracked files"
bash "$L/build.sh" Release psym > "$WT/build-b8r/psym.build.log" 2>&1 || { echo PSYM_BUILD_FAIL; tail -20 "$WT/build-b8r/psym.build.log"; exit 1; }
tail -3 "$WT/build-b8r/psym.build.log"
rm -rf "$WT/build-b8r/psym-before"
bash scripts/psym_compare.sh capture "$WT/build-b8r/psym-before" "$WT/build-b8r/psym" || { echo CAPTURE_FAIL; exit 1; }
( cd "$WT/build-b8r/psym-before" && find . -type f | LC_ALL=C sort | xargs sha256sum ) | sha256sum | sed 's/^/before-capture digest /'
SRC=$WT/build-b8r/armA-src bash "$L/build.sh" Release armA > "$WT/build-b8r/armA.build.log" 2>&1 || { echo ARMA_BUILD_FAIL; tail -20 "$WT/build-b8r/armA.build.log"; exit 1; }
tail -3 "$WT/build-b8r/armA.build.log"
mkdir -p "$WT/build-b8r/armAbin"
cp -p "$WT/build-b8r/armA/bench/hven_sqp_corpus" "$WT/build-b8r/armAbin/hven_sqp_corpus" || exit 1
cp -p "$WT/build-b8r/armA/compile_commands.json" "$WT/build-b8r/armA-compile_commands.json" || exit 1
sha256sum "$WT/build-b8r/armAbin/hven_sqp_corpus" "$WT/build-b8r/armA-compile_commands.json" "$WT/build-b8/armAbin/hven_sqp_corpus"
echo "FOREGROUND_END $(date -Is)"
