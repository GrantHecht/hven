#!/usr/bin/env bash
# Run several run_cell.sh legs in sequence.  Each line of the plan file is
#   PROBLEM LABEL BUDGET_ITERS TIMEOUT_S
# and the TAG is derived.  A leg that fails does NOT stop the batch (a timeout
# is a datum here), but its rc is recorded and the batch's own rc is the count
# of legs whose wrapper reported an unexpected failure (rc not 0 and not 124).
set -u
set -o pipefail
SELF_ROOT=/home/ghecht/Projects/hven/.scratch/m7w2a
PLAN="$1"
bad=0
printf 'BATCH_START %s plan=%s\n' "$(date -Is)" "${PLAN}"
while read -r prob label biters tmo; do
    [[ -z "${prob:-}" || "${prob}" == \#* ]] && continue
    tag="${prob}-${label}-b${biters}"
    "${SELF_ROOT}/run_cell.sh" "${prob}" "${label}" "${biters}" "${tmo}" "${tag}"
    rc=$?
    printf 'BATCH_LEG %-12s %-8s b=%-3s rc=%s\n' "${prob}" "${label}" "${biters}" "${rc}"
    if [[ ${rc} -ne 0 && ${rc} -ne 124 ]]; then bad=$((bad+1)); fi
done <"${PLAN}"
printf 'BATCH_END %s bad=%d\n' "$(date -Is)" "${bad}"
exit "${bad}"
