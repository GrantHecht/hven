#!/usr/bin/env bash
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# DN-1b r2 (../DECLARATION-r2.txt): t_run.sh + the clone made clean for the runner: DN-1's untracked runs/dn1-train/ is moved
# into the git-ignored .scratch/dn1-train.aside before the run and moved back after it (nothing deleted); this run's own
# runs/dn1b-train/ is moved into .scratch/dn1b-train.done after its copy, so the clone is left as found.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/dn1legs/ and re-pointed (worktree dn1b, evidence this directory, the DN-1b lane binary build-dn1be/dn1bbin/hven_bench_cell); older provenance lines below are historical.
# M8 DN-1 T leg (../DECLARATION-dn1legs.txt): CUTEst TRAIN default, hven-sqp, on the local clone at commit A, PF-1's
# invocation verbatim (hven-bench runs/2026-11-m8-pf1-train/README.txt: --set-version 'cutest-nlp-35 v1' --split TRAIN
# --budget-wall-s 300 --budget-iters 0 --options default --lanes hven-sqp; MKL_NUM_THREADS=1; one cell at a time).
# The runner writes into the clone (runs/dn1-train/default); this script copies the results to EV and compares them
# with PF-1's (t_compare.py). HOLDOUT is never run.
# usage: t_run.sh <evidence-dir> <lane binary sha256> <code head sha>   Run as: flock /tmp/box-build.lock bash t_run.sh ...
# DN-1b: the frozen DN-1b lane binary (commit C, `default` label = kNever) via --binary; the clone at commit C.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1c
D=$WT/docs/notes/data/2026-11-m8-dn1c-sqp-check/sl
HB=/home/ghecht/Projects/hven-bench-dn1
EV=${1:?evidence dir}
WANT=${2:?lane binary sha256}
HEAD_SHA=${3:?code head sha}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
mkdir -p "$EV"
cd "$HB" || exit 1
git log --oneline -1
[ "$(git rev-parse HEAD)" = "$(git rev-parse dn1c-legs)" ] || { echo HEAD_NOT_COMMIT_C; exit 4; }
BIN=$WT/build-dn1ce/dn1cbin/hven_bench_cell
[ "$(sha256sum "$BIN" | cut -d' ' -f1)" = "$WANT" ] || { echo LANE_BINARY_SHA; exit 4; }
grep -q "^set(HVEN_PIN_SHA \"$HEAD_SHA\")$" cmake/hven_pin.cmake || { echo PIN; exit 4; }
export MKL_NUM_THREADS=1
[ -e .scratch/dn1-train.aside ] && { echo ASIDE_EXISTS; exit 4; }
if [ -d runs/dn1-train ]; then mv runs/dn1-train .scratch/dn1-train.aside || exit 1; fi
st=$(git status --porcelain); [ -z "$st" ] || { echo "CLONE_STILL_DIRTY $st"; [ -d .scratch/dn1-train.aside ] && mv .scratch/dn1-train.aside runs/dn1-train; exit 4; }
rm -rf runs/dn1c-train/default
python3 runners/run_serial.py --set-version 'cutest-nlp-35 v1' --split TRAIN --budget-wall-s 300 --budget-iters 0 \
  --options default --lanes hven-sqp --binary "$BIN" --out runs/dn1c-train/default > "$EV/run.log" 2>&1
rc=$?
echo "runner rc=$rc"
tail -5 "$EV/run.log"
cp -a runs/dn1c-train/default "$EV/"; crc=$?
mkdir -p .scratch && rm -rf .scratch/dn1c-train.done && mv runs/dn1c-train .scratch/dn1c-train.done
[ -d .scratch/dn1-train.aside ] && mv .scratch/dn1-train.aside runs/dn1-train
git status --porcelain
[ $crc = 0 ] || exit 1
cp -p ".hven/install-${HEAD_SHA:0:12}/HVEN_PIN.txt" "$EV/HVEN_PIN.txt"
python3 "$D/t_compare.py" /home/ghecht/Projects/hven-bench/runs/2026-11-m8-pf1-train/default "$EV/default" | tee "$EV/comparison.txt"
echo "FOREGROUND_END $(date -Is)"
