#!/usr/bin/env bash
# One BOX LEG for E4 FIX5: pre-lock uncovered-cell gate, then flock with NO
# timeout and NO fallback, the in-lock pgrep, FOREGROUND_START/END markers.
# Never wrapped in `timeout`.  Usage: leg.sh LABEL COMMAND...
set -o pipefail
label="$1"; shift
uncovered_cells() {
    local holders pid rest p covered
    holders="$(fuser /tmp/box-build.lock 2>/dev/null | tr -s ' ' '\n' | grep -E '^[0-9]+$' || true)"
    while read -r pid rest; do
        [[ -z "${pid}" ]] && continue
        covered=no; p="${pid}"
        for _ in $(seq 1 40); do
            [[ -z "${p}" || "${p}" == "1" || "${p}" == "0" ]] && break
            if grep -qx "${p}" <<< "${holders}"; then covered=yes; break; fi
            p="$(ps -o ppid= -p "${p}" 2>/dev/null | tr -d ' ')"
        done
        [[ "${covered}" == "yes" ]] || echo "${pid} ${rest}"
    done < <(pgrep -a hven_bench_cell || true)
}
waited=0
while true; do
    seen="$(uncovered_cells)"; [[ -z "${seen}" ]] && break
    if [[ ${waited} -eq 0 ]]; then echo "FOREIGN_CELL_OUTSIDE_LOCK_WAITING ${label} $(date -Is)"; echo "${seen}"; fi
    waited=$((waited + 1)); sleep 20
done
if [[ ${waited} -eq 0 ]]; then
    echo "PGREP_NO_UNCOVERED_CELL ${label} $(date -Is)"
    covered="$(pgrep -a hven_bench_cell || true)"
    [[ -n "${covered}" ]] && { echo "CELLS_PRESENT_BUT_UNDER_THE_LOCK:"; echo "${covered}"; }
else
    echo "PGREP_NO_UNCOVERED_CELL_AFTER_WAITING ${waited} polls $(date -Is)"
fi
exec flock /tmp/box-build.lock bash -c '
set -o pipefail
label="$1"; shift
inlock=0
while true; do
    seen="$(pgrep -a hven_bench_cell || true)"; [[ -z "${seen}" ]] && break
    if [[ ${inlock} -eq 0 ]]; then echo "IN_LOCK_CELL_PRESENT_WAITING ${label} $(date -Is)"; echo "${seen}"; fi
    inlock=$((inlock + 1)); sleep 10
done
if [[ ${inlock} -eq 0 ]]; then echo "IN_LOCK_PGREP_EMPTY ${label} $(date -Is)";
else echo "IN_LOCK_PGREP_EMPTY_AFTER_WAITING ${inlock} polls $(date -Is)"; fi
echo "FOREGROUND_START ${label} $(date -Is)"
"$@"
rc=$?
echo "FOREGROUND_END ${label} $(date -Is) rc=${rc}"
exit ${rc}
' _ "${label}" "$@"
