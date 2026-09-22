#!/usr/bin/env bash
# M7 W2 M5 -- THE DEGENERACY-ARM CHAIN. Detached with `setsid`; nothing polls it.
#
# Governed by ../RULE.md, committed at 0047c593 and amended at ae4d716b BOTH
# BEFORE this chain's first leg, and never edited after it. Every term below is
# that rule's section 8; every stop is its section 7; the order is section 8's
# and it is not cosmetic.
#
#   STEP 0  THE PINS -- the probe binary's sha256, the thirteen COPIED OFF rows
#           against their own manifest AND against M4's committed originals,
#           the YORKNET parent trace against M4b's parent manifest, and the
#           cell roster read out of the harness's own source. A failure here
#           stops everything BEFORE any leg.
#   STEP 1  THE OFF CONTROLS ARE NOT RE-RUN (section 8 step 1). M4's ten OFF
#           rows and its three neutrality OFF rows are REUSED BY COPY; the
#           smoke at 152fe98b already proved this binary reproduces M3c's
#           committed ACOPP57 OFF row on every field, by bytes.
#   STEP 2  A6's OFFLINE half, from the committed/pinned YORKNET trace, BEFORE
#           the dynamic leg -- so the prediction is on the record before the
#           solver moves.
#   STEP 3  ARM a on YORKNET, alone.
#   STEP 4  ARM b1 on the EIGHT.
#   STEP 5  ARM a+b1 on the SAME EIGHT.
#   STEP 6  ARM a+b1 on BRITGAS and YORKNET.
#   STEP 7  NEUTRALITY, DESCRIPTIVE AND EXTENDED (A7): READING6, SMBANK,
#           ZAMB2-8 under a, b1 and a+b1, with FEASIBILITY and STATIONARITY
#           beside status. DESCRIPTIVE: this chain reports; it does not grade.
#
# ONE SOLVER AT A TIME. The box lock is taken per leg and released between legs;
# `pgrep` runs as its own command BEFORE the lock and again INSIDE it. No leg
# signals a process it did not start. Wall-clock is informational (CLAUDE.md
# section 7); the deadlines here are STOPS.
#
# `a+b1` CARRIES A `+`. Every grep for an arm label in this file is `grep -F`
# or `grep -qx`, never an ERE: an ERE would read the `+` as a repetition
# operator and the arm whose whole purpose is to be distinguished from `b1`
# would match nothing.
set -o pipefail

# THE ONE SCRATCH LITERAL IS `$S` BELOW, and `--preflight-only` is what proves
# it RESOLVES to the tree this chain means before any leg runs. The pre-flight
# judges by the VALUE each path resolves to -- the binary's sha, the rows'
# shas, the trace's sha, the roster read out of the harness -- and not by
# grepping this file's prose. It stops before STEP 2, so no solver runs and no
# trace is read.
PREFLIGHT_ONLY=0
[ "$1" = "--preflight-only" ] && PREFLIGHT_ONLY=1

ROOT=/home/ghecht/Projects/hven
D="$ROOT/docs/notes/data/2026-09-m7-w2-m5-degeneracy-arms"
M4="$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm"
M4B="$ROOT/docs/notes/data/2026-09-m7-w2-m4b-expand-on-the-ten"
S="$ROOT/.scratch/m7w2m5arm"
EXE="$S/build/m5_arm_probe"
EXE_SHA=eac445c0256e1b0af739f98e91a5d1cce586fedfa41562b7fb08684b145b43d4
BENCH=/home/ghecht/Projects/hven-bench
PARENT_TRACES="$ROOT/.scratch/m7w2m4/arm/traces"
LOG="$S/logs"

# THE ROSTERS. Spelled once, checked against the harness's own list below.
TEN="ACOPP57 DALLASM STEENBRB ACOPP118 ACOPR57 STEENBRC STEENBRG YORKNET SAWPATH BRITGAS"
EIGHT="ACOPP118 ACOPR57 DALLASM STEENBRB STEENBRC STEENBRG SAWPATH ACOPP57"
NEUTRAL="READING6 SMBANK ZAMB2-8"
A_CELLS="YORKNET"
AB_EXTRA="BRITGAS YORKNET"

mkdir -p "$S/rows" "$S/traces" "$LOG" "$S/a6"

echo "CHAIN_START $(date -Is)"
echo "HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
echo "RULE  $D/RULE.md  (committed 0047c593, AMENDMENT 1 ae4d716b, both before this chain)"
echo "EXE   $EXE"

for p in "$EXE" "$D/RULE.md" "$D/rows-off/SHA256-off-rows.txt" \
         "$D/wrappers/a6_offline_m5.py" "$D/wrappers/m4c_tails.py" \
         "$M4B/traces-m4b/SHA256-parent-traces-m4b.txt" \
         "$PARENT_TRACES/off-YORKNET.csv" "$BENCH"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; echo "CHAIN_END $(date -Is) rc=90"; exit 90; }
done

# ---------------------------------------------------------------------------
# STEP 0 -- THE PINS
# ---------------------------------------------------------------------------
echo
echo "=========== STEP 0: THE PINS"

# PIN 1: THE BINARY. Nothing is ever rebuilt inside this chain.
got=$(sha256sum "$EXE" | cut -d' ' -f1)
echo "EXE_SHA256 $got"
if [ "$got" != "$EXE_SHA" ]; then
    echo "REFUSED: the probe is not the binary this chain was written against"
    echo "    expected $EXE_SHA"
    echo "    got      $got"
    echo "CHAIN_END $(date -Is) rc=91"
    exit 91
fi
echo "PIN_BINARY OK -- the smoked probe, unrebuilt"

# PIN 2: THE THIRTEEN COPIED OFF ROWS, against their own manifest AND against
# M4's committed originals. A manifest a copy was generated FROM proves only
# that the copy has not rotted since; the second check is what proves it is
# M4's row (RULE.md section 8 step 1: REUSED BY COPY, never re-run).
echo "--- the thirteen COPIED OFF rows, against their own manifest"
if ( cd "$D/rows-off" && sha256sum -c SHA256-off-rows.txt ) > "$LOG/pin-off-rows.txt" 2>&1; then
    echo "PIN_OFF_ROWS OK -- $(grep -c ': OK$' "$LOG/pin-off-rows.txt") rows verified"
else
    echo "REFUSED: a copied OFF row does not match its manifest"
    grep -v ': OK$' "$LOG/pin-off-rows.txt" | head -5 | sed 's/^/    /'
    echo "CHAIN_END $(date -Is) rc=91"
    exit 91
fi
off_bad=0
for cell in $TEN $NEUTRAL; do
    a=$(sha256sum "$D/rows-off/off-$cell.txt" 2>/dev/null | cut -d' ' -f1)
    b=$(sha256sum "$M4/rows/off-$cell.txt" 2>/dev/null | cut -d' ' -f1)
    if [ -z "$a" ] || [ "$a" != "$b" ]; then
        echo "REFUSED: off-$cell.txt is not byte-identical to M4's committed row"
        off_bad=1
    fi
done
if [ "$off_bad" -ne 0 ]; then echo "CHAIN_END $(date -Is) rc=91"; exit 91; fi
echo "PIN_OFF_ROWS_PROVENANCE OK -- all 13 are M4's committed rows, byte for byte"

# PIN 3: THE YORKNET PARENT TRACE, against M4b's OWN parent manifest. A6's
# offline half reads this file; a file that is not the one M4b pinned would
# make the whole offline check a statement about an unknown walk.
echo "--- the YORKNET parent trace against M4b's parent manifest"
want=$(grep -F "off-YORKNET.csv" "$M4B/traces-m4b/SHA256-parent-traces-m4b.txt" | awk '{print $1}' | head -1)
gotc=$(sha256sum "$PARENT_TRACES/off-YORKNET.csv" | awk '{print $1}')
echo "PINNED_TRACE_SHA256  $want"
echo "MEASURED_TRACE_SHA256 $gotc"
if [ -z "$want" ] || [ "$want" != "$gotc" ]; then
    echo "REFUSED: the YORKNET parent trace is not the one M4b pinned"
    echo "CHAIN_END $(date -Is) rc=91"
    exit 91
fi
echo "PIN_PARENT_TRACE OK"

# PIN 4: THE CELL ROSTER, read out of the harness's OWN source -- the one list
# the probe itself validates against -- rather than re-typed here. READ ONLY:
# nothing under hven-bench is written and its CMake is never entered.
echo "--- the cell roster, checked before any leg"
ROSTER_SRC="$BENCH/lanes/cutest/cutest_source.cpp"
[ -r "$ROSTER_SRC" ] || { echo "REFUSED: cannot read $ROSTER_SRC"; echo "CHAIN_END $(date -Is) rc=92"; exit 92; }
ROSTER=$(sed -n '/kProblems = {/,/};/p' "$ROSTER_SRC" | grep -o '"[A-Za-z0-9-]*"' | tr -d '"')
n_roster=$(echo "$ROSTER" | grep -c .)
if [ "$n_roster" -lt 30 ]; then
    echo "REFUSED: the roster read back only $n_roster name(s) -- the harvest is wrong,"
    echo "         and a roster check that read nothing would pass everything"
    echo "CHAIN_END $(date -Is) rc=92"; exit 92
fi
# NON-VACUITY: a name that CANNOT be in the roster must be rejected by this
# same test, or the test is a no-op.
if echo "$ROSTER" | grep -qx "NOT_A_CELL"; then
    echo "REFUSED: the roster check accepts a name that cannot be in the set"
    echo "CHAIN_END $(date -Is) rc=92"; exit 92
fi
roster_bad=0
for cell in $TEN $EIGHT $NEUTRAL $A_CELLS $AB_EXTRA; do
    echo "$ROSTER" | grep -qx "$cell" || { echo "REFUSED: $cell is not in the roster"; roster_bad=1; }
done
if [ "$roster_bad" -ne 0 ]; then echo "CHAIN_END $(date -Is) rc=92"; exit 92; fi
echo "all cell names are in the $n_roster-name roster"
# AND THE EIGHT MUST BE A SUBSET OF THE TEN, or a b1 cell would have no OFF row
# to be read against.
for cell in $EIGHT; do
    echo "$TEN" | tr ' ' '\n' | grep -qx "$cell" || {
        echo "REFUSED: $cell is in the EIGHT but has no OFF row among the TEN"
        echo "CHAIN_END $(date -Is) rc=92"; exit 92; }
done
echo "the EIGHT are inside the TEN; every armed cell has a copied OFF row"

# ---------------------------------------------------------------------------
# run_leg ARM CELL
#
# RULE.md section 8's terms, in one place so no leg can quietly differ from
# another: DEFAULT options (no --tr-init is ever passed, so the option field is
# never assigned), MKL_NUM_THREADS=1 OMP_NUM_THREADS=1, taskset -c 2, the
# ledger on, the observer armed over EVERY solve, a 600 s solver deadline
# (300 s for SAWPATH, the value M3c and M4 ran it at) inside an outer
# `timeout 900`.
# ---------------------------------------------------------------------------
run_leg() {
    local arm="$1" cell="$2"
    local deadline=600
    [ "$cell" = "SAWPATH" ] && deadline=300
    local row="$S/rows/$arm-$cell.txt"
    local trace="$S/traces/$arm-$cell.csv"
    local err="$LOG/$arm-$cell.err"

    # THE pgrep AS ITS OWN COMMAND, BEFORE THE LOCK.
    if pgrep -a -f "hven_bench_cell|m3c_p2a_probe|m4_arm_probe|m5_arm_probe|hven_sqp_corpus" \
         | grep -v -e "eval " -e pgrep -e chain_m5 > /dev/null; then
        echo "LEG $arm-$cell REFUSED: a foreign cell binary is running BEFORE the lock"
        return 99
    fi

    echo "LEG_START $arm-$cell $(date -Is) deadline=${deadline}s"
    flock /tmp/box-build.lock bash -c '
        set -o pipefail
        arm="$1"; cell="$2"; deadline="$3"
        EXE="$4"; row="$5"; trace="$6"; err="$7"; BENCH="$8"
        if pgrep -a -f "hven_bench_cell|m3c_p2a_probe|m4_arm_probe|m5_arm_probe|hven_sqp_corpus" \
             | grep -v -e "eval " -e pgrep -e "bash -c" -e chain_m5; then
            echo "FOREIGN CELL BINARY UNDER MY LOCK -- REFUSING"
            exit 99
        fi
        echo "PGREP_EMPTY $arm-$cell: no foreign cell binary under my lock"
        cd "$BENCH" || exit 93
        echo "FOREGROUND_START $arm-$cell $(date -Is)"
        MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 "$EXE" \
            --problem "$cell" --arm "$arm" \
            --deadline "$deadline" --ledger on --trace on --trace-out "$trace" \
            --capture-solve -1 --max-records 2000000 > "$row" 2> "$err"
        rc=$?
        echo "FOREGROUND_END $arm-$cell $(date -Is) rc=$rc"
        exit $rc
    ' _ "$arm" "$cell" "$deadline" "$EXE" "$row" "$trace" "$err" "$BENCH"
    local rc=$?
    echo "LEG_END $arm-$cell $(date -Is) rc=$rc"
    if [ "$rc" -ne 0 ]; then
        # RULE.md section 7 item 6 / section 8: a killed leg, or one exiting
        # non-zero for any other reason, is CENSORED. Its ROW IS REMOVED so
        # nothing downstream can read a partial one as a measurement, and
        # nothing is filled in later.
        echo "CENSORED $arm-$cell (rc=$rc) -- nothing is filled in later"
        rm -f "$row"
        head -3 "$err" 2>/dev/null | sed 's/^/    /'
        return 0
    fi
    # THE SWITCH MUST BE REACHABLE, ON EVERY LEG, and the label the leg ran
    # must be the label it was asked for. FIXED-STRING match: `a+b1` carries a
    # `+`.
    if ! grep -qx "arm_compiled_in=1" "$row"; then
        echo "REFUSED: $arm-$cell has no arm_compiled_in=1 -- the switch is not reachable"
        echo "CHAIN_END $(date -Is) rc=94"
        exit 94
    fi
    if ! grep -qxF "arm=$arm" "$row"; then
        echo "REFUSED: $arm-$cell does not print arm=$arm -- the leg's label and its"
        echo "         switch disagree, and no read-out can be trusted past that"
        echo "CHAIN_END $(date -Is) rc=94"
        exit 94
    fi
    # THE PER-LEG SIGHTING LINE. Reported, never graded here: RULE.md section 7
    # is applied in the read-out, over the whole set, not one cell at a time.
    local st df qm mi dis rv lbe cr
    st=$(grep -m1 '^status=' "$row"); df=$(grep -m1 '^deadline_fired=' "$row")
    qm=$(grep -m1 '^qp_minor_iters=' "$row"); mi=$(grep -m1 '^major_iters=' "$row")
    dis=$(grep -m1 '^indep_disarmed=' "$row"); rv=$(grep -m1 '^indep_rank_violations=' "$row")
    lbe=$(grep -m1 '^indep_leak_bound_exceeded=' "$row")
    cr=$(grep -m1 '^indep_max_cond_r=' "$row")
    echo "    $st  $df  $qm  $mi"
    echo "    $dis  $rv  $lbe  $cr"
    # A9/A10's three UNSUPPORTED triggers are SAID at the leg, so the transcript
    # carries them even if a read-out is never written.
    case "$lbe" in
        indep_leak_bound_exceeded=0) ;;
        *) echo "    UNSUPPORTED $arm-$cell -- A10's bound was exceeded; cond(R) is beside it"
           echo "                and EXPLAINS it, never excuses it (RULE.md AMENDMENT 1)" ;;
    esac
    case "$rv" in
        indep_rank_violations=0) ;;
        *) echo "    UNSUPPORTED $arm-$cell -- A9: the arm disarmed on a rank violation" ;;
    esac
    local rmd
    rmd=$(grep -m1 '^indep_refused_mandatory_dependent=' "$row")
    case "$rmd" in
        indep_refused_mandatory_dependent=0) ;;
        *) echo "    UNSUPPORTED $arm-$cell -- A3: $rmd" ;;
    esac
    return 0
}

if [ "$PREFLIGHT_ONLY" -eq 1 ]; then
    echo
    echo "PREFLIGHT_OK: every pin resolved and was judged by its VALUE, before any"
    echo "              leg. S=$S  EXE=$EXE"
    echo "PREFLIGHT_ONLY: stopping before STEP 2. No solver ran and no trace was read."
    echo "CHAIN_END $(date -Is) rc=0"
    exit 0
fi

# ---------------------------------------------------------------------------
echo
echo "=========== STEP 1: THE OFF CONTROLS ARE REUSED BY COPY, NOT RE-RUN"
echo "RULE.md section 8 step 1. The thirteen rows in rows-off/ are M4's own,"
echo "verified above against their manifest AND against M4's committed files."
echo "The binary's agreement with an OFF row is already MEASURED, at 152fe98b:"
echo "ACOPP57 at --arm off reproduces M3c's committed pass-1 row on 58 of 58"
echo "compared fields with ZERO near-ulp acceptances."
sed -n '1,3p' "$D/rows-off/SHA256-off-rows.txt" | sed 's/^/    /'
echo "    ... ($(grep -c . "$D/rows-off/SHA256-off-rows.txt") rows)"

# ---------------------------------------------------------------------------
echo
echo "=========== STEP 2: A6's OFFLINE HALF, from the PINNED YORKNET trace"
echo "Run BEFORE the dynamic leg, so the prediction is on the record before the"
echo "solver moves. No solver runs in this step."
python3 "$D/wrappers/a6_offline_m5.py" "$S/a6" "$PARENT_TRACES/off-YORKNET.csv" \
    > "$LOG/a6-offline.log" 2>&1
a6_rc=$?
echo "A6_OFFLINE_RC=$a6_rc"
tail -24 "$LOG/a6-offline.log" | sed 's/^/    /'
if [ "$a6_rc" -ne 0 ]; then
    # A6 IS A PREDICTION, NOT A GATE, but a script that could not RUN has
    # measured nothing, and that is a stop for the offline half alone. The
    # dynamic legs below still run: A6's two halves are independent readings.
    echo "A6_OFFLINE DID NOT RUN (rc=$a6_rc) -- recorded as such; the dynamic legs"
    echo "                                      below are unaffected"
fi

# ---------------------------------------------------------------------------
echo
echo "=========== STEP 3: ARM a ON YORKNET, ALONE (RULE.md section 8 step 2)"
for cell in $A_CELLS; do
    run_leg a "$cell"
done

# ---------------------------------------------------------------------------
echo
echo "=========== STEP 4: ARM b1 ON THE EIGHT (RULE.md section 8 step 3)"
for cell in $EIGHT; do
    run_leg b1 "$cell"
done

# ---------------------------------------------------------------------------
echo
echo "=========== STEP 5: ARM a+b1 ON THE SAME EIGHT (RULE.md section 8 step 3)"
for cell in $EIGHT; do
    run_leg a+b1 "$cell"
done

# ---------------------------------------------------------------------------
echo
echo "=========== STEP 6: ARM a+b1 ON BRITGAS AND YORKNET (RULE.md section 8 step 3)"
for cell in $AB_EXTRA; do
    run_leg a+b1 "$cell"
done

# ---------------------------------------------------------------------------
echo
echo "=========== STEP 7: NEUTRALITY, DESCRIPTIVE AND EXTENDED (A7, section 8 step 4)"
echo "READING6, SMBANK and ZAMB2-8 under a, b1 and a+b1. The reference is M4's"
echo "COPIED off row for the same cell (rows-off/), and the reading is"
echo "DESCRIPTIVE: status, f, FEASIBILITY and STATIONARITY side by side. No"
echo "'production-ready' wording appears anywhere in this chain or its output."
for arm in a b1 a+b1; do
    for cell in $NEUTRAL; do
        run_leg "$arm" "$cell"
    done
done

echo
echo "--- THE NEUTRALITY READING, side by side with the COPIED off row"
printf '    %-6s %-10s %-18s %-24s %-24s %-24s\n' arm cell status f feasibility_e stationarity
for cell in $NEUTRAL; do
    ref="$D/rows-off/off-$cell.txt"
    for arm in off a b1 a+b1; do
        src="$S/rows/$arm-$cell.txt"
        [ "$arm" = "off" ] && src="$ref"
        if [ ! -s "$src" ]; then
            printf '    %-6s %-10s %s\n' "$arm" "$cell" "CENSORED -- no row"
            continue
        fi
        printf '    %-6s %-10s %-18s %-24s %-24s %-24s\n' "$arm" "$cell" \
            "$(grep -m1 '^status=' "$src" | cut -d= -f2)" \
            "$(grep -m1 '^f=' "$src" | cut -d= -f2)" \
            "$(grep -m1 '^feasibility_e=' "$src" | cut -d= -f2)" \
            "$(grep -m1 '^stationarity=' "$src" | cut -d= -f2)"
    done
done
echo "    A CHANGE OF STATUS on any of the three is HARM and stops that arm's"
echo "    verdicts being quoted (RULE.md section 8 step 4). This chain PRINTS"
echo "    the four rows; it does not pronounce on them."

# ---------------------------------------------------------------------------
echo
echo "=========== SUMMARY"
echo "rows written:"
ls -1 "$S/rows" 2>/dev/null | sed 's/^/    /'
echo "traces written: $(ls -1 "$S/traces" 2>/dev/null | wc -l)"
n_want=$(( $(echo $A_CELLS | wc -w) + 2 * $(echo $EIGHT | wc -w) + $(echo $AB_EXTRA | wc -w) + 3 * $(echo $NEUTRAL | wc -w) ))
n_got=$(ls -1 "$S/rows" 2>/dev/null | wc -l)
echo "legs attempted: $n_want   rows surviving: $n_got   CENSORED: $(( n_want - n_got ))"
echo "TRUNCATED (deadline_fired=1):"
grep -l '^deadline_fired=1' "$S"/rows/*.txt 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo "UNSUPPORTED triggers across the surviving rows:"
for k in indep_rank_violations indep_refused_mandatory_dependent indep_leak_bound_exceeded \
         indep_enumeration_mismatches scale_unavailable_passes; do
    n=$(grep -l "^$k=[1-9]" "$S"/rows/*.txt 2>/dev/null | wc -l)
    echo "    $k nonzero on $n row(s)"
done
echo "CHAIN_END $(date -Is)"
