#!/bin/bash
# W5 T8.9r fix2 -- THE R2' RE-AUDIT, as it ran.
#
# Runs ONE idle_proof.py -- the attrib4 (nice-inclusive) version, now byte-
# identical in all three places it sits -- over the retained batch logs of every
# round, then assembles logs/IDLE-PROOF.md with idle_audit_fix2.py.
#
# NOTHING HERE IS TIMED AND NOTHING HERE RUNS A SOLVER. It is arithmetic on
# snapshots that were already on disk. It takes the box lock anyway, and takes
# the pgrep audit both outside and inside it, because the box protocol is the
# protocol whether or not a given command is a measurement.
#
# ON THE FOLD. `idle_proof.py` exits 1 when ANY batch is UNPROVEN, and after the
# amendment that is an expected AUDIT FINDING, not a failure of this script --
# four wall batches and eleven counter batches of the retained rounds do not
# meet R2' and the whole point of the audit is to say so. So each round's status
# is RECORDED and EXPECTED, and only the assembler's status and the copy checks
# are folded. That is declared here rather than hidden in a `|| true`.
set -o pipefail
LEG_RC=0
fold() { local rc=$1; if [ "$rc" -gt "$LEG_RC" ]; then LEG_RC=$rc; fi; return 0; }
run() { "$@"; fold $?; return 0; }
utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }

ART=${ART:-/home/ghecht/Projects/hven/docs/notes/data/2026-09-m6-w5-t8-runtime}
W=${W:-/home/ghecht/Projects/hven/.scratch/w5t89r/fix2}
LOCK=/tmp/box-build.lock
IP=$ART/scripts/idle_proof.py
mkdir -p $W

echo "PGREP_SEPARATE $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang'
echo "PGREP_SEPARATE_END rc=$?"

flock $LOCK bash -c '
set -o pipefail
LEG_RC=0
fold() { local rc=$1; if [ "$rc" -gt "$LEG_RC" ]; then LEG_RC=$rc; fi; return 0; }
run() { "$@"; fold $?; return 0; }
utc() { date -u +%Y-%m-%dT%H:%M:%SZ; }
ART='"$ART"'; W='"$W"'; IP='"$IP"'

echo "PGREP_INLOCK $(utc)"
pgrep -af "cmake|ninja|ctest|hven_|codex|clang"
echo "PGREP_INLOCK_END rc=$?"
echo "FOREGROUND_START $(utc)  (no solver runs here; nothing is timed)"

echo "THE ONE TOOL, in all three places:"
sha256sum $ART/scripts/idle_proof.py \
          $ART/attribution-interior/t84/scripts/idle_proof.py \
          $ART/attribution-interior/t84/redraw/scripts/idle_proof.py
n=$(sha256sum $ART/scripts/idle_proof.py \
              $ART/attribution-interior/t84/scripts/idle_proof.py \
              $ART/attribution-interior/t84/redraw/scripts/idle_proof.py \
    | cut -d" " -f1 | sort -u | wc -l)
echo "distinct hashes = $n (expected 1)"
[ "$n" -eq 1 ] || fold 1

round() {   # round <label> <logs...>
    local label=$1; shift
    echo
    echo "ROUND $label  ($# logs)"
    python3 -B $IP "$@" $W/round-$label.md
    local rc=$?
    echo "  idle_proof exit=$rc  (EXPECTED: 1 where any batch of the round is UNPROVEN)"
    echo "  batches: $(grep -c "^| .*-r\?[0-9]*\.log |" $W/round-$label.md) table rows"
}

round fix1                $ART/logs/L4-*.log $ART/logs/L5-*.log $ART/logs/L6-*.log \
                          $ART/logs/L7-*.log $ART/logs/L8-*.log
round attrib2             $ART/attribution-interior/raw/logs/*.log
round attrib3             $ART/attribution-interior/t84/logs/*.log
round attrib3-superseded  $ART/attribution-interior/t84/raw/round1-superseded-argv/*.log
round attrib4             $ART/attribution-interior/t84/redraw/logs/*.log

echo
echo "ROUNDS WITH NO RETAINED SNAPSHOTS (declared, not audited):"
echo "  round 1 superseded:  $(grep -lc "^BATCH_START" $ART/logs/round1-superseded/*.log 2>/dev/null | wc -l) of $(ls $ART/logs/round1-superseded/*.log | wc -l) logs carry a BATCH_START"
echo "  attrib5 single-row:  $(grep -lc "^BATCH_START" $ART/attribution-interior/t84/single-row/logs/*.log 2>/dev/null | wc -l) of $(ls $ART/attribution-interior/t84/single-row/logs/*.log | wc -l) logs carry a BATCH_START"

echo
echo "ASSEMBLING $ART/logs/IDLE-PROOF.md"
run python3 -B $ART/scripts/idle_audit_fix2.py \
    fix1=$W/round-fix1.md \
    attrib2=$W/round-attrib2.md \
    attrib3=$W/round-attrib3.md \
    attrib3-superseded=$W/round-attrib3-superseded.md \
    attrib4=$W/round-attrib4.md \
    $ART/logs/IDLE-PROOF.md
echo "  assembler LEG_RC=$LEG_RC"
echo "FOREGROUND_END $(utc)"
exit $LEG_RC
'
rc=$?
echo "WRAPPER_EXIT=$rc"
exit $rc
