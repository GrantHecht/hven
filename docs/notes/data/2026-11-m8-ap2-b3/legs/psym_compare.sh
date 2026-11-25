#!/usr/bin/env bash
# AP-2 B3-g: P-SYM census of the layout change (INFORMATIONAL inventory). Captures
# build-b3/rel (B3, built in place at the same absolute path as the pin capture) into
# build-b3/psym-after and compares it with build-b3/psym-before, no maps.
# usage: psym_compare.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash psym_compare.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a3b067b098f5e9221
EV=${1:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$EV"
rm -rf build-b3/psym-after
bash scripts/psym_compare.sh capture build-b3/psym-after "$WT/build-b3/rel" > "$EV/capture-after.txt" 2>&1
echo "capture rc=$?"
PSYM_MAX_PAIRS=3 bash scripts/psym_compare.sh compare build-b3/psym-before build-b3/psym-after > "$EV/compare.txt" 2>&1
rc=$?
echo "compare rc=$rc (informational)"
grep -E '^(COUNTS|RESULT|VERDICT|SUMMARY)|TOTAL' "$EV/compare.txt" | tail -20
echo "FOREGROUND_END $(date -Is)"
exit 0
