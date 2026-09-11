#!/bin/bash
# W5 T8.9r-attrib5, commit 1's gates: ONE (config, arm) leg -- build, ctest -N,
# libhven.a sha256, and (AFTER arm only) the full suite.
# $1 = NN  $2 = config (rel|dbg)  $3 = arm (before|after)  $4 = run-suite (0|1)
# Invoked ALWAYS as: flock /tmp/box-build.lock bash gate_leg.sh NN CFG ARM SUITE
cd / && cd /home/ghecht/Projects/hven || exit 70
R=/home/ghecht/Projects/hven/.scratch/w5t89ra5
. $R/common.sh
NN=${1:?NN} CFG=${2:?cfg} ARM=${3:?arm} SUITE=${4:?suite}
LOG=$R/logs/G$NN-$CFG-$ARM.log
if [ "$CFG" = rel ]; then BD=/home/ghecht/Projects/hven/build; else BD=/home/ghecht/Projects/hven/build-debug; fi
# The >600 s Debug test, excluded from the timed suite run and named in the log.
LONG='Continuation.ProbeBudgetBoundsAFailingProposal'
{
echo "GATE $NN  config=$CFG  arm=$ARM  suite=$SUITE  build_dir=$BD"
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock, verbatim:"
cat $R/logs/G$NN-pgrep-separate.txt
echo "--- inside lock ---"
box_pgrep
echo "FOREGROUND_START $(utc) gate=$NN cfg=$CFG arm=$ARM"
echo "tree: $(git rev-parse HEAD)  dirty-files:"
git status --porcelain
echo "=== build $(utc) ==="
CCACHE_DISABLE=1 cmake --build $BD -j 8 2>&1 | tail -12
fold ${PIPESTATUS[0]}
echo "=== libhven.a sha256 $(utc) ==="
sha256sum $BD/libhven.a
fold $?
echo "=== ctest -N $(utc) ==="
ctest --test-dir $BD -N 2>&1 | tail -3
fold ${PIPESTATUS[0]}
if [ "$SUITE" = 1 ]; then
  echo "=== ctest FULL, minus $LONG $(utc) ==="
  ctest --test-dir $BD -j 4 --output-on-failure -E "^$LONG\$" 2>&1 | tail -25
  fold ${PIPESTATUS[0]}
fi
echo "FOREGROUND_END $(utc) gate=$NN"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
rc=$LEG_RC; echo "WRAPPER_EXIT=$rc"; exit $rc
