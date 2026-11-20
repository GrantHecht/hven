#!/usr/bin/env bash
# PF-5 prototype co-run (CLAUDE.md s7 terms): one process per pinned PHYSICAL core of the lane ($BOX_LANE_PHYS,
# SMT siblings idle), MKL_NUM_THREADS=1; asserts only per-process instructions:u and deterministic rows.
# usage: corun.sh <outdir> <cap> <jobfile: lines "MODE CELL NAME RETAIN FLAG">   Run as ONE box-lane step.
set -uo pipefail
S=/home/ghecht/Projects/hven/.claude/worktrees/pf5-proto/.scratch
OUT=$(realpath -m "$1"); CAP=$2; JOBS=$3
export TMPDIR=$S/tmp CELL_BIN=${CELL_BIN:-$S/bin/proto/hven_bench_cell}
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=[$BOX_LANE_PHYS]"; sha256sum "$CELL_BIN"
read -ra CORES <<< "$BOX_LANE_PHYS"
run_one() { # core mode cell name retain flag
  local core=$1 mode=$2 P=$3 NAME=$4 RET=$5 FLAG=$6
  mkdir -p "$OUT/$NAME"
  env -u HVEN_PF5_RETAIN -u HVEN_PF5_PROBE -u HVEN_PF5_VERIFY -u HVEN_PF5_KEEP PINCPU=$core \
    $( [ "$RET" != 0 ] && echo HVEN_PF5_RETAIN=$RET ) $( [[ "$FLAG" == *PROBE* ]] && echo HVEN_PF5_PROBE=1 ) $( [[ "$FLAG" == *VERIFY* ]] && echo HVEN_PF5_VERIFY=1 ) $( [[ "$FLAG" == *KEEP* ]] && echo HVEN_PF5_KEEP=1 ) \
    bash $S/tools/cell.sh "$mode" hven-sqp "$P" default 1 "$OUT/$NAME/nlp-hven-sqp-$P.D" "$CAP"
}
i=0; declare -A busy
while read -r mode P NAME RET FLAG; do
  [ -z "$mode" ] && continue
  while :; do
    for c in "${CORES[@]}"; do
      pid=${busy[$c]:-}
      if [ -z "$pid" ] || ! kill -0 "$pid" 2>/dev/null; then
        run_one "$c" "$mode" "$P" "$NAME" "$RET" "${FLAG:-none}" & busy[$c]=$!; continue 3
      fi
    done
    sleep 1
  done
done < "$JOBS"
wait
echo "FOREGROUND_END $(date -Is)"
