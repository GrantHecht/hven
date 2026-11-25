#!/usr/bin/env bash
# M8 walk cap ADDENDUM-r1 (../../ADDENDUM-r1.md): I4 co-run. 4 workers, cores 2-5, each takes every 4th roster line,
# D then K per cell through ./cell.sh (CELL_CORE). Then train_check.py vs the walk-cap leg rows. usage: train_corun.sh <lane sha256>
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
D=$WT/docs/notes/data/2026-11-m8-walk-cap
A=$D/legs/addendum-r1
EV=$D/addendum-r1/train
BIN=$WT/build-wc/bin-r1/hven_bench_cell
ROSTER=$WT/docs/notes/data/2026-11-m8-dn3/roster.tsv
CAP=300
WANT=${1:?lane sha}
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "START $(date -Is)"
[[ "$(sha256sum "$BIN" | cut -d' ' -f1)" == "$WANT" ]] || { echo LANE_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
mkdir -p "$EV/cells"
worker() {
  local w=$1 core=$2 i=0 pop lane prob a arm label tag
  while IFS=$'\t' read -r pop lane prob; do
    if (( i % 4 == w )); then
      tag="${pop}-${lane}-${prob}"
      for a in D:default K:never-default; do
        arm=${a%%:*}; label=${a#*:}
        [[ -f "$EV/cells/$tag.$arm.result" ]] || CELL_CORE=$core CELL_BIN=$BIN bash "$A/cell.sh" run "$lane" "$prob" "$label" 1 "$EV/cells/$tag.$arm" "$CAP" > /dev/null
      done
    fi
    i=$((i + 1))
  done < "$ROSTER"
}
for w in 0 1 2 3; do worker $w $((w + 2)) & done
wait
echo "co-run done $(date -Is)"
python3 "$D/legs/train_check.py" "$EV" "$WT/docs/notes/data/2026-11-m8-dn3" "$D/train" "$D/legs/movers-train.txt" > "$EV/train_check.txt" 2>&1; rc=$?
grep -E '^(SUMMARY|MOVER|  FAIL|I4)' "$EV/train_check.txt" | head -60
echo "train_check rc=$rc"
echo "END $(date -Is)"
exit $rc
