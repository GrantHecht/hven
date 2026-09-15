#!/bin/bash
# Driver: the pgrep audit OUTSIDE the lock, then the batch UNDER the lock.
# $1 = leg script basename   $2.. = its args ; $LABEL names the pgrep/log pair.
set -o pipefail
R=/home/ghecht/Projects/hven/.scratch/w5t89ra2
LEG=$1; LABEL=$2; shift 2
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' > $R/logs/$LABEL.pgrep || echo PGREP_EMPTY > $R/logs/$LABEL.pgrep
flock /tmp/box-build.lock bash $R/$LEG "$@"
rc=$?; echo "WRAPPER_EXIT=$rc"; exit $rc
