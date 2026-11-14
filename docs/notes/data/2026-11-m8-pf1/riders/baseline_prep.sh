#!/usr/bin/env bash
# export 888f0e88 (code == e55cecde) with the pinned deps copied from the riders worktree, then build Release `hven` only.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf1-riders
R=$WT/docs/notes/data/2026-11-m8-pf1/riders
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
rm -rf build-riders/base-src; mkdir -p build-riders/base-src
git archive 888f0e88 | tar -x -C build-riders/base-src || exit 1
[ "$(git rev-parse 888f0e88:dep/eigen)" = "$(git -C dep/eigen rev-parse HEAD)" ] || { echo EIGEN_PIN_MISMATCH; exit 1; }
[ "$(git rev-parse 888f0e88:dep/fmt)" = "$(git -C dep/fmt rev-parse HEAD)" ] || { echo FMT_PIN_MISMATCH; exit 1; }
rm -rf build-riders/base-src/dep/eigen build-riders/base-src/dep/fmt
cp -a dep/eigen build-riders/base-src/dep/eigen && cp -a dep/fmt build-riders/base-src/dep/fmt || exit 1
echo "FOREGROUND_END $(date -Is)"
