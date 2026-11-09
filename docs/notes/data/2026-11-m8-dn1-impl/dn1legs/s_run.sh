#!/usr/bin/env bash
# M8 DN-1 S legs (../DECLARATION-dn1legs.txt): DN-0's 238 cells (../../2026-11-m8-dn0/raw/roster.tsv), each run
# twice on the frozen S binary -- label `default` (kNever) then `dense-default` (kAlways) -- serial, one process at a
# time, THREADS=1 (MKL_NUM_THREADS=1, core 2), CAP 300 s, then s_analyze.py -> EV/s-analysis.txt.
# usage: s_run.sh <evidence-dir> <expected S binary sha256>   Run as: flock /tmp/box-build.lock bash s_run.sh ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
D=$WT/docs/notes/data/2026-11-m8-dn1-impl/dn1legs
EV=${1:?evidence dir}
WANT=${2:?S binary sha256}
ROSTER=$WT/docs/notes/data/2026-11-m8-dn0/raw/roster.tsv
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "FOREGROUND_START $(date -Is)"
got=$(sha256sum "$WT/build-dn1r2/dn1bin/hven_bench_cell.S" | cut -d' ' -f1)
[[ "$got" == "$WANT" ]] || { echo "S_BINARY_SHA_MISMATCH $got"; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
mkdir -p "$EV/cells"
cp -p "$ROSTER" "$EV/roster.tsv"
while IFS=$'\t' read -r -u 3 pop lane prob; do
    tag="${pop}-${lane}-${prob}"
    for label in default dense-default; do
        [[ -f "$EV/cells/${tag}.${label}.result" ]] && continue
        bash "$D/dn1_cell.sh" run "$lane" "$prob" "$label" 1 "$EV/cells/${tag}.${label}" 300 > /dev/null
    done
done 3< "$ROSTER"
echo "cells done $(date -Is): $(ls "$EV"/cells/*.result | wc -l) results"
python3 "$D/s_analyze.py" "$EV/roster.tsv" "$EV/cells" > "$EV/s-analysis.txt" 2>&1
echo "analysis rc=$?"
sed -n '/== VERDICTS/,$p' "$EV/s-analysis.txt"
echo "FOREGROUND_END $(date -Is)"
