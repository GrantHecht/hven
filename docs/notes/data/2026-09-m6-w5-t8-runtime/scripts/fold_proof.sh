#!/bin/bash
# W5 T8.9r fold proof. $1 = fail|pass. Sources the SAME common.sh the timed
# legs source; the only difference is the comparator stub's exit status.
R=/home/ghecht/Projects/hven/.scratch/w5t89r
. $R/common.sh
MODE=${1:?fail|pass}
LOG=$R/logs/L2-fold-proof-$MODE.log
{
echo "PGREP_SEPARATE $(utc)"
cat $R/logs/L2-pgrep-separate.txt
box_pgrep
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
