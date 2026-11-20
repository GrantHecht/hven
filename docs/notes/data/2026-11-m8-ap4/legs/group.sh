#!/usr/bin/env bash
# usage: group.sh <label> <evidence dir>   -- the M6 proof of one group at the current tree: ctest Release (build-rel must be built),
# replay B, Debug build + ctest, install smoke. Each step is its own box-lane call.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/ap4
LABEL=${1:?label}; EV=${2:?ev}; mkdir -p "$EV"
L=~/.local/bin/box-lane
cd $W
echo "commit $(git rev-parse HEAD) label $LABEL start $(date -Is)" | tee "$EV/head.txt"
$L -- bash .scratch/legs/build.sh $W Release $W/.scratch/build-rel > "$EV/build-rel.out" 2>&1; echo "build-rel rc=$?" | tee -a "$EV/summary.txt"
$L -- bash .scratch/legs/ctest.sh $W/.scratch/build-rel "$EV/ctest-rel.log" > "$EV/ctest-rel.out" 2>&1; echo "ctest-rel rc=$?" | tee -a "$EV/summary.txt"
$L -- bash .scratch/legs/replay.sh $W/.scratch/build-rel "$EV/replay" B-$LABEL > "$EV/replay-B.out" 2>&1; echo "replay-B rc=$?" | tee -a "$EV/summary.txt"
while [ ! -s "$W/docs/notes/data/2026-11-m8-ap4/gate/replay-A/A-ipm.csv" ] || pgrep -f "replay.sh.*build-A-rel" >/dev/null; do sleep 15; done
for m in walk ssn ipm; do python3 scripts/compare_replay.py "$W/docs/notes/data/2026-11-m8-ap4/gate/replay-A/A-$m.csv" "$EV/replay/B-$LABEL-$m.csv" $m ; done > "$EV/replay-compare.txt" 2>&1; cat "$EV/replay-compare.txt" | tee -a "$EV/summary.txt"
$L -- bash .scratch/legs/build.sh $W Debug $W/.scratch/build-debug > "$EV/build-debug.out" 2>&1; echo "build-debug rc=$?" | tee -a "$EV/summary.txt"
$L -- bash .scratch/legs/ctest.sh $W/.scratch/build-debug "$EV/ctest-dbg.log" > "$EV/ctest-dbg.out" 2>&1; echo "ctest-dbg rc=$?" | tee -a "$EV/summary.txt"
WT=$W $L -- bash .scratch/legs/install_smoke.sh "$EV/install-smoke" > "$EV/install-smoke.out" 2>&1; echo "install-smoke rc=$?" | tee -a "$EV/summary.txt"
echo "done $(date -Is)" | tee -a "$EV/summary.txt"
