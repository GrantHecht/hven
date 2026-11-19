#!/usr/bin/env bash
# WA-1 dev runs (NOT legs; before the DECLARATION): arm A (base worktree wa1-base, origin/m8 66cc44ccc) and arm B (this worktree at the
# code commit) corpus binaries frozen to build-wa/bin/{A,B}; replay once per arm and mode -> .scratch/dev-replay/{A,B}-k1-<mode>.csv;
# TRAIN lane binaries (train_setup.sh) and one enumeration per arm -> .scratch/dev-train-{A,B}.
# usage: dev.sh <arm A sha> <code sha>   Run under: flock /tmp/box-build.lock setsid -w bash dev.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1
BASE=/home/ghecht/Projects/hven/.claude/worktrees/wa1-base
L=$WT/docs/notes/data/2026-11-m8-wa1/legs
ARMA=${1:?arm A sha}; CODE=${2:?code sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
echo "FOREGROUND_START $(date -Is)"
[ "$(git -C "$BASE" rev-parse HEAD)" = "$ARMA" ] || { echo BASE_NOT_ARMA; exit 2; }
git -C "$WT" diff --quiet --ignore-submodules=all "$CODE" -- src include tests bench examples bindings CMakeLists.txt cmake scripts || { echo CODE_PATHS_DIFFER; exit 2; }
bash "$L/build.sh" "$BASE" Release rel hven_sqp_corpus || exit 1
bash "$L/build.sh" "$WT" Release rel || exit 1
bash "$L/build.sh" "$WT" Debug dbg || exit 1
mkdir -p "$WT/build-wa/bin/A" "$WT/build-wa/bin/B"
cp -p "$BASE/build-wa/rel/bench/hven_sqp_corpus" "$WT/build-wa/bin/A/" && cp -p "$WT/build-wa/rel/bench/hven_sqp_corpus" "$WT/build-wa/bin/B/" || exit 1
sha256sum "$WT/build-wa/bin/A/hven_sqp_corpus" "$WT/build-wa/bin/B/hven_sqp_corpus"
bash "$L/replay.sh" "$WT/.scratch/dev-replay" A "$WT/build-wa/bin/A/hven_sqp_corpus" 1 || exit 1
bash "$L/replay.sh" "$WT/.scratch/dev-replay" B "$WT/build-wa/bin/B/hven_sqp_corpus" 1 || exit 1
for M in walk ssn ipm; do
  python3 "$WT/scripts/compare_replay.py" "$WT/.scratch/dev-replay/A-k1-$M.csv" "$WT/.scratch/dev-replay/B-k1-$M.csv" "dev-A-vs-B-$M" > "$WT/.scratch/dev-replay/compare-$M.txt" 2>&1; tail -1 "$WT/.scratch/dev-replay/compare-$M.txt"
done
bash "$L/train_setup.sh" A "$ARMA" || exit 1
bash "$L/train_setup.sh" B "$CODE" || exit 1
LA=$(sha256sum "$WT/build-wa/bin/A/hven_bench_cell" | cut -d' ' -f1); LB=$(sha256sum "$WT/build-wa/bin/B/hven_bench_cell" | cut -d' ' -f1)
bash "$L/train_enum.sh" "$WT/.scratch/dev-train-A" "$WT/build-wa/bin/A/hven_bench_cell" "$LA" || exit 1
bash "$L/train_enum.sh" "$WT/.scratch/dev-train-B" "$WT/build-wa/bin/B/hven_bench_cell" "$LB" || exit 1
echo "FOREGROUND_END $(date -Is)"
