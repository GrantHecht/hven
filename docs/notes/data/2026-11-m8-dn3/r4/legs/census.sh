#!/usr/bin/env bash
# M8 DN-3 r4 (../../DECLARATION-r4.md, I2): DN-1d r3's legs/census.sh (../../../2026-11-m8-dn1d-singular/r3/legs/census.sh)
# re-pointed in code lines only: worktree agent-a739270f237b3e30c, the flip tree's frozen corpus binary
# build-flip/flipbin/hven_sqp_corpus. The 57-cell walk census, parallel-tiered v1 at the script's defaults (the
# runner's own declared co-run protocol: T1 6-wide pinned, T2 solo, T3 5-wide full budget), against the PROMOTED
# baseline bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv; the criterion is DN-1d r3's census_check.py with
# PF-1's two declared movers (docs/notes/data/2026-11-m8-dn1-impl/census-movers.csv), unchanged.
# usage: census.sh <evidence-dir> <corpus binary sha256>   Run as: flock /tmp/box-build.lock bash census.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a739270f237b3e30c
EV=${1:?evidence dir}
EXPECT=${2:?corpus binary sha256}
BIN=$WT/build-flip/flipbin/hven_sqp_corpus
BASE=$WT/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
export MKL_NUM_THREADS=1 TMPDIR=$WT/build-flip/tmp
GOT=$(sha256sum "$BIN" | cut -d' ' -f1)
echo "binary $BIN sha256 $GOT"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
RUN=$WT/build-flip/census-run
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
python3 "$WT/docs/notes/data/2026-11-m8-dn1d-singular/r3/legs/census_check.py" "$BASE" "$EV/walk_census.csv" \
  "$WT/docs/notes/data/2026-11-m8-dn1-impl/census-movers.csv" > "$EV/census_check.txt" 2>&1
crc=$?
cat "$EV/census_check.txt"
echo "census_check rc=$crc"
echo "FOREGROUND_END $(date -Is)"
exit $crc
