#!/usr/bin/env bash
# M8 DN-0 IPM r1: ONE cell process, the runner's hven-lane argv (runners/run_serial.py run_cell) unchanged:
#   hven_bench_cell --lane L --problem P --options default --budget-iters 0
#                   --options-dump F.optdump --assert-reference
# cwd = harness root; MKL_NUM_THREADS=1 OMP_NUM_THREADS=1. The lane process runs alone on core CORE (2).
# The watchdog is this script's own: at CAP seconds it SIGKILLs ONLY the lane process it started.
# Never run under `timeout`.
#
# Usage: cell.sh MODE HARNESS_ROOT LANE PROBLEM OUTPREFIX CAP_S [PERIOD]
#   MODE stat     MEASURED binary (build/):  taskset -c CORE perf stat -x, -e instructions:u,cycles:u,branches:u
#   MODE statrec  RECORD binary (CFI):       the same perf stat (counter-identity round SC)
#   MODE record   RECORD binary (CFI):       taskset -c PERFCORE perf record -q -m 1M -e instructions:u -c PERIOD
#                 --call-graph dwarf,16384 --user-callchains -- taskset -c CORE <lane>
#                 (perf's reader on PERFCORE (4), the lane process alone on CORE: no LOST; DECLARATION-r1 §3)
#   MODE dump     the DUMP build (pardiso wrap), HVEN_DN0_DUMP_DIR=OUTPREFIX.dump
set -uo pipefail
MODE="$1"; HB="$2"; LANE="$3"; P="$4"; F="$5"; CAP="$6"; PERIOD="${7:-}"
CORE="${DN0_CORE:-2}"
PERFCORE="${DN0_PERFCORE:-4}"
MEAS="${HB}/build/lanes/hven/hven_bench_cell"
REC="${HB}/.scratch/dn0r1/build-cfi/hven_bench_cell"
DUMPBIN="${HB}/.scratch/dn0r1/build-dump/lanes/hven/hven_bench_cell"
ARGV=(--lane "${LANE}" --problem "${P}" --options default --budget-iters 0
      --options-dump "${F}.optdump" --assert-reference)
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
cd "${HB}" || exit 2
case "${MODE}" in
stat|statrec)
    BIN="${MEAS}"; [[ "${MODE}" == statrec ]] && BIN="${REC}"
    taskset -c "${CORE}" perf stat -x, -e instructions:u,cycles:u,branches:u -o "${F}.stat" -- \
        "${BIN}" "${ARGV[@]}" > "${F}.out" 2> "${F}.err" &
    ;;
record)
    [[ -n "${PERIOD}" ]] || { echo "record needs PERIOD" >&2; exit 2; }
    taskset -c "${PERFCORE}" perf record -q -m 1M -e instructions:u -c "${PERIOD}" \
        --call-graph dwarf,16384 --user-callchains -o "${F}.perf.data" -- \
        taskset -c "${CORE}" "${REC}" "${ARGV[@]}" > "${F}.out" 2> "${F}.err" &
    ;;
dump)
    mkdir -p "${F}.dump"
    HVEN_DN0_DUMP_DIR="${F}.dump" HVEN_DN0_DUMP_MAX=50 taskset -c "${CORE}" \
        "${DUMPBIN}" "${ARGV[@]}" > "${F}.out" 2> "${F}.err" &
    ;;
*) echo "unknown mode ${MODE}" >&2; exit 2 ;;
esac
wrapper=$!
start=$(date +%s.%N)
lane_pid() {
    if [[ "${MODE}" == dump ]]; then echo "${wrapper}"; else pgrep -P "${wrapper}" -x hven_bench_cell | head -1; fi
}
capped=0
while kill -0 "${wrapper}" 2>/dev/null; do
    now=$(date +%s.%N)
    if awk "BEGIN{exit !(${now}-${start} >= ${CAP})}"; then
        lp="$(lane_pid)"
        [[ -n "${lp}" ]] && kill -KILL "${lp}"
        capped=1
        break
    fi
    sleep 0.02
done
wait "${wrapper}"; rc=$?
end=$(date +%s.%N)
wall=$(awk "BEGIN{printf \"%.3f\", ${end}-${start}}")
if [[ ${capped} -eq 1 ]]; then
    echo "CELL_RESULT ${MODE} ${LANE} ${P} CAPPED rc=${rc} wall=${wall}" | tee "${F}.result"
else
    echo "CELL_RESULT ${MODE} ${LANE} ${P} rc=${rc} wall=${wall}" | tee "${F}.result"
fi
