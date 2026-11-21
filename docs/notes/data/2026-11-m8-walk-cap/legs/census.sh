#!/usr/bin/env bash
# M8 walk cap (../DECLARATION.md, I2): ../../2026-11-m8-dn3/r4/legs/census.sh re-pointed in code lines (this worktree,
# the frozen corpus binary build-wc/bin/hven_sqp_corpus) and in its movers file: the 57-cell walk census,
# parallel-tiered v1 at the script's defaults (the runner's own declared co-run protocol: T1 6-wide pinned one process
# per physical core, T2 solo, T3 5-wide full budget), against the PROMOTED baseline
# bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv; the criterion is census_walkcap_check.py (DN-1d r3's
# census_check.py reading and deadline rule, PF-1's two declared movers as reference rows, and every other difference a
# reported walk-cap mover that must not be WORSE). Counters asserted; wall-clock informational only.
# usage: census.sh <evidence-dir> <corpus binary sha256> <PF-1 movers.csv>   Run as: flock /tmp/box-build.lock bash census.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
EV=${1:?evidence dir}
EXPECT=${2:?corpus binary sha256}
MOVERS=${3:?movers csv}
BIN=$WT/build-wc/bin/hven_sqp_corpus
BASE=$WT/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-wc/tmp
GOT=$(sha256sum "$BIN" | cut -d' ' -f1)
echo "binary $BIN sha256 $GOT"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
sha256sum "$MOVERS"
RUN=$WT/build-wc/census-run
rm -rf "$RUN"
bash scripts/run_walk_census.sh --binary "$BIN" --out "$RUN" --baseline "$BASE" \
  --compare scripts/census_compare.py > "$EV/census-run.log" 2>&1
rc=$?
echo "census rc=$rc"
tail -6 "$EV/census-run.log"
for f in walk_census.csv compare.txt serial_confirm_list.txt; do
  [ -f "$RUN/$f" ] && cp "$RUN/$f" "$EV/$f"
done
[ -f "$EV/walk_census.csv" ] || { echo "NO_MERGED_CENSUS"; exit 5; }
python3 "$WT/docs/notes/data/2026-11-m8-walk-cap/legs/census_walkcap_check.py" "$BASE" "$EV/walk_census.csv" \
  "$MOVERS" > "$EV/census_check.txt" 2>&1
crc=$?
cat "$EV/census_check.txt"
echo "census_check rc=$crc"
echo "FOREGROUND_END $(date -Is)"
exit $crc
