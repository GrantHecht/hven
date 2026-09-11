#!/bin/bash
# W5 T8.9r fix1 fold proof. $1 = fail|pass. Sources the SAME common.sh the timed
# legs source; the only difference is the comparator stub's exit status.
R=/home/ghecht/Projects/hven/.scratch/w5t89r
. $R/fix1/common.sh
MODE=${1:?fail|pass}
LOG=$R/logs2/L2-fold-proof-$MODE.log
mkdir -p $R/logs2
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs2/fold-proof-$MODE.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc)"
echo "-- step 1: a harness invocation that SUCCEEDS (control, rc 0)"
run /bin/true
echo "   LEG_RC after step 1 = $LEG_RC"
echo "-- step 2: a perf stat invocation that SUCCEEDS (control, rc 0)"
run perf stat -e instructions:u -- /bin/true
echo "   LEG_RC after step 2 = $LEG_RC"
echo "-- step 3: the COMPARATOR, stubbed $MODE"
if [ "$MODE" = fail ]; then
    run bash -c 'echo "STUB COMPARATOR: deliberate failure" >&2; exit 3'
else
    run bash -c 'echo "STUB COMPARATOR: control pass"; exit 0'
fi
echo "   LEG_RC after step 3 = $LEG_RC"
echo "-- step 4: a further SUCCEEDING step, to prove a later rc 0 does NOT clear the fold"
run /bin/true
echo "   LEG_RC after step 4 = $LEG_RC"
echo "FOREGROUND_END $(utc)"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
