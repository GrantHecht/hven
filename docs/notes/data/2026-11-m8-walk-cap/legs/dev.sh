#!/usr/bin/env bash
# M8 walk cap (../DECLARATION.md section 2, before the declaration; NOT a leg): the dev runs whose rows the declaration
# names as movers. On the frozen binaries of the code commit: the 27-cell replay once (replay.sh, REPLAY_KS=1, no dev
# reference yet: its I1-B mover check runs against an empty mover file and so lists every mover), saved as
# ../dev/replay/dev-<mode>.csv; then the 238-cell TRAIN enumeration (train_enum.sh, report mode) into ../dev/train.
# usage: dev.sh <corpus sha256> <lane sha256>   Run as: flock /tmp/box-build.lock bash dev.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a7a2ce81cf8988de5
D=$WT/docs/notes/data/2026-11-m8-walk-cap
CS=${1:?corpus sha256}
LS=${2:?lane sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
[ ! -e "$D/legs/movers-replay.txt" ] || { echo "MOVERS_FILE_EXISTS (dev runs precede the declaration)"; exit 2; }
mkdir -p "$D/dev/replay" "$D/dev/train"
REPLAY_KS=1 bash "$D/legs/replay.sh" "$D/dev/replay" "$CS"
for m in walk ssn ipm; do cp "$D/dev/replay/k1-$m.csv" "$D/dev/replay/dev-$m.csv"; done
bash "$D/legs/train_enum.sh" "$D/dev/train" "$LS"
echo "FOREGROUND_END $(date -Is)"
