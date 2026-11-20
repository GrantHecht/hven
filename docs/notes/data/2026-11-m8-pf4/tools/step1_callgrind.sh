#!/usr/bin/env bash
# PF-4 step 1 (measurement, not a gate leg): callgrind on the PF-3 lane binary (hven 48e1eeae, hven-bench e03cbad) for the
# three declared cells. Run as: setsid -w flock /tmp/box-build.lock bash step1_callgrind.sh <outdir>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4
OUT=$(realpath -m "${1:?outdir}"); mkdir -p "$OUT"
RAW=$WT/.scratch/callgrind; mkdir -p "$RAW"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 TMPDIR=$WT/.scratch/tmp
cd /home/ghecht/Projects/hven-bench-pf3 || exit 1
BIN=build/lanes/hven/hven_bench_cell
echo "FOREGROUND_START $(date -Is)"; sha256sum "$BIN"
for P in ACOPP57 SMBANK DALLASM; do
  echo "RUN $P $(date -Is)"
  valgrind --tool=callgrind --callgrind-out-file="$RAW/hven-sqp__$P.cg" "$BIN" --lane hven-sqp --problem "$P" --options default \
    --budget-iters 0 --options-dump "$TMPDIR/opts-$P.txt" > "$RAW/hven-sqp__$P.stdout" 2> "$RAW/hven-sqp__$P.stderr"
  echo "rc=$?"
  grep -E '^(status|iterations|factorizations|side.symbolic_analyses|side.qp_minor_iters)=' "$RAW/hven-sqp__$P.stdout" | tr '\n' ' ' > "$OUT/hven-sqp__$P.counters"; echo >> "$OUT/hven-sqp__$P.counters"
  python3 "$WT/docs/notes/data/2026-11-m8-pf4/tools/pf4_callgrind.py" "$RAW/hven-sqp__$P.cg" > "$OUT/hven-sqp__$P.json"
done
echo "FOREGROUND_END $(date -Is)"
