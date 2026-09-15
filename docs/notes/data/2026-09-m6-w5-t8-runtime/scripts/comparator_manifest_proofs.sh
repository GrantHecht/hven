#!/bin/bash
# W5 T8.9r fix2 -- THE EXPLICIT MANIFEST, PROVED (settler ruling R9; astra's
# fix1 review item 4).
#
# astra's item 4: the fix1 manifest DISCOVERED its required populations from the
# files on disk, so omitting a whole leg-1 perf cell, or the whole
# `leg2/ipm/off` combination, still returned exit 0 WITHOUT reporting the
# omission. Only holes INSIDE a discovered population (a missing round) fired.
#
# Two stubs prove the fix, beside the control:
#   STUB 1  a whole leg-1 perf CELL is removed        -> exit 1, MANIFEST names it
#   STUB 2  a whole leg-2 COMBINATION is removed      -> exit 1, MANIFEST names it
#   REGRESSION  the fix1 missing-ROUND stub still fires (the old check is intact)
#   CONTROL the untouched tree                        -> exit 0, and its output is
#           BYTE-IDENTICAL to the pre-manifest tool's, which is what says the
#           manifest changed no number and reading.md section 9 still stands.
#
# Every stub is a reflink COPY of the artifact's raw/ and perf/ under .scratch,
# mutated in exactly one way. NOTHING HERE RUNS A SOLVER AND NOTHING IS TIMED.
# The box protocol is followed anyway: the pgrep audit separately and again
# inside the lock, FOREGROUND_START/END inside the lock, pipefail, worst-fold,
# and `rc=$?; echo WRAPPER_EXIT=$rc; exit $rc` on the outside.
set -o pipefail
LEG_RC=0
fold() { local rc=$1; if [ "$rc" -gt "$LEG_RC" ]; then LEG_RC=$rc; fi; return 0; }
utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }

ART=${ART:-/home/ghecht/Projects/hven/docs/notes/data/2026-09-m6-w5-t8-runtime}
W=${W:-/home/ghecht/Projects/hven/.scratch/w5t89r/fix2/proofwork}
LOCK=/tmp/box-build.lock
mkdir -p $W

echo "PGREP_SEPARATE $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang'
echo "PGREP_SEPARATE_END rc=$?"

flock $LOCK bash -c '
set -o pipefail
LEG_RC=0
fold() { local rc=$1; if [ "$rc" -gt "$LEG_RC" ]; then LEG_RC=$rc; fi; return 0; }
utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }
ART='"$ART"'; W='"$W"'
CMP="python3 -B $ART/comparator.py"
say() { echo; echo "=============================================================="; echo "$*"; echo "=============================================================="; }
mktree() { rm -rf $W/$1; mkdir -p $W/$1; for d in raw perf; do cp -a --reflink=auto $ART/$d $W/$1/$d; done; }
expect() {  # expect <want-rc> <got-rc> <what>
    if [ "$2" -eq "$1" ]; then echo "   $3: PROVED (exit $2)"; else echo "   $3: NOT PROVED (exit $2, wanted $1)"; fold 1; fi
}

echo "PGREP_INLOCK $(utc)"
pgrep -af "cmake|ninja|ctest|hven_|codex|clang"
echo "PGREP_INLOCK_END rc=$?"
echo "FOREGROUND_START $(utc)  (no solver runs here; nothing is timed)"
echo "comparator under proof:"
sha256sum $ART/comparator.py

say "CONTROL -- the real tree must PASS (exit 0)"
$CMP --root $ART --out $W/control-out.md
rc=$?; fold $rc
echo "   CONTROL exit=$rc (expected 0); output bytes: $(wc -c < $W/control-out.md)"

say "CONTROL, THE SECOND HALF -- the manifest changed NO NUMBER"
echo "The pre-manifest tool (git HEAD before this commit) and the manifest tool,"
echo "on the same tree, must produce BYTE-IDENTICAL output -- which is what lets"
echo "reading.md section 9 stand unrewritten."
git -C /home/ghecht/Projects/hven show HEAD:docs/notes/data/2026-09-m6-w5-t8-runtime/comparator.py > $W/comparator-head.py
rc=$?; fold $rc
python3 -B $W/comparator-head.py --root $ART --out $W/head-out.md
rc=$?; fold $rc
echo "   pre-manifest tool exit=$rc"
if cmp $W/head-out.md $W/control-out.md; then echo "   BYTE-IDENTICAL: PROVED"; else echo "   NOT IDENTICAL"; fold 1; fi

say "STUB 1 -- A WHOLE LEG-1 PERF CELL IS REMOVED (astra item 4, first half)"
echo "Removed: perf/leg1/ipm/*f7_n5000_bound_neutral* (all 12 captures, both"
echo "passes, both arms, three rounds). Round 1 discovered its cell list from"
echo "this directory, so it saw a two-cell population and exited 0."
mktree cellgone
rm -f $W/cellgone/perf/leg1/ipm/*f7_n5000_bound_neutral*
echo "   captures left in perf/leg1/ipm: $(ls $W/cellgone/perf/leg1/ipm | wc -l) (was $(ls $ART/perf/leg1/ipm | wc -l))"
$CMP --root $W/cellgone --out $W/cellgone-out.md
rc=$?
grep -m3 "MANIFEST" $W/cellgone-out.md
expect 1 $rc "R9 stub 1 (missing leg-1 perf cell)"

say "STUB 2 -- A WHOLE LEG-2 COMBINATION IS REMOVED (astra item 4, second half)"
echo "Removed: raw/leg2/ipm/off and perf/leg2/ipm/off entirely. Round 1 walked"
echo "raw/leg2/ to learn its combinations, so it scored five and exited 0."
mktree combogone
rm -rf $W/combogone/raw/leg2/ipm/off $W/combogone/perf/leg2/ipm/off
echo "   combinations left: $(ls -d $W/combogone/raw/leg2/*/* | wc -l) (was $(ls -d $ART/raw/leg2/*/* | wc -l))"
$CMP --root $W/combogone --out $W/combogone-out.md
rc=$?
grep -m3 "MANIFEST" $W/combogone-out.md
expect 1 $rc "R9 stub 2 (missing leg-2 combination)"

say "REGRESSION -- the fix1 missing-ROUND check still fires"
mktree roundgone
rm -f $W/roundgone/raw/leg1/ipm/head-r3.csv
$CMP --root $W/roundgone --out $W/roundgone-out.md
rc=$?
grep -m2 "MANIFEST" $W/roundgone-out.md
expect 1 $rc "fix1 I5(d) (missing round), re-checked under the new manifest"

say "AND A MISSING INTERIOR CELL, for the same reason"
mktree intcell
rm -rf $W/intcell/perf/interior_cells/f7_n2000_bound_physics
$CMP --root $W/intcell --out $W/intcell-out.md
rc=$?
grep -m2 "MANIFEST" $W/intcell-out.md
expect 1 $rc "R9 stub 3 (missing interior per-cell population)"

echo
echo "FOREGROUND_END $(utc)"
echo "LEG_RC=$LEG_RC"
exit $LEG_RC
'
rc=$?
echo "WRAPPER_EXIT=$rc"
exit $rc
