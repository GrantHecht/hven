#!/usr/bin/env bash
# M8 DN-1d r2 (../../DECLARATION-r2.txt): r1's copy of this file re-pointed (r2/, build-dn1de2/); older provenance lines below are historical.
# M8 DN-1d (../DECLARATION.txt): copied from ../../2026-11-m8-dn1c-sqp-check/ and re-pointed (worktree dn1d, evidence this directory, arm A and arm B trees build-dn1de/, arm A 4776aa2ff rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1 r2 (../../DECLARATION-r2.txt): copied from ../../legs/ and re-pointed (evidence r2/, arm B trees build-dn1r2/; arm A read-only from build-dn1e/).
# M8 DN-1 impl: copied from R11 (2026-11-m8-r11-block-decode) and re-pointed: worktree dn1, build-dn1e, this directory, arm A 02065dac.
# M8 R11: copied from F1 (2026-11-m8-f1-init; runtime from its runtime-r2, period 400000; census legs from PF-1) and re-pointed: worktree r11, build-r11e, this directory, arm A a8c540cf.
# M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1 (PF-HB's instrument, re-pointed) (the B8 riders' instrument, re-pointed): the 27-cell replay x walk/ssn/ipm on the default (internal) route,
# compared EXACT against docs/notes/data/2026-10-m7-wx2-gates/gates/replay/head-*.csv.
# usage: replay.sh <out-dir>      Run as: flock /tmp/box-build.lock bash replay.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1d
OUT=${1:?out dir}
BIN=$WT/build-dn1de2/rel/bench/hven_sqp_corpus
REF=$WT/docs/notes/data/2026-10-m7-wx2-gates/gates/replay
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd "$WT" || exit 1
mkdir -p "$OUT"
sha256sum "$BIN"
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
worst=0
for MODE in walk ssn ipm; do
  echo "RUN mode=$MODE $(date -Is)"
  "$BIN" --engine "$MODE" --cells "$CELLS" --csv "$OUT/dn1-$MODE.csv" > "$OUT/dn1-run-$MODE.txt" 2>&1
  rc=$?; echo "run rc=$rc"; [ $rc -gt $worst ] && worst=$rc
  python3 scripts/compare_replay.py "$REF/head-$MODE.csv" "$OUT/dn1-$MODE.csv" "dn1-$MODE" > "$OUT/dn1-comparison-$MODE.txt" 2>&1
  rc=$?; echo "compare rc=$rc"; tail -3 "$OUT/dn1-comparison-$MODE.txt"; [ $rc -gt $worst ] && worst=$rc
done
echo "worst=$worst"
echo "FOREGROUND_END $(date -Is)"
exit $worst
