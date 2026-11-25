#!/usr/bin/env bash
# Leg B (DECLARATION-QPC section 6): the arm installs, lane binaries and SENS-Q grids, and the BINARIES.txt lines.
#   usage: legB.sh <hven-bench checkout at 27c02687d9389ed43f3dbef27b4cbe389ca2b5d0> <scratch root> <out> ARM=SHA [ARM=SHA ...]
#   e.g.   legB.sh ~/Projects/hven-bench-<checkout at 27c02687> <scratch> gate-QPC/B QPC=<qpc sha> QPC-C1=<qpc-c1 sha> QPC-C2=<qpc-c2 sha>
# For each arm: a detached hven worktree at the sha (<scratch>/<arm>/src), scripts/census_build.sh (install
# <scratch>/<arm>/install, lane build <scratch>/<arm>/lane), legs/build_grid.sh (<scratch>/<arm>/grid). The R grid is built
# against the census install. EVERY grid (R's and each arm's) compiles the SAME test-support source: the QPC arm's tree
# (<scratch>/QPC/src/tests -- qp_dump.h differs between R and H, so a grid built from its own tree would differ by source,
# not by library); the QPC arm must therefore be among the ARM=SHA arguments. Prints one `declare` line per arm and one `QPC grid` tag line per grid; the caller pastes
# them into BINARIES.txt (with the H0 declare line of section 2) before any leg reads it.
# Run under: ~/.local/bin/box-lane -- bash legB.sh ...
set -euo pipefail
BENCH=$(realpath "$1"); SCR=$(realpath -m "$2"); OUT=$(realpath -m "$3"); shift 3
HERE=$(cd "$(dirname "$0")" && pwd); HVEN=${HVEN_REPO:-/home/ghecht/Projects/hven}
R_PIN=99712ec324716e6d08ee0545a43d595ca15eec3f
R_INST=/home/ghecht/Projects/hven/.claude/worktrees/census/.scratch/install
LANE_SRC=4319ab1512e2de6dbc385eb964be182f0af4eb70
mkdir -p "$SCR" "$OUT"
[ "$(git -C "$BENCH" rev-parse HEAD)" = "$(git -C "$BENCH" rev-parse 27c02687d9389ed43f3dbef27b4cbe389ca2b5d0)" ] || { echo "BENCH_NOT_AT_27c02687d9389ed43f3dbef27b4cbe389ca2b5d0"; exit 2; }
[ "$(git -C "$BENCH" rev-parse HEAD:lanes)" = "$LANE_SRC" ] || { echo "LANES_TREE_MISMATCH"; exit 2; }
grep -q "^HVEN_PIN_SHA $R_PIN" "$R_INST/HVEN_PIN.txt" || { echo "R_INSTALL_PIN_MISMATCH"; exit 2; }
printf '%s\n' "$@" | grep -q '^QPC=' || { echo "QPC_ARM_REQUIRED (the grids' common test-support tree)"; exit 2; }
for spec in "$@"; do
  arm=${spec%%=*}; sha=$(git -C "$HVEN" rev-parse "${spec#*=}")
  d=$SCR/$arm; mkdir -p "$d"
  [ -d "$d/src" ] || git -C "$HVEN" worktree add --detach "$d/src" "$sha" > /dev/null
  [ "$(git -C "$d/src" rev-parse HEAD)" = "$sha" ] || { echo "WORKTREE_NOT_AT $arm $sha"; exit 2; }
  git -C "$HVEN" merge-base --is-ancestor "$R_PIN" "$sha" || { echo "NOT_DESCENDED_FROM_R $arm"; exit 2; }
  bash "$BENCH/scripts/census_build.sh" "$sha" "$d/src" "$d" "$d/lane" > "$OUT/build-$arm.log" 2>&1
  for k in HVEN_CMAKE_ARGS HVEN_EIGEN_ALIGN HVEN_CXX_COMPILER; do
    [ "$(grep "^$k " "$d/install/HVEN_PIN.txt")" = "$(grep "^$k " "$R_INST/HVEN_PIN.txt")" ] || { echo "PIN_FIELD_DIFFERS $arm $k"; exit 2; }
  done
  [ "$(cat "$d/lane/lanes/hven/hven_bench_cell.lane-src")" = "$LANE_SRC" ] || { echo "LANE_SRC_DIFFERS $arm"; exit 2; }
  ls=$(sha256sum "$d/lane/lanes/hven/hven_bench_cell" | cut -d' ' -f1)
  echo "declare $arm options=H0 prefix=$d/install base=$R_PIN bin=$d/lane/lanes/hven/hven_bench_cell lane_src=$LANE_SRC $sha $ls" | tee -a "$OUT/binaries-lines.txt"
  git -C "$HVEN" diff --stat "$R_PIN" "$sha" -- src include > "$OUT/diffstat-$arm.txt"
done
TT=$SCR/QPC/src
rg=$(bash "$HERE/build_grid.sh" "$R_INST" "$TT" "$SCR/R-grid" | cut -d' ' -f1)
echo "QPC grid R $R_PIN $rg" | tee -a "$OUT/binaries-lines.txt"
for spec in "$@"; do
  arm=${spec%%=*}; sha=$(git -C "$HVEN" rev-parse "${spec#*=}"); d=$SCR/$arm
  gs=$(bash "$HERE/build_grid.sh" "$d/install" "$TT" "$d/grid" | cut -d' ' -f1)
  echo "QPC grid $arm $sha $gs" | tee -a "$OUT/binaries-lines.txt"
done
echo "LEG B done $(date -Is)"
