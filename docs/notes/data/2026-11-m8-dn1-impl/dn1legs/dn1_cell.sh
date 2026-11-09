#!/usr/bin/env bash
# M8 DN-1 S and M legs (../DECLARATION-dn1legs.txt): ONE cell process of the frozen S binary
# (build-dn1r2/dn1bin/hven_bench_cell.S), DN-0's argv (tools/cell.sh) with the label as given:
#   hven_bench_cell --lane L --problem P --options LABEL --budget-iters 0 --options-dump F.optdump --assert-reference
# cwd = the local clone (problem sources are relative to it). THREADS=1: MKL_NUM_THREADS=1 OMP_NUM_THREADS=1, pinned
# to core 2 (taskset); THREADS=default: both UNSET and unpinned (MKL's own thread count). The watchdog is this
# script's own (DN-0's): at CAP seconds it SIGKILLs ONLY the lane process it started. Never run under `timeout`.
# usage: dn1_cell.sh MODE LANE PROBLEM LABEL THREADS OUTPREFIX CAP [PERIOD]
#   MODE run     the cell alone                                              -> F.out F.err F.result
#   MODE stat    perf stat -x, -e instructions:u,cycles:u                     -> + F.stat
#   MODE record  perf record -e instructions:u -c PERIOD --call-graph dwarf,16384 --user-callchains -m 1M -> F.perf.data
set -uo pipefail
MODE="$1"; LANE="$2"; P="$3"; LABEL="$4"; THREADS="$5"; F="$6"; CAP="$7"; PERIOD="${8:-}"
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
HB=/home/ghecht/Projects/hven-bench-dn1
BIN=$WT/build-dn1r2/dn1bin/hven_bench_cell.S
ARGV=(--lane "${LANE}" --problem "${P}" --options "${LABEL}" --budget-iters 0
      --options-dump "${F}.optdump" --assert-reference)
case "${THREADS}" in
1) export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1; PIN=(taskset -c 2) ;;
default) unset MKL_NUM_THREADS OMP_NUM_THREADS; PIN=() ;;
*) echo "THREADS must be 1 or default" >&2; exit 2 ;;
esac
cd "${HB}" || exit 2
case "${MODE}" in
run)
    "${PIN[@]}" "${BIN}" "${ARGV[@]}" > "${F}.out" 2> "${F}.err" &
    ;;
stat)
    "${PIN[@]}" perf stat -x, -e instructions:u,cycles:u -o "${F}.stat" -- \
        "${BIN}" "${ARGV[@]}" > "${F}.out" 2> "${F}.err" &
    ;;
record)
    [[ -n "${PERIOD}" && "${PERIOD}" -ge 400000 ]] || { echo "record needs PERIOD >= 400000" >&2; exit 2; }
    "${PIN[@]}" perf record -q -m 1M -e instructions:u -c "${PERIOD}" \
        --call-graph dwarf,16384 --user-callchains -o "${F}.perf.data" -- \
        "${BIN}" "${ARGV[@]}" > "${F}.out" 2> "${F}.err" &
    ;;
*) echo "unknown mode ${MODE}" >&2; exit 2 ;;
esac
wrapper=$!
start=$(date +%s.%N)
lane_pid() {
    if [[ "${MODE}" == run && ${#PIN[@]} -eq 0 ]]; then echo "${wrapper}"; return; fi
    local w="${wrapper}" c
    for _ in 1 2 3; do
        c=$(pgrep -P "${w}" | head -1)
        [[ -z "${c}" ]] && break
        [[ "$(cat /proc/${c}/comm 2>/dev/null)" == hven_bench_cell ]] && { echo "${c}"; return; }
        w="${c}"
    done
    [[ "$(cat /proc/${wrapper}/comm 2>/dev/null)" == hven_bench_cell ]] && echo "${wrapper}"
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
    sleep 0.1
done
wait "${wrapper}"; rc=$?
end=$(date +%s.%N)
wall=$(awk "BEGIN{printf \"%.2f\", ${end}-${start}}")
if [[ ${capped} -eq 1 ]]; then
    echo "CELL_RESULT ${MODE} ${LANE} ${P} ${LABEL} ${THREADS} CAPPED rc=${rc} wall=${wall}" | tee "${F}.result"
else
    echo "CELL_RESULT ${MODE} ${LANE} ${P} ${LABEL} ${THREADS} rc=${rc} wall=${wall}" | tee "${F}.result"
fi
