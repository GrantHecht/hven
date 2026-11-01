#!/usr/bin/env bash
# M7 W2 A-SQP -- ONE hven-sqp cell, re-run at a chosen MAJOR-ITERATION CAP.
#
# Counters and statuses only.  No wall-clock number this script produces is a
# measurement and none is quoted anywhere (hven docs/measurement.md sec 1, 3.1).
#
# It runs the HARNESS's already-built binary
# (/home/ghecht/Projects/hven-bench/build/lanes/hven/hven_bench_cell) from a
# PRIVATE root under this task's scratch, which holds a COPY of the decoded
# CUTEst instances, so nothing is written into the harness tree.
#
# LOCK: tries the box lock first (flock -w LOCKWAIT on fd 9, held for the whole
# leg).  If the foreign lane (the W2 A4-0 sweep) holds it, the leg runs as a
# DECLARED CO-RUN: this process taskset-pinned to one logical CPU,
# MKL_NUM_THREADS=1, OMP_NUM_THREADS=1.  The foreign lane's placement is NOT
# controlled, so this is not the "one process per pinned PHYSICAL core"
# discipline sec 3 defines and nothing here is offered as one.  Every leg's log
# records LOCK=held or LOCK=corun.
#
# Usage: run_cell.sh PROBLEM LABEL BUDGET_ITERS TIMEOUT_S TAG
set -u
set -o pipefail

SELF_ROOT=/home/ghecht/Projects/hven/.scratch/m7w2a
BIN=${HVEN_W2A_BIN:-/home/ghecht/Projects/hven-bench/build/lanes/hven/hven_bench_cell}
LOCK=/tmp/box-build.lock
LOCKWAIT=${HVEN_W2A_LOCKWAIT:-20}
PINCPU=${HVEN_W2A_PINCPU:-7}

if [[ $# -ne 5 ]]; then
    printf 'usage: run_cell.sh PROBLEM LABEL BUDGET_ITERS TIMEOUT_S TAG\n' >&2
    exit 2
fi
PROBLEM="$1"; LABEL="$2"; BITERS="$3"; TMO="$4"; TAG="$5"
OUT="${SELF_ROOT}/raw/${TAG}.log"

cd "${SELF_ROOT}/root" || exit 3
: >"${OUT}"
{
    printf 'PGREP_FOREIGN_BEGIN\n'
    pgrep -af 'run_serial|hven_bench_cell' 2>&1 | cut -c1-160
    printf 'PGREP_FOREIGN_END\n'
} >>"${OUT}"

exec 9>"${LOCK}" || exit 3
if flock -w "${LOCKWAIT}" 9; then HOW=held; else HOW=corun; fi

{
    printf 'LEG_START %s\n' "$(date -Is)"
    printf 'LOCK=%s\n' "${HOW}"
    printf 'CELL problem=%s lane=hven-sqp label=%s budget_iters=%s pin_cpu=%s timeout_s=%s\n' \
           "${PROBLEM}" "${LABEL}" "${BITERS}" "${PINCPU}" "${TMO}"
    printf 'BINARY %s\n' "${BIN}"
} >>"${OUT}"

env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 \
    taskset -c "${PINCPU}" \
    timeout -s TERM -k 10 "${TMO}" \
    "${BIN}" --lane hven-sqp --problem "${PROBLEM}" --options "${LABEL}" \
             --budget-iters "${BITERS}" >>"${OUT}" 2>&1
rc=$?
{
    printf 'CELL_RC=%d\n' "${rc}"
    printf 'LEG_END %s\n' "$(date -Is)"
    printf 'LEG_RC=%d\n' "${rc}"
} >>"${OUT}"
exit "${rc}"
