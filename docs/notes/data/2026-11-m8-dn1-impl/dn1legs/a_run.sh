#!/usr/bin/env bash
# M8 DN-1 A legs (../DECLARATION-dn1legs.txt): the replay over DN-0's 5501 dumps, twice --
#   full  at MKL_NUM_THREADS=1 (A1-A7)       -> EV/replay-full-mkl1.csv
#   dense at the default thread setting, MKL_NUM_THREADS and OMP_NUM_THREADS UNSET (A-thr) -> EV/replay-dense-default.csv
# then a_analyze.py -> EV/a-analysis.txt. Serial, alone; the full run pinned to core 2 (DN-0's core), the
# default-thread run unpinned (MKL's own threads).
# usage: a_run.sh <evidence-dir> <expected dn1_replay sha256>   Run as: flock /tmp/box-build.lock bash a_run.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
D=$WT/docs/notes/data/2026-11-m8-dn1-impl/dn1legs
EV=${1:?evidence dir}
WANT=${2:?tool sha256}
DUMP=/home/ghecht/Projects/hven-bench-sd/.scratch/dn0/run/dump
MATS=$WT/docs/notes/data/2026-11-m8-dn1-design/matrices.csv.gz
BIN=$WT/build-dn1r2/dn1bin/dn1_replay
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
mkdir -p "$EV"
got=$(sha256sum "$BIN" | cut -d' ' -f1)
[ "$got" = "$WANT" ] || { echo "TOOL_SHA_MISMATCH $got"; exit 4; }
python3 "$D/a_list.py" "$MATS" "$DUMP" "$EV/list.txt" || exit 3
sha256sum "$MATS" "$EV/list.txt"
( export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1; taskset -c 2 "$BIN" full "$EV/replay-full-mkl1.csv" "$EV/list.txt" 2> "$EV/replay-full-mkl1.err" )
echo "full rc=$? $(date -Is)"
( unset MKL_NUM_THREADS OMP_NUM_THREADS; "$BIN" dense "$EV/replay-dense-default.csv" "$EV/list.txt" 2> "$EV/replay-dense-default.err" )
echo "dense-default rc=$? $(date -Is)"
wc -l "$EV/replay-full-mkl1.csv" "$EV/replay-dense-default.csv"
python3 "$D/a_analyze.py" "$MATS" "$EV/replay-full-mkl1.csv" "$EV/replay-dense-default.csv" > "$EV/a-analysis.txt" 2>&1
echo "analysis rc=$?"
sed -n '/== VERDICTS/,$p' "$EV/a-analysis.txt"
gzip -kf "$EV/replay-full-mkl1.csv" "$EV/replay-dense-default.csv"
echo "FOREGROUND_END $(date -Is)"
