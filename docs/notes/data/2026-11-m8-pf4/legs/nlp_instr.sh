#!/usr/bin/env bash
# PF-4 P4b: the PF-3 profile population's cutest-nlp-35 hven-sqp `full` cells (15: PF-3's cells.csv; its 8 deadline-class
# sqp cells are excluded, as PF-3 asserts no instruction total on them), arm A then arm B per cell, serial, one process at
# a time, THREADS=1 (core 2), CAP 300 s, through cell.sh MODE stat (label `default`). Instruction counts asserted only as
# the declaration says; rows compared A vs B by nlp_check.py.
# usage: nlp_instr.sh <evidence-dir> <lane A> <sha A> <lane B> <sha B>   Run under: setsid -w flock /tmp/box-build.lock bash ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4
L=$WT/docs/notes/data/2026-11-m8-pf4/legs
EV=${1:?}; BA=${2:?}; SA=${3:?}; BB=${4:?}; SB=${5:?}
CELLS="STEENBRC STEENBRG YORKNET STEENBRB STEENBRE STEENBRD BIGBANK ACOPP118 ACOPR57 DALLASM READING6 SAWPATH ZAMB2-8 SMBANK ACOPP57"
export TMPDIR=$WT/.scratch/tmp
p=$(pgrep -x hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "FOREGROUND_START $(date -Is)"
[[ "$(sha256sum "$BA" | cut -d' ' -f1)" == "$SA" ]] || { echo LANE_A_SHA_MISMATCH; exit 4; }
[[ "$(sha256sum "$BB" | cut -d' ' -f1)" == "$SB" ]] || { echo LANE_B_SHA_MISMATCH; exit 4; }
EV=$(realpath -m "$EV"); mkdir -p "$EV/A" "$EV/B"
for P in $CELLS; do
  for arm in A B; do
    bin=$BA; [[ $arm == B ]] && bin=$BB
    CELL_BIN=$bin bash "$L/cell.sh" stat hven-sqp "$P" default 1 "$EV/$arm/nlp-hven-sqp-$P.D" 300
  done
done
echo "FOREGROUND_END $(date -Is)"
