#!/bin/bash
# Driver: pgrep OUTSIDE the lock, then the arm leg UNDER the lock, then the
# progress-file row. Always invoked the same way, from the same cwd.
set -o pipefail
R=/home/ghecht/Projects/hven/.scratch/w5t89ra
NN=$1 SHA=$2 LABEL=$3
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' > $R/logs/A$NN-pgrep-separate.txt || echo PGREP_EMPTY > $R/logs/A$NN-pgrep-separate.txt
flock /tmp/box-build.lock bash $R/arm_leg.sh $NN $SHA $LABEL
RC=$?
CB=$(sha256sum $R/bin/$SHA/hven_sqp_corpus 2>/dev/null | cut -c1-64)
LB=$(sha256sum $R/bin/$SHA/libhven.a 2>/dev/null | cut -c1-64)
echo "ARM$NN rc=$RC corpus=$CB lib=$LB"
grep -a -E '^(ENV_FOOTPRINT|SUBMODULE|extracted:|child argv|perf files|FOREGROUND_END|WRAPPER_EXIT)' $R/logs/A$NN-$LABEL.log
exit $RC
