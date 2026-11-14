#!/usr/bin/env bash
# AP-2 B3 proof 6: the 57-cell walk census (DECLARATION.txt proof 6, X2), parallel-tiered v1
# at the script's defaults, on B3's corpus binary (the runtime arm B copy, sha256 checked),
# against bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv, compared by
# scripts/census_compare.py. Then the declared re-derivation: the baseline with exactly the
# two X2 rows' status changed MaxIter -> WorkLimit, compared against the same fresh rows.
# usage: census.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash census.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a3b067b098f5e9221
EV=${1:?evidence dir}
BIN=$WT/build-b3/armB/hven_sqp_corpus
EXPECT=3810b64557dd14b2f70925243f61709b5389c17582043a483f3c2d9507c55dab
BASE=$WT/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
GOT=$(sha256sum "$BIN" | cut -d' ' -f1)
echo "binary $BIN sha256 $GOT"
[ "$GOT" = "$EXPECT" ] || { echo "BINARY_SHA_MISMATCH"; exit 4; }
RUN=$WT/build-b3/census-run
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
# The declared re-derivation: the two X2 rows' status column only.
python3 - "$BASE" "$EV/walk_baseline_rederived.csv" <<'PY'
import sys
src, dst = sys.argv[1], sys.argv[2]
cells = {'f7_n2000_path_neutral', 'f7_n2000_path_corrupted'}
out, n = [], 0
for line in open(src):
    if not line.startswith('#'):
        f = line.rstrip('\n').split(',')
        if f[0] in cells and f[6] == 'MaxIter':
            f[6] = 'WorkLimit'
            n += 1
            line = ','.join(f) + '\n'
    out.append(line)
open(dst, 'w').writelines(out)
print('re-derived rows:', n)
PY
FRESH=$(ls "$RUN"/*.csv 2>/dev/null | grep -v baseline | head -1)
[ -f "$RUN/walk_census.csv" ] && FRESH=$RUN/walk_census.csv
echo "fresh csv: $FRESH"
python3 scripts/census_compare.py "$EV/walk_baseline_rederived.csv" "$FRESH" > "$EV/compare-rederived.txt" 2>&1
echo "compare vs re-derived rc=$?"
tail -3 "$EV/compare-rederived.txt"
echo "FOREGROUND_END $(date -Is)"
exit $rc
