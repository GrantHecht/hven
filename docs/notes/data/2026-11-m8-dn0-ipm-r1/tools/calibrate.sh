#!/usr/bin/env bash
# M8 DN-0 IPM r1: PRE-DECLARATION sampling calibration on cells OUTSIDE the DN-0 population.
# Cells: cutest-nlp-35 TRAIN, hven-ipm lane (SMBANK READING6 ZAMB2-8 DALLASM ACOPP57), run from the MAIN
# harness root (/home/ghecht/Projects/hven-bench, whose .scratch/h3/decoded holds them) with the RECORD binary.
# Each name is checked TRAIN by schema/split.py and absent from the small-dense and hs-27 sets first.
# Per cell: one perf stat; then for each candidate period 3 perf record runs (the declared record options),
# keeping `perf report --stats` (LOST / THROTTLE) and sample timestamps (peak samples per 1 ms window).
# Env: CAL_M (perf record -m, default 1M), CAL_STACK (dwarf stack bytes, default 16384), and CAL_PERFCORE
# (unset: `taskset -c 2 perf record -- REC`, DN-0's form, perf and the lane process share core 2; set to C:
# `taskset -c C perf record -- taskset -c 2 REC`, perf's reader on core C, the lane process alone on 2), logged.
# Run under: flock /tmp/box-build.lock bash calibrate.sh SD_HARNESS OUTDIR "PERIODS"
set -uo pipefail
SD="$1"; OUT="$2"; PERIODS="$3"
MAIN=/home/ghecht/Projects/hven-bench
REC="${SD}/.scratch/dn0r1/build-cfi/hven_bench_cell"
CELLS="SMBANK READING6 ZAMB2-8 DALLASM ACOPP57"
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "BUSY ${p}"; exit 9; }
echo "FOREGROUND_START calibrate $(date -u +%FT%TZ) max_sample_rate=$(cat /proc/sys/kernel/perf_event_max_sample_rate)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
M="${CAL_M:-1M}"; STACK="${CAL_STACK:-16384}"
PC="${CAL_PERFCORE:-}"
if [[ -n "${PC}" ]]; then PRE=(taskset -c "${PC}"); POST=(taskset -c 2); else PRE=(taskset -c 2); POST=(); fi
echo "CALIB_PARAMS -m ${M} --call-graph dwarf,${STACK} periods ${PERIODS} perfcore ${PC:-2(shared)}"
python3 - "${SD}" ${CELLS} <<'PY' || { echo CALIB_REFUSED; exit 3; }
import json, os, sys
sd = sys.argv[1]; sys.path.insert(0, os.path.join(sd, "schema")); import split
sdset = {p["problem"] for p in json.load(open(os.path.join(sd, "sets", "cutest-small-dense.json")))["problems"]}
hs = {p["problem"].upper() for p in json.load(open(os.path.join(sd, "sets", "hs-27.json")))["problems"]}
for n in sys.argv[2:]:
    assert split.assign("cutest", n) == "TRAIN", n
    assert n not in sdset and n.upper() not in hs, n
print("calibration cells: TRAIN, outside the DN-0 population")
PY
mkdir -p "${OUT}"
cd "${MAIN}" || exit 2
for c in ${CELLS}; do
    ARGV=(--lane hven-ipm --problem "${c}" --options default --budget-iters 0 --options-dump "${OUT}/${c}.optdump" --assert-reference)
    taskset -c 2 perf stat -x, -e instructions:u -o "${OUT}/${c}.stat" -- "${REC}" "${ARGV[@]}" > "${OUT}/${c}.stat.out" 2>&1
    for per in ${PERIODS}; do
        for k in 0 1 2; do
            f="${OUT}/${c}.p${per}.k${k}"
            "${PRE[@]}" perf record -q -m "${M}" -e instructions:u -c "${per}" --call-graph "dwarf,${STACK}" --user-callchains \
                -o "${f}.perf.data" -- "${POST[@]}" "${REC}" "${ARGV[@]}" > "${f}.out" 2> "${f}.err"
            perf report -i "${f}.perf.data" --stats > "${f}.stats" 2>&1
            perf script -i "${f}.perf.data" -F comm,time,period 2>/dev/null | awk '$1=="hven_bench_cell"{print $2, $3}' > "${f}.times"
            perf script -i "${f}.perf.data" --no-inline -F comm,tid,period,ip,sym,dso 2>/dev/null | gzip > "${f}.script.gz"
            rm -f "${f}.perf.data"
        done
    done
    echo "CALIB_CELL_DONE ${c}"
done
echo "FOREGROUND_END calibrate $(date -u +%FT%TZ) max_sample_rate=$(cat /proc/sys/kernel/perf_event_max_sample_rate)"
