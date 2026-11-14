#!/usr/bin/env bash
# M8 DN-0 IPM r1 legs. Run each leg as:  flock /tmp/box-build.lock bash legs.sh LEG HARNESS_ROOT RUNDIR
# Serial: one cell process at a time, the lane process pinned to core 2 (DN0_CORE), alone under the box lock.
#
#   S1      warm-up (one untimed, unrecorded process), then perf stat round 1 (MEASURED binary), CAP 300 s
#   S2, S3  perf stat rounds 2 and 3 (MEASURED binary) over the cells NOT capped in S1
#   SC      perf stat, one round, RECORD (CFI) binary, cells not capped in S1: counter identity, informational
#   P       perf record per cell (RECORD binary; period/repeats from the S medians, DECLARATION-r1 §3)
#   D       KKT dump per cell (dump build), CAP 300 s (60 s for an S1-capped cell)
#   R       dense replay per dumped cell: verify (iparm identity) then measure
set -uo pipefail
LEG="$1"; HB="$2"; RUN="$3"
TOOLS="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "BUSY ${p}"; exit 9; }
echo "FOREGROUND_START ${LEG} $(date -u +%FT%TZ) max_sample_rate=$(cat /proc/sys/kernel/perf_event_max_sample_rate)"
bash "${TOOLS}/preflight.sh" "${HB}" "${DN0_HARNESS_COMMIT:-38f5b4a}" || exit 3
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
mkdir -p "${RUN}"
ROSTER="${RUN}/roster.tsv"
if [[ ! -f "${ROSTER}" ]]; then
    python3 "${TOOLS}/roster.py" "${HB}" > "${ROSTER}.tmp" || { echo "ROSTER_REFUSED"; exit 3; }
    mv "${ROSTER}.tmp" "${ROSTER}"
fi
[[ "$(wc -l < "${ROSTER}")" == 119 ]] || { echo "ROSTER_REFUSED count"; exit 3; }
CAP=300
CAP_SHORT=60
capped_s1() { grep -q CAPPED "${RUN}/stat-r1/$1.result" 2>/dev/null; }
stat_instr() { # median instructions over the S rounds present for cell $1 (MEASURED binary only)
    python3 - "$RUN" "$1" <<'PY'
import sys, os, statistics
run, tag = sys.argv[1], sys.argv[2]
v = []
for r in (1, 2, 3):
    f = os.path.join(run, f"stat-r{r}", tag + ".stat")
    if os.path.exists(f):
        for line in open(f):
            parts = line.split(",")
            if len(parts) > 2 and parts[2].startswith("instructions"):
                v.append(int(parts[0]))
print(int(statistics.median(v)) if v else 0)
PY
}
rc_leg=0
case "${LEG}" in
S1|S2|S3|SC)
    r="${LEG#S}"
    mode=stat; [[ "${LEG}" == SC ]] && mode=statrec
    out="${RUN}/stat-r${r}"; mkdir -p "${out}"
    if [[ "${r}" == 1 ]]; then
        read -r pop lane prob < <(head -1 "${ROSTER}")
        bash "${TOOLS}/cell.sh" stat "${HB}" "${lane}" "${prob}" "${RUN}/warmup" "${CAP}" > /dev/null
        rm -f "${RUN}"/warmup.*
        echo "WARMUP_DONE ${lane} ${prob} (discarded)"
    fi
    while IFS=$'\t' read -r -u 3 pop lane prob; do
        tag="${pop}-${lane}-${prob}"
        [[ -f "${out}/${tag}.result" ]] && continue
        if [[ "${r}" != 1 ]] && capped_s1 "${tag}"; then continue; fi
        bash "${TOOLS}/cell.sh" "${mode}" "${HB}" "${lane}" "${prob}" "${out}/${tag}" "${CAP}"
        grep -q ',instructions' "${out}/${tag}.stat" || { echo "STAT_MISSING_INSTRUCTIONS ${tag}"; rc_leg=4; }
    done 3< "${ROSTER}"
    ;;
P)
    out="${RUN}/record"; mkdir -p "${out}"
    while IFS=$'\t' read -r -u 3 pop lane prob; do
        tag="${pop}-${lane}-${prob}"
        [[ -f "${out}/${tag}.attr.json" ]] && continue
        T="$(stat_instr "${tag}")"
        read -r base reps cap < <(python3 - "$T" "$(capped_s1 "${tag}" && echo 1 || echo 0)" <<'PY'
import sys, math
T, capped = int(sys.argv[1]), sys.argv[2] == "1"
base = max(250_007, T // 8000) | 1
reps = 1 if capped else min(100, max(10, math.ceil(8000 * base / max(T, 1))))
print(base, reps, 60 if capped else 300)
PY
)
        : > "${out}/${tag}.runs"
        rm -f "${out}/${tag}.script"
        for ((k = 0; k < reps; k++)); do
            per=$(( (base * (1000 + 7 * k) / 1000) | 1 ))
            bash "${TOOLS}/cell.sh" record "${HB}" "${lane}" "${prob}" "${out}/${tag}.k${k}" "${cap}" "${per}" > /dev/null
            st="$(perf report -i "${out}/${tag}.k${k}.perf.data" --stats 2>/dev/null)"
            lost=$(grep -E '^\s*LOST events' <<< "${st}" | awk '{print $3}' | head -1)
            lsamp=$(grep -E '^\s*LOST_SAMPLES events' <<< "${st}" | awk '{print $3}' | head -1)
            thr=$(grep -E '^\s*THROTTLE events' <<< "${st}" | awk '{print $3}' | head -1)
            echo "k=${k} period=${per} $(cat "${out}/${tag}.k${k}.result") LOST=${lost:-0} LOST_SAMPLES=${lsamp:-0} THROTTLE=${thr:-0}" >> "${out}/${tag}.runs"
            if [[ "${lost:-0}" != 0 || "${lsamp:-0}" != 0 || "${thr:-0}" != 0 ]]; then
                echo "SAMPLING_DEFECT ${tag} k=${k} LOST=${lost:-0} LOST_SAMPLES=${lsamp:-0} THROTTLE=${thr:-0}"; rc_leg=5
            fi
            perf script -i "${out}/${tag}.k${k}.perf.data" --no-inline -F comm,tid,period,ip,sym,dso \
                2>/dev/null >> "${out}/${tag}.script"
            rm -f "${out}/${tag}.k${k}.perf.data"
        done
        python3 "${TOOLS}/attribute.py" "${TOOLS}/bucket_map.tsv" "${out}/${tag}.script" > "${out}/${tag}.attr.json.tmp" \
            && mv "${out}/${tag}.attr.json.tmp" "${out}/${tag}.attr.json"
        gzip -f "${out}/${tag}.script"
        [[ -f "${out}/${tag}.attr.json" ]] || { echo "ATTRIBUTION_MISSING ${tag}"; rc_leg=4; }
        echo "RECORD_DONE ${tag} T=${T} base=${base} reps=${reps}"
    done 3< "${ROSTER}"
    ;;
D)
    out="${RUN}/dump"; mkdir -p "${out}"
    while IFS=$'\t' read -r -u 3 pop lane prob; do
        tag="${pop}-${lane}-${prob}"
        [[ -f "${out}/${tag}.result" ]] && continue
        cap="${CAP}"; capped_s1 "${tag}" && cap="${CAP_SHORT}"
        rm -rf "${out}/${tag}.dump"
        bash "${TOOLS}/cell.sh" dump "${HB}" "${lane}" "${prob}" "${out}/${tag}" "${cap}"
    done 3< "${ROSTER}"
    ;;
R)
    out="${RUN}/replay"; mkdir -p "${out}"
    T="${HB}/.scratch/dn0r1/build-tools"
    while IFS=$'\t' read -r -u 3 pop lane prob; do
        tag="${pop}-${lane}-${prob}"
        [[ -f "${out}/${tag}.replay.csv" ]] && continue
        mats=( "${RUN}/dump/${tag}.dump"/mat_*.bin )
        [[ -f "${mats[0]}" ]] || { echo "REPLAY_NONE ${tag}"; continue; }
        taskset -c "${DN0_CORE:-2}" "${T}/dn0_replay_verify" "${out}/${tag}.verify.csv" "${mats[@]}" 2> "${out}/${tag}.verify.err"
        taskset -c "${DN0_CORE:-2}" "${T}/dn0_replay" "${out}/${tag}.replay.csv.tmp" "${mats[@]}" 2> "${out}/${tag}.replay.err" \
            && mv "${out}/${tag}.replay.csv.tmp" "${out}/${tag}.replay.csv"
        nbad=$(awk -F, 'NR==1{for(i=1;i<=NF;i++) if($i=="iparm_match") c=i; next} c && $c!="1"' "${out}/${tag}.verify.csv" | wc -l)
        [[ "${nbad}" == 0 ]] || { echo "IPARM_IDENTITY_FAILED ${tag} ${nbad}"; rc_leg=4; }
        echo "REPLAY_DONE ${tag} mats=${#mats[@]}"
    done 3< "${ROSTER}"
    ;;
*) echo "unknown leg ${LEG}"; exit 2 ;;
esac
echo "FOREGROUND_END ${LEG} $(date -u +%FT%TZ) max_sample_rate=$(cat /proc/sys/kernel/perf_event_max_sample_rate) rc=${rc_leg}"
exit "${rc_leg}"
