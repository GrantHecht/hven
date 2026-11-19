#!/usr/bin/env bash
# WA-1 r7 dev runs (NOT legs; before DECLARATION-WA1-r7.md): dev.sh on the r7 code and the r7 base (origin/m8 64e384e2a,
# worktree wa1-base). Arm A: corpus from wa1-base, lane via train_setup.sh round r7b; arm B: this worktree at the code commit.
# Both frozen to build-wa/bin/{A,B}; replay once per arm and mode -> .scratch/r7/dev-replay; one TRAIN enumeration per arm ->
# .scratch/r7/dev-train-{A,B}; ctest Release and Debug of arm B (dev evidence).
# usage: dev_r7.sh <arm A sha> <code sha>   Run under: flock /tmp/box-build.lock setsid -w bash dev_r7.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1
L=$WT/docs/notes/data/2026-11-m8-wa1/legs
ARMA=${1:?arm A sha}; CODE=${2:?code sha}; DEVROOT=$WT/.scratch/r7
BASE=/home/ghecht/Projects/hven/.claude/worktrees/wa1-base
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
echo "FOREGROUND_START $(date -Is)"
[ "$(git -C "$BASE" rev-parse HEAD)" = "$ARMA" ] || { echo BASE_NOT_ARMA; exit 2; }
git -C "$WT" diff --quiet --ignore-submodules=all "$CODE" -- src include tests bench examples bindings CMakeLists.txt cmake scripts || { echo CODE_PATHS_DIFFER; exit 2; }
bash "$L/build.sh" "$BASE" Release rel hven_sqp_corpus || exit 1
bash "$L/build.sh" "$WT" Release rel || exit 1
bash "$L/build.sh" "$WT" Debug dbg || exit 1
mkdir -p "$WT/build-wa/bin/A" "$WT/build-wa/bin/B" "$DEVROOT"
cp -p "$BASE/build-wa/rel/bench/hven_sqp_corpus" "$WT/build-wa/bin/A/" || exit 1
cp -p "$WT/build-wa/rel/bench/hven_sqp_corpus" "$WT/build-wa/bin/B/" || exit 1
sha256sum "$WT/build-wa/bin/A/hven_sqp_corpus" "$WT/build-wa/bin/B/hven_sqp_corpus"
bash "$L/replay.sh" "$DEVROOT/dev-replay" A "$WT/build-wa/bin/A/hven_sqp_corpus" 1 || exit 1
bash "$L/replay.sh" "$DEVROOT/dev-replay" B "$WT/build-wa/bin/B/hven_sqp_corpus" 1 || exit 1
for M in walk ssn ipm; do
  python3 "$WT/scripts/compare_replay.py" "$DEVROOT/dev-replay/A-k1-$M.csv" "$DEVROOT/dev-replay/B-k1-$M.csv" "dev-A-vs-B-$M" > "$DEVROOT/dev-replay/compare-$M.txt" 2>&1; tail -1 "$DEVROOT/dev-replay/compare-$M.txt"
done
bash "$L/train_setup.sh" A "$ARMA" r7b || exit 1
bash "$L/train_setup.sh" B "$CODE" r7b || exit 1
LA=$(sha256sum "$WT/build-wa/bin/A/hven_bench_cell" | cut -d' ' -f1); LB=$(sha256sum "$WT/build-wa/bin/B/hven_bench_cell" | cut -d' ' -f1)
bash "$L/train_enum.sh" "$DEVROOT/dev-train-A" "$WT/build-wa/bin/A/hven_bench_cell" "$LA" || exit 1
bash "$L/train_enum.sh" "$DEVROOT/dev-train-B" "$WT/build-wa/bin/B/hven_bench_cell" "$LB" || exit 1
sha256sum "$WT/build-wa/bin/A/"* "$WT/build-wa/bin/B/"*
bash "$L/ctest.sh" "$WT" rel "$DEVROOT/dev-ctest-rel.log"; echo "ctest rel rc=$?"
bash "$L/ctest.sh" "$WT" dbg "$DEVROOT/dev-ctest-dbg.log"; echo "ctest dbg rc=$?"
echo "FOREGROUND_END $(date -Is)"
