#!/usr/bin/env bash
# B2-runtime-r2 EXPLANATORY record (changes no verdict): one `perf record -g` of the
# interior leg in arm B, to name what triggers the wall-clock spin symbols and how
# often (once per process, or once per Pardiso session). Run by run_all.sh first.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-aba3deeb97313f444
OUT=${1:?out dir}
cd "$WT" || exit 1
mkdir -p "$OUT" "$WT/build-b2/r2-perf"
BIN=$WT/build-b2/armB-r2/hven_sqp_corpus
DATA=$WT/build-b2/r2-perf/spin-explain.data
CELLS=$(sed -n 3p docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt)
SPIN='difftime,difftime@plt,mkl_serv_get_clocks_frequency,__vdso_time,__vdso_clock_gettime,time,clock_gettime'
echo "SPIN_START $(date -Is)"
env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    perf record -q -c 2000000 -e instructions:u --call-graph dwarf,8192 --user-callchains -o "$DATA" -- \
    "$BIN" --engine interior --cells "$CELLS" --csv "$OUT/spin-explain-interior.csv" > "$OUT/spin-explain.stdout" 2>&1
echo "record rc=$?"
# Callers of each spin symbol: the full caller-ordered report, then each spin symbol's
# block cut out of it (a block runs from the symbol's top line to the next top line).
perf report -i "$DATA" --comm hven_sqp_corpus --stdio --no-children --sort sym \
    -g caller,0.5,callee,function,percent 2> /dev/null > "$WT/build-b2/r2-perf/spin-report.txt"
python3 - "$SPIN" "$WT/build-b2/r2-perf/spin-report.txt" > "$OUT/spin-callers.txt" <<'PY'
import re, sys
spin = set(sys.argv[1].split(','))
top = re.compile(r'^\s+([\d.]+)%\s+\[.\]\s+(.*\S)\s*$')
keep = False
for line in open(sys.argv[2]):
    m = top.match(line)
    if m:
        keep = m.group(2) in spin
    if keep and line.strip():
        print(line.rstrip())
PY
# The spin samples' timestamps, clustered into bursts (gap > 50 ms opens a new burst).
perf script -i "$DATA" --comm hven_sqp_corpus -F time,ip,sym -G 2> /dev/null \
    | awk -v spin="$SPIN" 'BEGIN{n=split(spin,s,","); for(i=1;i<=n;i++) S[s[i]]=1}
        /^ *[0-9]+\.[0-9]+:/ {t=$1; sub(":","",t); sym=$3; if (sym in S) print t, sym}' > "$OUT/spin-samples.txt"
awk 'BEGIN{b=0; last=-1} {t=$1+0; if (last<0 || t-last>0.05) {b++; start[b]=t; cnt[b]=0} cnt[b]++; last=t; end[b]=t}
     END{print "bursts:", b; for(i=1;i<=b;i++) printf("burst %d start=%.6f end=%.6f spin_samples=%d\n", i, start[i], end[i], cnt[i])}' \
    "$OUT/spin-samples.txt" > "$OUT/spin-bursts.txt"
# The per-session reading's denominator: each CSV data row is one solve on a fresh
# solver, hence at least one Pardiso session.
grep -vc '^cell_id\|^#' "$OUT/spin-explain-interior.csv" > "$OUT/solve-rows.txt"
rm -f "$DATA" "$WT/build-b2/r2-perf/spin-report.txt"
echo "SPIN_END $(date -Is)"
