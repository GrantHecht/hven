#!/usr/bin/env bash
# AP-2 B3 riders: the 57-cell walk census, parallel-tiered v1 at the script's defaults, on the
# riders-head corpus binary (arm B, sha256 checked), against the PROMOTED re-derived baseline
# bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv (the two probe-stop rows read
# WorkLimit), compared by scripts/census_compare.py. Criterion: 57/57, 0 mismatches.
# usage: census.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash census.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a3b067b098f5e9221
EV=${1:?evidence dir}
BIN=$WT/build-r/armB/hven_sqp_corpus
EXPECT=24a097eaad6abacc0a5ff9a689c7b39ca245cc5961e98cfc832e10afca5a061d
BASE=$WT/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
GOT=$(sha256sum "$BIN" | cut -d' ' -f1)
echo "binary $BIN sha256 $GOT"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
RUN=$WT/build-r/census-run
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
