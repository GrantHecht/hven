#!/bin/bash
# W5 T8.9r-attrib4 fold proof. $1 = fail|pass. Sources the SAME common.sh every
# timed leg of this artifact sources, so the proof exercises the SAME fold and
# run code the legs use; only the stubbed steps' exit statuses differ.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra4
. $R/common.sh
MODE=${1:?fail|pass}
LOG=$R/logs/F1-fold-proof-$MODE.log
if [ "$MODE" = fail ]; then S3=2; S5=5; S6=3; else S3=0; S5=0; S6=0; fi
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/fold-proof-$MODE.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) fold proof mode=$MODE"
echo "-- step 1: a step that SUCCEEDS (control, rc 0)"
run /bin/true
echo "   LEG_RC after step 1 = $LEG_RC"
echo "-- step 2: a timed_run of a SUCCEEDING command, through the leg's own wrapper"
timed_run "foldproof/ok" /dev/null -- /bin/true
echo "   LEG_RC after step 2 = $LEG_RC"
echo "-- step 3: the SOLVE step, stubbed rc $S3, through timed_run (the wrapper the legs use)"
timed_run "foldproof/stub" /dev/null -- bash -c "echo 'STUB SOLVE rc $S3' >&2; exit $S3"
echo "   LEG_RC after step 3 = $LEG_RC"
echo "-- step 4: a SUCCEEDING step, to prove a later rc 0 does NOT clear the fold"
run /bin/true
echo "   LEG_RC after step 4 = $LEG_RC"
echo "-- step 5: a PIPED step, stubbed rc $S5 in the FIRST stage, tail rc 0"
bash -c "echo 'STUB PIPED STEP rc $S5' >&2; exit $S5" 2>&1 | tail -1
fold ${PIPESTATUS[0]}
echo "   LEG_RC after step 5 = $LEG_RC   (PIPESTATUS[0] folded, not the tail's 0)"
echo "-- step 6: a LATER step with a SMALLER non-zero rc $S6: worst-fold, not last-fold"
run bash -c "exit $S6"
echo "   LEG_RC after step 6 = $LEG_RC"
echo "FOREGROUND_END $(utc) fold proof mode=$MODE"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
