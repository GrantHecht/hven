#!/usr/bin/env bash
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1 r2, P8's warning-set criterion (../../DECLARATION-r2.txt; settler ruling C269): arm A's Release build log
# (build-dn1e/armA.build.log, legs/build.sh's flags, read-only) against arm B's (build-dn1r2/build-rel.out, the same
# script's Release build of the code head), through legs/warnings_compare.py.
# usage: warnings.sh <evidence-dir>   Run as: flock /tmp/box-build.lock bash warnings.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1b
EV=${1:?evidence dir}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
mkdir -p "$EV"
A=$WT/build-dn1be/armA.build.log
B=$WT/build-dn1be/build-rel.out
sha256sum "$A" "$B"
python3 "$WT/docs/notes/data/2026-11-m8-dn1b-guard/legs/warnings_compare.py" "$A" "$B" > "$EV/warnings-compare.txt" 2>&1
rc=$?
grep -E '^arm [AB]:|^ONLY IN|^COUNT MOVED|^WARNINGS_' "$EV/warnings-compare.txt"
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
