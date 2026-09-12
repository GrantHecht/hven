#!/bin/bash
# W5 T8.9r fix3 -- THE WHOLE-ARTIFACT REPRODUCTION CHECK, as it ran.
#
# Re-derives, from the artifact alone, every saved output fix3 touched or relies
# on, and compares byte for byte:
#
#   comparator.py           -> reading.md section 9 (66 702 bytes)
#   record_analyze.py       -> record_analyze.out  (now from perf-report/, not scratch)
#   wall_bracket.py         -> wall_bracket.out    (new at fix3)
#   classify.py             -> classify.out        (untouched; checked anyway)
#   the three idle_proof.py copies -> one sha256
#   every .sha256 pin beside its file
#
# It does NOT re-run attribute_interior.py or attribution/attribute.py: neither
# their code nor their inputs changed at fix3, and both write CSVs beside their
# stdout, so re-running them in place would rewrite committed artifacts. astra's
# fix1 and fix2 reviews reproduced both with their writes intercepted.
#
# NOTHING HERE RUNS A SOLVER AND NOTHING IS TIMED. The box protocol is followed
# anyway: the pgrep audit separately and again inside the lock,
# FOREGROUND_START/END inside the lock, pipefail, worst-fold, and
# `rc=$?; echo WRAPPER_EXIT=$rc; exit $rc` on the outside.
set -o pipefail
LEG_RC=0
fold() { local rc=$1; if [ "$rc" -gt "$LEG_RC" ]; then LEG_RC=$rc; fi; return 0; }
utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }

ART=${ART:-/home/ghecht/Projects/hven/docs/notes/data/2026-09-m6-w5-t8-runtime}
W=${W:-/home/ghecht/Projects/hven/.scratch/w5t89r/fix3/verify}
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
same() {  # same <label> <saved> <fresh>
    if cmp -s "$2" "$3"; then echo "   $1: BYTE-IDENTICAL ($(wc -c < "$3") bytes)"
    else echo "   $1: DIFFERS"; diff "$2" "$3" | head -20; fold 1; fi
}

echo "PGREP_INLOCK $(utc)"
pgrep -af "cmake|ninja|ctest|hven_|codex|clang"
echo "PGREP_INLOCK_END rc=$?"
echo "FOREGROUND_START $(utc)  (no solver runs here; nothing is timed)"

echo
echo "== the one idle_proof.py, in all three places"
sha256sum $ART/scripts/idle_proof.py \
          $ART/attribution-interior/t84/scripts/idle_proof.py \
          $ART/attribution-interior/t84/redraw/scripts/idle_proof.py
n=$(sha256sum $ART/scripts/idle_proof.py \
              $ART/attribution-interior/t84/scripts/idle_proof.py \
              $ART/attribution-interior/t84/redraw/scripts/idle_proof.py \
    | cut -d" " -f1 | sort -u | wc -l)
echo "   distinct hashes = $n (expected 1)"
[ "$n" -eq 1 ] || fold 1
echo "== and every .sha256 beside its file"
echo "   (some of these pins are BARE HASHES and some are sha256sum format; both"
echo "    are retained as written -- the first field is compared, not the line.)"
for f in $ART/scripts/idle_proof.py \
         $ART/comparator.py \
         $ART/attribution-interior/attribute_interior.py \
         $ART/attribution-interior/t84/single-row/classify.py \
         $ART/attribution-interior/t84/single-row/record_analyze.py \
         $ART/attribution-interior/t84/single-row/wall_bracket.py; do
    want=$(cut -d" " -f1 < $f.sha256)
    got=$(sha256sum $f | cut -d" " -f1)
    if [ "$want" = "$got" ]; then echo "   $(basename $f): OK"
    else echo "   $(basename $f): PIN MISMATCH (pin $want, file $got)"; fold 1; fi
done

echo
echo "== comparator.py -> reading.md section 9"
python3 -B $ART/comparator.py --root $ART --out $W/cmp.out
rc=$?; fold $rc; echo "   comparator exit=$rc (expected 0)"
python3 - "$ART/reading.md" "$W/sec9.txt" <<PYEOF
import sys
lines = open(sys.argv[1]).read().split("\n")
i = [k for k, l in enumerate(lines) if l.startswith("# W5 T8.9r comparator output")][0]
j = [k for k, l in enumerate(lines) if l.startswith("## 10. Attribution per task")][0]
open(sys.argv[2], "w").write("\n".join(lines[i:j]).rstrip("\n") + "\n")
PYEOF
fold $?
same "reading.md section 9" $W/sec9.txt $W/cmp.out

echo
echo "== record_analyze.py -> record_analyze.out (inputs now inside the artifact)"
( cd $ART/attribution-interior/t84/single-row && python3 -B record_analyze.py > $W/ra.out )
rc=$?; fold $rc
same "record_analyze.out" $ART/attribution-interior/t84/single-row/record_analyze.out $W/ra.out

echo
echo "== wall_bracket.py -> wall_bracket.out"
( cd $ART/attribution-interior/t84/single-row && python3 -B wall_bracket.py > $W/wb.out )
rc=$?; fold $rc
same "wall_bracket.out" $ART/attribution-interior/t84/single-row/wall_bracket.out $W/wb.out

echo
echo "== classify.py -> classify.out"
( cd $ART/attribution-interior/t84/single-row && python3 -B classify.py > $W/cl.out 2>&1 )
rc=$?; fold $rc
same "classify.out" $ART/attribution-interior/t84/single-row/classify.out $W/cl.out

echo
echo "FOREGROUND_END $(utc)"
echo "LEG_RC=$LEG_RC"
exit $LEG_RC
'
rc=$?
echo "WRAPPER_EXIT=$rc"
exit $rc
