#!/usr/bin/env bash
# M8 DN-0 IPM r1 chain: S1 S2 S3 SC P D R, each leg under its own box-lock acquisition, in order;
# stops at the first failing leg (DECLARATION-r1 §7, §9). Logs to RUNDIR/../logs/leg-<LEG>.log and chain.log.
# Usage: bash run_chain.sh HARNESS_ROOT [FIRST_LEG]
set -uo pipefail
HB="$1"; FIRST="${2:-S1}"
TOOLS="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
RUN="${HB}/.scratch/dn0r1/run"; LOGS="${HB}/.scratch/dn0r1/logs"
mkdir -p "${LOGS}"
started=0
for L in S1 S2 S3 SC P D R; do
    [[ "${L}" == "${FIRST}" ]] && started=1
    [[ ${started} == 1 ]] || continue
    p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] || { echo "BUSY before ${L}: ${p}" >> "${LOGS}/chain.log"; exit 9; }
    flock /tmp/box-build.lock bash "${TOOLS}/legs.sh" "${L}" "${HB}" "${RUN}" > "${LOGS}/leg-${L}.log" 2>&1
    rc=$?
    if [[ ${rc} != 0 ]]; then echo "LEG_FAILED ${L} rc=${rc} $(date -u +%FT%TZ)" >> "${LOGS}/chain.log"; exit 1; fi
    echo "LEG_DONE ${L} $(date -u +%FT%TZ)" >> "${LOGS}/chain.log"
done
echo "CHAIN_DONE $(date -u +%FT%TZ)" >> "${LOGS}/chain.log"
