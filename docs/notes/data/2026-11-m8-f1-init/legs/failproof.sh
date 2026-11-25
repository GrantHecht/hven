#!/usr/bin/env bash
# M8 F1 (DECLARATION P15): THE FALSIFIER. The code head's `git archive` export at build-f1/fp-src with the old warm-up
# restored by ONE mutation -- `(void)mkl_get_max_threads();` -> `(void)dsecnd();` in src/drivers/solver_init.cpp
# (fail closed unless exactly one line changes) -- built (Release, legs/build.sh's flags, hven_sqp_corpus only) at
# build-f1/fp. Then arm B (build-f1/armBbin) and the mutant each run the interior engine on ONE cell
# (f7_n1000_bound_neutral), phase-aligned, core 2, under perf record (instructions:u, -c 200000) + perf stat.
# Criterion: the instrument tells the two apart -- mutant spin samples (difftime, difftime@plt,
# mkl_serv_get_clocks_frequency, __vdso_time, time) > 0, arm B's == 0, mutant minus arm B perf-stat instructions:u
# >= 1.0e8 -- and the two rows are EXACT (scripts/compare_replay.py, 0 differences).
# usage: failproof.sh <code-head sha> <evidence-dir>   Run as: flock /tmp/box-build.lock bash failproof.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/f1
L=$WT/docs/notes/data/2026-11-m8-f1-init/legs
HEAD_SHA=${1:?code head sha}
EV=${2:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
rm -rf "$WT/build-f1/fp-src" "$WT/build-f1/fp"
mkdir -p "$WT/build-f1/fp-src"
git archive "$HEAD_SHA" | tar -x -C "$WT/build-f1/fp-src" || { echo ARCHIVE_FAIL; exit 1; }
rm -rf "$WT/build-f1/fp-src/dep/eigen" "$WT/build-f1/fp-src/dep/fmt"
cp -a dep/eigen "$WT/build-f1/fp-src/dep/eigen" && cp -a dep/fmt "$WT/build-f1/fp-src/dep/fmt" || exit 1
F=$WT/build-f1/fp-src/src/drivers/solver_init.cpp
n=$(grep -c '^        (void)mkl_get_max_threads();$' "$F")
[ "$n" = 1 ] || { echo "MUTATION_SITE_COUNT $n"; exit 1; }
sed -i 's/^        (void)mkl_get_max_threads();$/        (void)dsecnd();/' "$F"
diff <(git show "$HEAD_SHA:src/drivers/solver_init.cpp") "$F" | tee "$EV/failproof-mutation.diff"
[ "$(diff <(git show "$HEAD_SHA:src/drivers/solver_init.cpp") "$F" | grep -c '^[<>]')" = 2 ] || { echo MUTATION_NOT_ONE_LINE; exit 1; }
SRC=$WT/build-f1/fp-src bash "$L/build.sh" Release fp hven_sqp_corpus > "$EV/failproof-build.log" 2>&1 || { echo FP_BUILD_FAIL; tail -20 "$EV/failproof-build.log"; exit 1; }
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
align() { while :; do ns=$(date +%N); [ $((10#$ns)) -lt 5000000 ] && return 0; done; }
CELL=f7_n1000_bound_neutral
declare -A BIN=([B]=$WT/build-f1/armBbin/hven_sqp_corpus [M]=$WT/build-f1/fp/bench/hven_sqp_corpus)
sha256sum "${BIN[B]}" "${BIN[M]}"
for arm in B M; do
  align
  taskset -c 2 perf record -q -c 200000 -e instructions:u -o "$WT/build-f1/fp/$arm.data" -- \
    perf stat -x, -e instructions:u -o "$EV/stat-$arm.txt" -- \
    "${BIN[$arm]}" --engine interior --cells "$CELL" --csv "$EV/row-$arm.csv" > "$EV/run-$arm.txt" 2>&1
  echo "arm $arm rc=$?"
  perf report -i "$WT/build-f1/fp/$arm.data" --comm hven_sqp_corpus --sort symbol --stdio --no-children \
    -F overhead,sample,sym 2> /dev/null | grep -v '^#' | grep -v '^$' > "$EV/syms-$arm.txt"
  rm -f "$WT/build-f1/fp/$arm.data"
done
python3 scripts/compare_replay.py "$EV/row-B.csv" "$EV/row-M.csv" "failproof" > "$EV/compare.txt" 2>&1
crc=$?; tail -1 "$EV/compare.txt"
python3 - "$EV" "$crc" <<'PY' | tee "$EV/failproof-verdict.txt"
import re, sys
ev, crc = sys.argv[1], int(sys.argv[2])
SPIN = {'difftime', 'difftime@plt', 'mkl_serv_get_clocks_frequency', '__vdso_time', 'time'}
LINE = re.compile(r'\s*([\d.]+)%\s+(\d+)\s+\[(.)\]\s+(.*?)(?:\s+-\s+-)?\s*$')
def spin(p):
    n = 0
    for l in open(p):
        m = LINE.match(l)
        if m and m.group(4) in SPIN:
            n += int(m.group(2))
    return n
def ins(p):
    for l in open(p):
        x = l.strip().split(',')
        if len(x) >= 3 and x[2].startswith('instructions'):
            return float(x[0])
sb, sm = spin(ev + '/syms-B.txt'), spin(ev + '/syms-M.txt')
ib, im = ins(ev + '/stat-B.txt'), ins(ev + '/stat-M.txt')
print('arm B: spin samples %d, instructions:u %.0f' % (sb, ib))
print('mutant: spin samples %d, instructions:u %.0f' % (sm, im))
print('mutant - arm B = %.4e; rows EXACT: %s' % (im - ib, crc == 0))
ok = sm > 0 and sb == 0 and im - ib >= 1.0e8 and crc == 0
print('FAILPROOF_AS_DECLARED' if ok else 'FAILPROOF_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
PY
rc=${PIPESTATUS[0]}
echo "FOREGROUND_END $(date -Is)"
exit $rc
