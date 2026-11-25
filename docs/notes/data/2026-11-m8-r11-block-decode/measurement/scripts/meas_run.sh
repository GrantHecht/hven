#!/usr/bin/env bash
# R11 scratch measurement: probe binary on the 27-cell replay x walk/ssn/ipm (one process per cell) and the 57-cell walk census.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/r11
B=$WT/build-r11
BIN=$B/meas/bench/hven_sqp_corpus
O=$B/meas-out
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo PGREP_BUSY; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
rm -rf "$O"; mkdir -p "$O/replay"
sha256sum "$BIN"
cd "$WT" || exit 1
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
for MODE in walk ssn ipm; do
  for c in ${CELLS//,/ }; do
    R11_PROBE_OUT=$O/replay-$MODE.probe "$BIN" --engine "$MODE" --cells "$c" --csv "$O/replay/$MODE-$c.csv" > "$O/replay/$MODE-$c.log" 2>&1
    echo "replay $MODE $c rc=$?"
  done
done
for MODE in walk ssn ipm; do
  python3 - "$O/replay" "$MODE" > "$O/replay-$MODE.csv" <<'PY'
import sys,glob,os
d,m=sys.argv[1:3]; hdr=None; out=[]
for f in sorted(glob.glob(os.path.join(d,m+'-*.csv'))):
    rows=[l for l in open(f) if not l.startswith('#') and l.strip()]
    if hdr is None: hdr=rows[0]; out.append(hdr)
    out+=rows[1:]
sys.stdout.write(''.join(out))
PY
  python3 scripts/compare_replay.py docs/notes/data/2026-10-m7-wx2-gates/gates/replay/head-$MODE.csv "$O/replay-$MODE.csv" "meas-$MODE" > "$O/replay-compare-$MODE.txt" 2>&1
  echo "compare $MODE rc=$?"; tail -2 "$O/replay-compare-$MODE.txt"
done
R11_PROBE_OUT=$O/census.probe bash scripts/run_walk_census.sh --binary "$BIN" --out "$O/census-run" \
  --baseline bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv --compare scripts/census_compare.py > "$O/census-run.log" 2>&1
echo "census rc=$?"; tail -8 "$O/census-run.log"
echo "FOREGROUND_END $(date -Is)"
