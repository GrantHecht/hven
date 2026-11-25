#!/usr/bin/env bash
# AP-2 B3: run four legs in sequence, each under its own flock (each leg script checks
# pgrep inside the lock). Install smoke, examples, PCH neutrality, off-reference build.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a3b067b098f5e9221
L=$WT/docs/notes/data/2026-11-m8-ap2-b3
cd "$WT" || exit 1
pgrep -a hven_bench_cell && exit 3
flock /tmp/box-build.lock bash "$L/legs/install_smoke.sh" "$L/install-smoke" > build-b3/smoke.out 2>&1
echo "smoke rc=$?"
pgrep -a hven_bench_cell && exit 3
flock /tmp/box-build.lock bash "$L/legs/examples.sh" > build-b3/examples.out 2>&1
echo "examples rc=$?"
pgrep -a hven_bench_cell && exit 3
flock /tmp/box-build.lock bash "$L/legs/misc.sh" pch "$L/pch" > build-b3/pch.out 2>&1
echo "pch rc=$?"
pgrep -a hven_bench_cell && exit 3
flock /tmp/box-build.lock bash "$L/legs/offref.sh" "$L/offref" > build-b3/offref.out 2>&1
echo "offref rc=$?"
echo ALLDONE
