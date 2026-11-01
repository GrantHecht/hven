#!/usr/bin/env bash
# M7 W2 M2 chain 1: compile the S5 commit and run the tests it moves, then the
# whole Release suite. One lock acquisition, no timeout, no fallback.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
LOG=$ROOT/.scratch/m7w2m2/logs/chain1b.log
exec >"$LOG" 2>&1
echo "CHAIN1B_START $(date -Is)"
flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
echo "FOREGROUND_START chain1b $(date -Is)"
cd "$ROOT" || exit 90
echo "=== HEAD $(git rev-parse --short HEAD)  dirty-files=$(git status --porcelain | wc -l) ==="
cmake --build build -j "$(nproc)" || exit 91
echo "--- BUILD OK $(date -Is)"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
ctest --test-dir build -j4 -R "Restoration|TraceWriter|JsonLines|Callback|Threads|Counters" --output-on-failure || exit 92
echo "--- TARGETED OK $(date -Is)"
ctest --test-dir build -j4 --output-on-failure || exit 93
echo "--- RELEASE SUITE OK $(date -Is)"
echo "FOREGROUND_END chain1b $(date -Is) rc=0"
'
rc=$?
echo "WRAPPER_RC chain1b rc=${rc}"
echo "CHAIN1B_END $(date -Is)"
exit $rc
