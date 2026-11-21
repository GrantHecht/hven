#!/usr/bin/env bash
# M8 dense gate (../DECLARATION.md, before the declaration): the lane clone's decoded CUTEst instances
# (.scratch/h3/decoded/, git-ignored, deleted with the build trees) re-made by the harness's own
# scripts/decode_cutest.sh with SET=small-dense (TRAIN only; the harness's holdout guard runs first). SIFDecode is an
# external install invoked as a process (owner ruling 2026-09-15). Not a leg.
# usage: decode.sh   Run as: flock /tmp/box-build.lock bash decode.sh
set -o pipefail
HB=/home/ghecht/Projects/hven-bench-dn1
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$HB" || exit 1
SET=small-dense bash scripts/decode_cutest.sh 2>&1 | tail -5
rc=$?
gfortran --version | head -1
echo "rc=$rc"
echo "FOREGROUND_END $(date -Is)"
exit $rc
