#!/bin/bash
# The fold proof: the SAME common.sh the legs source, shown to end at the WORST
# status and never at the last one.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra5
. $R/common.sh
echo "FOLD PROOF $(utc)"
run true;            echo "after true            LEG_RC=$LEG_RC  (expect 0)"
run bash -c 'exit 7'; echo "after exit 7          LEG_RC=$LEG_RC  (expect 7)"
run true;            echo "after true            LEG_RC=$LEG_RC  (expect 7 -- NOT reset by a later success)"
run bash -c 'exit 3'; echo "after exit 3          LEG_RC=$LEG_RC  (expect 7 -- a SMALLER status never lowers it)"
run bash -c 'exit 9'; echo "after exit 9          LEG_RC=$LEG_RC  (expect 9)"
rc=$LEG_RC; echo "WRAPPER_EXIT=$rc"; exit $rc
