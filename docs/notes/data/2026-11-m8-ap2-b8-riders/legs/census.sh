#!/usr/bin/env bash
# AP-2 B8 riders (copied from B7 riders, re-pointed) (B5's instrument): the 57-cell walk census, parallel-tiered v1 at the script's defaults, on the
# rider-head corpus binary (arm B, build-b8r/armBbin, sha256 checked), against the PROMOTED re-derived baseline
# bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv (the two probe-stop rows read
# WorkLimit), compared by scripts/census_compare.py. Criterion: 57/57, 0 mismatches.
# usage: census.sh <evidence-dir> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash census.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4
EV=${1:?evidence dir}
BIN=$WT/build-b8r/armBbin/hven_sqp_corpus
EXPECT=${2:?expected arm B sha256}
BASE=$WT/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
# Arm B is frozen by the runtime gate's run_all.sh; frozen here the same way if the census runs first.
if [ ! -f "$BIN" ]; then mkdir -p "$(dirname "$BIN")"; cp -p "$WT/build-b8r/rel/bench/hven_sqp_corpus" "$BIN" || exit 1; fi
GOT=$(sha256sum "$BIN" | cut -d' ' -f1)
echo "binary $BIN sha256 $GOT"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
RUN=$WT/build-b8r/census-run
rm -rf "$RUN"
export MKL_NUM_THREADS=1
bash scripts/run_walk_census.sh --binary "$BIN" --out "$RUN" --baseline "$BASE" \
  --compare scripts/census_compare.py > "$EV/census-run.log" 2>&1
rc=$?
echo "census rc=$rc"
tail -6 "$EV/census-run.log"
for f in walk_census.csv compare.txt serial_confirm_list.txt; do
  [ -f "$RUN/$f" ] && cp "$RUN/$f" "$EV/$f"
done
echo "FOREGROUND_END $(date -Is)"
exit $rc
