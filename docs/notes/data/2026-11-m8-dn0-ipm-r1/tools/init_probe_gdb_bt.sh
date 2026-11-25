#!/usr/bin/env bash
# M8 DN-0 IPM r1 POST-HOC probe (informational): call path of the OpenMP serial init (SMBANK, a non-population
# cutest-nlp-35 TRAIN cell, from the main harness). Run under flock /tmp/box-build.lock. Output: results/init-audit.txt.
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "BUSY $p"; exit 9; }
echo "FOREGROUND_START gdbprobe $(date -u +%FT%TZ)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd /home/ghecht/Projects/hven-bench
gdb -batch -nx -ex 'set debuginfod enabled off' -ex 'set breakpoint pending on' -ex 'break __kmp_serial_initialize' -ex run -ex 'bt 25' -ex kill \
  --args /home/ghecht/Projects/hven-bench-sd/.scratch/dn0r1/build-cfi/hven_bench_cell --lane hven-ipm --problem SMBANK --options default --budget-iters 0 --options-dump /tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/x.optdump --assert-reference 2>&1 | grep -E "^#|Breakpoint" | head -30
echo "FOREGROUND_END gdbprobe $(date -u +%FT%TZ)"
