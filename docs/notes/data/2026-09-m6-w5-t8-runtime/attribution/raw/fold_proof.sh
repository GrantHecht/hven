#!/bin/bash
# W5 T8.9r-attrib fold proof. $1 = fail|pass. Sources the SAME common.sh every
# arm leg sources; the only difference is the stubbed steps' exit statuses.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra
. $R/common.sh
MODE=${1:?fail|pass}
LOG=$R/logs/A0-fold-proof-$MODE.log
if [ "$MODE" = fail ]; then S3=2; S5=5; S6=3; else S3=0; S5=0; S6=0; fi
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock, verbatim:"
cat $R/logs/A0-pgrep-separate.txt
echo "--- inside lock ---"
box_pgrep
echo "FOREGROUND_START $(utc)"
echo "-- step 1: a step that SUCCEEDS (control, rc 0)"
run /bin/true
echo "   LEG_RC after step 1 = $LEG_RC"
echo "-- step 2: a perf stat invocation that SUCCEEDS (control, rc 0)"
run perf stat -e instructions:u,branches:u,cycles:u -- /bin/true
echo "   LEG_RC after step 2 = $LEG_RC"
echo "-- step 3: the BUILD step, stubbed rc $S3"
run bash -c "echo 'STUB BUILD STEP rc $S3' >&2; exit $S3"
echo "   LEG_RC after step 3 = $LEG_RC"
echo "-- step 4: a SUCCEEDING step, to prove a later rc 0 does NOT clear the fold"
run /bin/true
echo "   LEG_RC after step 4 = $LEG_RC"
echo "-- step 5: a PIPED perf step, stubbed rc $S5 in the FIRST stage, tail rc 0"
bash -c "echo 'STUB PERF STEP rc $S5' >&2; exit $S5" 2>&1 | tail -1
fold ${PIPESTATUS[0]}
echo "   LEG_RC after step 5 = $LEG_RC   (PIPESTATUS[0] folded, not the tail's 0)"
echo "-- step 6: a LATER step with a SMALLER non-zero rc $S6, to prove worst-fold is not last-fold"
run bash -c "exit $S6"
echo "   LEG_RC after step 6 = $LEG_RC"
echo "FOREGROUND_END $(utc)"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
