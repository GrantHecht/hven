#!/usr/bin/env bash
# M8 DN-1 T leg (../DECLARATION-dn1legs.txt): CUTEst TRAIN default, hven-sqp, on the local clone at commit A, PF-1's
# invocation verbatim (hven-bench runs/2026-11-m8-pf1-train/README.txt: --set-version 'cutest-nlp-35 v1' --split TRAIN
# --budget-wall-s 300 --budget-iters 0 --options default --lanes hven-sqp; MKL_NUM_THREADS=1; one cell at a time).
# The runner writes into the clone (runs/dn1-train/default); this script copies the results to EV and compares them
# with PF-1's (t_compare.py). HOLDOUT is never run.
# usage: t_run.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash t_run.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
D=$WT/docs/notes/data/2026-11-m8-dn1-impl/dn1legs
HB=/home/ghecht/Projects/hven-bench-dn1
EV=${1:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
mkdir -p "$EV"
cd "$HB" || exit 1
git log --oneline -1
[ "$(git rev-parse HEAD)" = "$(git rev-parse dn1-legs~1)" ] || { echo HEAD_NOT_COMMIT_A; exit 4; }
cmp -s build/lanes/hven/hven_bench_cell "$WT/build-dn1r2/dn1bin/hven_bench_cell.T" || { echo T_BINARY_NOT_FROZEN; exit 4; }
grep -q '^set(HVEN_PIN_SHA "2171f8dd4233b1bc21682fc250060294e7d42a3d")$' cmake/hven_pin.cmake || { echo PIN; exit 4; }
export MKL_NUM_THREADS=1
python3 runners/run_serial.py --set-version 'cutest-nlp-35 v1' --split TRAIN --budget-wall-s 300 --budget-iters 0 \
  --options default --lanes hven-sqp --out runs/dn1-train/default > "$EV/run.log" 2>&1
rc=$?
echo "runner rc=$rc"
tail -5 "$EV/run.log"
cp -a runs/dn1-train/default "$EV/" || exit 1
cp -p ".hven/install-2171f8dd4233/HVEN_PIN.txt" "$EV/HVEN_PIN.txt"
python3 "$D/t_compare.py" /home/ghecht/Projects/hven-bench/runs/2026-11-m8-pf1-train/default "$EV/default" | tee "$EV/comparison.txt"
echo "FOREGROUND_END $(date -Is)"
