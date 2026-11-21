#!/usr/bin/env bash
# Run as: flock /tmp/box-build.lock bash run_both.sh <armA sha256> <armB sha256>
# The A/A calibration FIRST, then the B/A runtime legs (declaration P4); stops at the first failure.
set -o pipefail
H=$(dirname "$(readlink -f "$0")")
bash "$H/run_aa.sh" "${1:?armA}" || { echo "AA_FAILED"; exit 1; }
bash "$H/run_all.sh" "${2:?armB}" "$1" || { echo "RUNTIME_FAILED"; exit 1; }
echo "RUNTIME_BOTH_DONE"
