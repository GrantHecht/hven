#!/usr/bin/env bash
# M8 DN-0 IPM r1 POST-HOC probe (informational): per-process call counts of MKL_Read_ThreadENV_Count_Cores,
# __kmp_serial_initialize and pardiso under gdb on hs071 (a TRAIN population cell, run AFTER all legs; no number
# from it enters any reading). Run under flock /tmp/box-build.lock. Output: results/init-audit.txt.
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "BUSY $p"; exit 9; }
echo "FOREGROUND_START gdbprobe2 $(date -u +%FT%TZ)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd /home/ghecht/Projects/hven-bench-sd
cat > /tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/g.cmd <<'G'
set debuginfod enabled off
set breakpoint pending on
set pagination off
break MKL_Read_ThreadENV_Count_Cores
commands 1
silent
printf "HIT_READENV\n"
bt 3
continue
end
break __kmp_serial_initialize
commands 2
silent
printf "HIT_KMPINIT\n"
continue
end
break pardiso
commands 3
silent
printf "HIT_PARDISO\n"
continue
end
run
G
gdb -batch -nx -x /tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/g.cmd --args .scratch/dn0r1/build-cfi/hven_bench_cell --lane hven-ipm --problem hs071 --options default --budget-iters 0 --options-dump /tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/x.optdump --assert-reference > /tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/g.out 2>&1
grep -c HIT_READENV /tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/g.out
grep -c HIT_KMPINIT /tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/g.out
grep -c HIT_PARDISO /tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/g.out
grep -A3 HIT_READENV /tmp/claude-1000/-home-ghecht-Projects-hven/0c17d1d8-660f-4914-9feb-e49a88005c27/scratchpad/g.out | grep "^#[12]" | sort | uniq -c | head
echo "FOREGROUND_END gdbprobe2 $(date -u +%FT%TZ)"
