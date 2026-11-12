#!/usr/bin/env bash
# M8 DN-3a (../DECLARATION.txt, FA): the forced-arm leg. Serial, one process at a time, THREADS=1 (MKL_NUM_THREADS=1,
# core 2), CAP 300 s (cell.sh, DN-1d's runner); then fa_analyze.py -> EV/fa-analysis.txt.
#   FA1  DN-0's 238 cells (../../2026-11-m8-dn0/raw/roster.tsv), per cell `default` (kNever) then `dense-default`
#        (kAlways), the head lane binary  -> EV/fa1/cells
#   FA2  the 119 hven-sqp cells, labels sqp-ssn, dense-sqp-ssn, sqp-ipm, dense-sqp-ipm, the head lane binary
#        -> EV/fa2/head; labels dense-sqp-ssn, dense-sqp-ipm, the BASE lane binary (b70e45cfb) -> EV/fa2/base
# usage: fa_run.sh <evidence-dir> <head lane sha256> <base lane sha256>
# Run as: flock /tmp/box-build.lock bash fa_run.sh ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-acfc8f7d568381148
D=$WT/docs/notes/data/2026-11-m8-dn3a/sl
EV=${1:?evidence dir}
WANT_H=${2:?head lane sha256}
WANT_B=${3:?base lane sha256}
HEADBIN=$WT/build-dn3a/fabin/hven_bench_cell
BASEBIN=$WT/build-dn3a/fabasebin/hven_bench_cell
ROSTER=$WT/docs/notes/data/2026-11-m8-dn0/raw/roster.tsv
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "FOREGROUND_START $(date -Is)"
[[ "$(sha256sum "$HEADBIN" | cut -d' ' -f1)" == "$WANT_H" ]] || { echo HEAD_LANE_SHA_MISMATCH; exit 4; }
[[ "$(sha256sum "$BASEBIN" | cut -d' ' -f1)" == "$WANT_B" ]] || { echo BASE_LANE_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
unset HVEN_DN3A_PROBE
mkdir -p "$EV/fa1/cells" "$EV/fa2/head" "$EV/fa2/base"
cp -p "$ROSTER" "$EV/roster.tsv"
run() { # <bin> <dir> <lane> <prob> <tag> <label>
  [[ -f "$2/$5.$6.result" ]] && return
  CELL_BIN=$1 bash "$D/cell.sh" run "$3" "$4" "$6" 1 "$2/$5.$6" 300 > /dev/null
}
while IFS=$'\t' read -r -u 3 pop lane prob; do
  tag="${pop}-${lane}-${prob}"
  for label in default dense-default; do run "$HEADBIN" "$EV/fa1/cells" "$lane" "$prob" "$tag" "$label"; done
done 3< "$ROSTER"
echo "FA1 done $(date -Is): $(ls "$EV"/fa1/cells/*.result | wc -l) results"
while IFS=$'\t' read -r -u 3 pop lane prob; do
  [[ "$lane" == hven-sqp ]] || continue
  tag="${pop}-${lane}-${prob}"
  for label in sqp-ssn dense-sqp-ssn sqp-ipm dense-sqp-ipm; do run "$HEADBIN" "$EV/fa2/head" "$lane" "$prob" "$tag" "$label"; done
  for label in dense-sqp-ssn dense-sqp-ipm; do run "$BASEBIN" "$EV/fa2/base" "$lane" "$prob" "$tag" "$label"; done
done 3< "$ROSTER"
echo "FA2 done $(date -Is): head $(ls "$EV"/fa2/head/*.result | wc -l), base $(ls "$EV"/fa2/base/*.result | wc -l) results"
python3 "$D/fa_analyze.py" "$EV" > "$EV/fa-analysis.txt" 2>&1
echo "analysis rc=$?"
sed -n '/== VERDICTS/,$p' "$EV/fa-analysis.txt"
echo "FOREGROUND_END $(date -Is)"
