#!/usr/bin/env bash
# RF-1c: re-create the swept pilot harness copy /home/ghecht/Projects/hven/.scratch/tu1-diag/hb (it was swept; pilot_build.sh
# expects it there): the hven-bench-tu1 tree at commit 146e6ec (git archive: tracked files only, the tree itself untouched)
# plus the DIAGQP stderr line (diagqp_patch.py, re-written for this gate from the line's format in the earlier pilots' .err
# files). Idempotent: refuses to overwrite an existing copy. usage: harness_rebuild.sh <worktree>
set -euo pipefail
WT=${1:?worktree}
DST=/home/ghecht/Projects/hven/.scratch/tu1-diag/hb
SRCREPO=/home/ghecht/Projects/hven-bench-tu1
[ "$(git -C $SRCREPO rev-parse 146e6ec^{commit})" = 146e6ecd85cbc6dbb11626f866665e3e87a0665f ]
[ -e "$DST" ] && { echo "EXISTS $DST"; exit 4; }
mkdir -p "$DST"
git -C $SRCREPO archive 146e6ec | tar -xf - -C "$DST"
python3 "$WT/docs/notes/data/2026-11-m8-rf1/legs-rf1c/diagqp_patch.py" "$WT" "$DST/lanes/hven/hven_bench_cell.cpp"
( cd "$DST" && find . -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum )
echo REBUILT
