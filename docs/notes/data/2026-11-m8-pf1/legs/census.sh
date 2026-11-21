#!/usr/bin/env bash
# M8 PF-1, copied from PF-HB (which copied the B8 riders) and re-pointed (worktree, build-pf1e, this directory); was: AP-2 B8 riders (copied from B7 riders, re-pointed) (B5's instrument): the 57-cell walk census, parallel-tiered v1 at the script's defaults, on the
# rider-head corpus binary (arm B, build-pf1e/armBbin, sha256 checked), against the PROMOTED re-derived baseline
# bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv (the two probe-stop rows read
# WorkLimit), compared by scripts/census_compare.py (reported) and legs/census_check.py (the criterion, DECLARATION P5).
# usage: census.sh <evidence-dir> <expected arm B sha256>   Run as: flock /tmp/box-build.lock bash census.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa2c5c3a798557d0e
EV=${1:?evidence dir}
BIN=$WT/build-pf1e/armBbin/hven_sqp_corpus
EXPECT=${2:?expected arm B sha256}
BASE=$WT/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
# Arm B is frozen by the runtime gate's run_all.sh; frozen here the same way if the census runs first.
if [ ! -f "$BIN" ]; then mkdir -p "$(dirname "$BIN")"; cp -p "$WT/build-pf1e/rel/bench/hven_sqp_corpus" "$BIN" || exit 1; fi
GOT=$(sha256sum "$BIN" | cut -d' ' -f1)
echo "binary $BIN sha256 $GOT"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
RUN=$WT/build-pf1e/census-run
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
# PF-1: the declared mover makes the comparator's own exit 1 by construction; the criterion is census_check.py
# against the declared movers (DECLARATION P5). The runner's merge must have produced 57 rows.
[ -f "$EV/walk_census.csv" ] || { echo "NO_MERGED_CENSUS"; exit 5; }
python3 "$WT/docs/notes/data/2026-11-m8-pf1/legs/census_check.py" "$BASE" "$EV/walk_census.csv" \
  "$WT/docs/notes/data/2026-11-m8-pf1/census-movers.csv" > "$EV/census_check.txt" 2>&1
crc=$?
cat "$EV/census_check.txt"
echo "census_check rc=$crc"
echo "FOREGROUND_END $(date -Is)"
exit $crc
