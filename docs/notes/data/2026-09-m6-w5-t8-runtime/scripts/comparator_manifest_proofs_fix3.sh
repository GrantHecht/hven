#!/bin/bash
# W5 T8.9r fix3 -- THE MANIFEST BY IDENTITY, PROVED (settler ruling R14; astra's
# fix2 review, item 3).
#
# astra's item 3: the fix2 manifest declared leg 1's population as THE NUMBER 27,
# so replacing a required key CONSISTENTLY with an undeclared key still exited 0;
# removing a required cell from ONE ROUND exited 2 rather than the manifest's 1;
# and removing the whole `perf/interior_cells` tree exited 2, bypassing the
# explicit member check inside the block its own absence skipped.
#
# THE THREE STUBS THE RULING NAMES, plus the control and the fix2 regressions:
#   STUB A  a required leg-1 cell id SUBSTITUTED for an undeclared one,
#           consistently across every file of the mode            -> exit 1
#   STUB B  a required leg-1 cell removed from ONE ROUND only     -> exit 1
#   STUB C  the whole `perf/interior_cells` directory removed     -> exit 1
#   STUB D  an interior arm's CSVs SUBSTITUTED for another arm's  -> exit 1
#   CONTROL the untouched tree                                    -> exit 0, and
#           its output BYTE-IDENTICAL to the fix2 tool's on the same tree, which
#           is what lets reading.md section 9 stand unrewritten.
#   REGRESSIONS  the fix2 stubs (missing leg-1 perf cell, missing leg-2
#           combination, missing round, missing interior per-cell population)
#           still fire.
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
W=${W:-/home/ghecht/Projects/hven/.scratch/w5t89r/fix3/proofwork}
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

say "CONTROL, THE SECOND HALF -- the identity manifest changed NO NUMBER"
echo "The fix2 tool (git HEAD before this commit) and the fix3 tool, on the same"
echo "tree, must produce BYTE-IDENTICAL output -- which is what lets reading.md"
echo "section 9 stand unrewritten."
git -C /home/ghecht/Projects/hven show HEAD:docs/notes/data/2026-09-m6-w5-t8-runtime/comparator.py > $W/comparator-head.py
rc=$?; fold $rc
python3 -B $W/comparator-head.py --root $ART --out $W/head-out.md
rc=$?; fold $rc
echo "   fix2 tool exit=$rc"
if cmp $W/head-out.md $W/control-out.md; then echo "   BYTE-IDENTICAL: PROVED"; else echo "   NOT IDENTICAL"; fold 1; fi
echo "   and against the section 9 text itself:"
python3 - "$ART/reading.md" "$W/control-out.md" <<PYEOF
import sys
lines = open(sys.argv[1]).read().split("\n")
i = [k for k, l in enumerate(lines) if l.startswith("# W5 T8.9r comparator output")][0]
j = [k for k, l in enumerate(lines) if l.startswith("## 10. Attribution per task")][0]
body = "\n".join(lines[i:j]).rstrip("\n") + "\n"
got = open(sys.argv[2]).read()
print("   section 9 vs the tool: %s (%d bytes)"
      % ("BYTE-IDENTICAL: PROVED" if body == got else "NOT IDENTICAL", len(got)))
sys.exit(0 if body == got else 1)
PYEOF
fold $?

say "STUB A -- A REQUIRED LEG-1 CELL ID IS SUBSTITUTED (R14, the fix2 hole)"
echo "f7_n5000_bound_warm -> f7_n5000_bound_undeclared, in ALL SIX raw CSVs of"
echo "leg 1 / ipm. The COUNT stays 27, so the fix2 manifest exited 0 and scored"
echo "a corpus nobody declared."
mktree subkey
for f in $W/subkey/raw/leg1/ipm/*.csv; do
    sed -i "s/^f7_n5000_bound_warm,/f7_n5000_bound_undeclared,/" $f
done
echo "   rows per file still: $(grep -vc "^#" $W/subkey/raw/leg1/ipm/base-r1.csv)"
$CMP --root $W/subkey --out $W/subkey-out.md
rc=$?
grep -m3 "MANIFEST" $W/subkey-out.md
expect 1 $rc "R14 stub A (substituted leg-1 cell id)"

say "STUB B -- A REQUIRED LEG-1 CELL IS REMOVED FROM ONE ROUND (R14)"
echo "f7_n20000_bound_warm deleted from raw/leg1/ssn/head-r2.csv ONLY. The fix2"
echo "tool reported this downstream, as a PROBLEM, and exited 2."
mktree rowgone
sed -i "/^f7_n20000_bound_warm,/d" $W/rowgone/raw/leg1/ssn/head-r2.csv
echo "   rows in that file: $(grep -vc "^#" $W/rowgone/raw/leg1/ssn/head-r2.csv) (was 27)"
$CMP --root $W/rowgone --out $W/rowgone-out.md
rc=$?
grep -m3 "MANIFEST" $W/rowgone-out.md
expect 1 $rc "R14 stub B (cell missing from one round)"

say "STUB C -- THE WHOLE perf/interior_cells DIRECTORY IS REMOVED (R14)"
echo "The fix2 tool skipped its own explicit member check when the directory was"
echo "absent, and surfaced the absence downstream as exit 2."
mktree intdir
rm -rf $W/intdir/perf/interior_cells
$CMP --root $W/intdir --out $W/intdir-out.md
rc=$?
grep -m3 "MANIFEST" $W/intdir-out.md
expect 1 $rc "R14 stub C (missing interior_cells directory)"

say "STUB D -- AN INTERIOR ARM IS SUBSTITUTED FOR ANOTHER (R14, arms by sha)"
echo "head-r1/r2/r3.csv copied over arm102-r1/r2/r3.csv. Same file count, same"
echo "rounds; a manifest by count cannot see it."
mktree armsub
for r in r1 r2 r3; do cp $W/armsub/raw/interior/head-$r.csv $W/armsub/raw/interior/arm102-$r.csv; done
$CMP --root $W/armsub --out $W/armsub-out.md
rc=$?
grep -m3 "MANIFEST" $W/armsub-out.md
expect 1 $rc "R14 stub D (substituted interior arm)"

say "REGRESSIONS -- the fix2 stubs still fire"
mktree cellgone
rm -f $W/cellgone/perf/leg1/ipm/*f7_n5000_bound_neutral*
$CMP --root $W/cellgone --out $W/cellgone-out.md
rc=$?
grep -m1 "MANIFEST" $W/cellgone-out.md
expect 1 $rc "R9 stub 1 (missing leg-1 perf cell)"

mktree combogone
rm -rf $W/combogone/raw/leg2/ipm/off $W/combogone/perf/leg2/ipm/off
$CMP --root $W/combogone --out $W/combogone-out.md
rc=$?
grep -m1 "MANIFEST" $W/combogone-out.md
expect 1 $rc "R9 stub 2 (missing leg-2 combination)"

mktree roundgone
rm -f $W/roundgone/raw/leg1/ipm/head-r3.csv
$CMP --root $W/roundgone --out $W/roundgone-out.md
rc=$?
grep -m1 "MANIFEST" $W/roundgone-out.md
expect 1 $rc "fix1 I5(d) (missing round)"

mktree intcell
rm -rf $W/intcell/perf/interior_cells/f7_n2000_bound_physics
$CMP --root $W/intcell --out $W/intcell-out.md
rc=$?
grep -m1 "MANIFEST" $W/intcell-out.md
expect 1 $rc "R9 stub 3 (missing interior per-cell population)"

echo
echo "FOREGROUND_END $(utc)"
echo "LEG_RC=$LEG_RC"
exit $LEG_RC
'
rc=$?
echo "WRAPPER_EXIT=$rc"
exit $rc
