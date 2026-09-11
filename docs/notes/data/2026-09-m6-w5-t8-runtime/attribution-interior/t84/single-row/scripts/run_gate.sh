#!/bin/bash
# Driver: pgrep OUTSIDE the lock, then the gate leg UNDER the lock.
set -o pipefail
R=/home/ghecht/Projects/hven/.scratch/w5t89ra5
NN=$1 CFG=$2 ARM=$3 SUITE=$4
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' > $R/logs/G$NN-pgrep-separate.txt || echo PGREP_EMPTY > $R/logs/G$NN-pgrep-separate.txt
flock /tmp/box-build.lock bash $R/gate_leg.sh $NN $CFG $ARM $SUITE
rc=$?; echo "WRAPPER_EXIT=$rc"; exit $rc
