#!/usr/bin/env bash
# M7 W2 M4 -- THE ARM CHAIN. Detached with `setsid`; nothing polls it.
#
# Governed by RULE.md, committed at a22d08e8 BEFORE this script existed and
# never edited after this chain's first leg. Every term below is section 4.2's;
# every stop is section 8's; the order is section 4.1's and it is not cosmetic.
#
#   PHASE 1  the OFF CONTROLS -- the ten section 1.1 cells at `--arm off`, each
#            gated against its COMMITTED M3c pass-1 row on EVERY printed field.
#            A cell that does not reproduce STOPS: it enters no arm.
#   PHASE 2  the NEUTRALITY legs -- the three section 1.2 cells that hven-sqp
#            already solves at DEFAULT, x four arms. E, L and L+ must reproduce
#            that cell's own `off` leg BIT-FOR-BIT (the gate's STRICT mode: a
#            WITHIN is a difference). A difference STOPS THAT ARM entirely.
#   PHASE 3  E, then L, then L+, over the cells whose OFF control passed,
#            skipping any arm phase 2 stopped.
#
# ONE SOLVER AT A TIME. The box lock is taken per leg and released between legs;
# `pgrep` runs as its own command BEFORE the lock and again INSIDE it. No leg
# signals a process it did not start. Wall-clock is informational (CLAUDE.md
# section 7); the deadlines here are STOPS.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
D="$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm"
M3C="$ROOT/docs/notes/data/2026-09-m7-w2-m3c-default-census"
S="$ROOT/.scratch/m7w2m4/arm"
EXE="$S/build/m4_arm_probe"
BENCH=/home/ghecht/Projects/hven-bench
LOG="$S/logs"

CELLS="ACOPP57 DALLASM STEENBRB ACOPP118 ACOPR57 STEENBRC STEENBRG YORKNET SAWPATH BRITGAS"
NEUTRAL="READING6 SMBANK ZAMB2-8"
ARMS="E L L+"

mkdir -p "$S/rows" "$S/traces" "$LOG"

echo "CHAIN_START $(date -Is)"
echo "HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
echo "RULE  $D/RULE.md"
echo "EXE   $EXE"

for p in "$EXE" "$D/RULE.md" "$D/wrappers/gate_m4.py" "$M3C/rows" "$BENCH"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; echo "CHAIN_END $(date -Is) rc=90"; exit 90; }
done
sha256sum "$EXE"

# EVERY CELL NAME IS CHECKED AGAINST THE SET'S OWN ROSTER BEFORE ANY LEG.
# A misspelled name would otherwise produce a rc=2 refusal that this chain would
# class as CENSORED (RULE.md section 8), and a NEUTRALITY arm would then be
# STOPPED for a typo rather than for a behaviour change. The roster is READ out
# of the harness's own source -- `lanes/cutest/cutest_source.cpp`, the one list
# the probe itself validates against -- rather than re-typed here, and it is
# READ ONLY: nothing under hven-bench is written and its CMake is never entered.
echo "--- the cell roster, checked before any leg"
ROSTER_SRC="$BENCH/lanes/cutest/cutest_source.cpp"
[ -r "$ROSTER_SRC" ] || { echo "REFUSED: cannot read $ROSTER_SRC"; echo "CHAIN_END $(date -Is) rc=92"; exit 92; }
ROSTER=$(sed -n '/kProblems = {/,/};/p' "$ROSTER_SRC" | grep -o '"[A-Za-z0-9-]*"' | tr -d '"')
n_roster=$(echo "$ROSTER" | grep -c .)
if [ "$n_roster" -lt 30 ]; then
    echo "REFUSED: the roster read back only $n_roster name(s) -- the harvest is wrong,"
    echo "         and a roster check that read nothing would pass everything"
    echo "CHAIN_END $(date -Is) rc=92"
    exit 92
fi
# NON-VACUITY: a name that is NOT in the roster must be rejected by this same
# test, or the test is a no-op.
if echo "$ROSTER" | grep -qx "NOT_A_CELL"; then
    echo "REFUSED: the roster check accepts a name that cannot be in the set"
    echo "CHAIN_END $(date -Is) rc=92"; exit 92
fi
roster_bad=0
for cell in $CELLS $NEUTRAL; do
    echo "$ROSTER" | grep -qx "$cell" || { echo "REFUSED: $cell is not in the roster"; roster_bad=1; }
done
if [ "$roster_bad" -ne 0 ]; then
    echo "CHAIN_END $(date -Is) rc=92"
    exit 92
fi
echo "all $(echo $CELLS $NEUTRAL | wc -w) cell names are in the $n_roster-name roster"

# ---------------------------------------------------------------------------
# run_leg ARM CELL
#
# RULE.md section 4.2's terms, in one place so no leg can quietly differ from
# another: DEFAULT options (no --tr-init is ever passed, so the option field is
# never assigned), MKL_NUM_THREADS=1 OMP_NUM_THREADS=1, taskset -c 2, the ledger
# on, the observer armed over EVERY solve, a 600 s solver deadline (300 s for
# SAWPATH, the value M3c ran it at) inside an outer `timeout 900`.
# ---------------------------------------------------------------------------
run_leg() {
    local arm="$1" cell="$2"
    local deadline=600
    [ "$cell" = "SAWPATH" ] && deadline=300
    local row="$S/rows/$arm-$cell.txt"
    local trace="$S/traces/$arm-$cell.csv"
    local err="$LOG/$arm-$cell.err"
    local lex="off"
    [ "$arm" = "L+" ] && lex="on"

    # THE pgrep AS ITS OWN COMMAND, BEFORE THE LOCK.
    if pgrep -a -f "hven_bench_cell|m3c_p2a_probe|m4_arm_probe|hven_sqp_corpus" \
         | grep -v -e "eval " -e pgrep -e chain_m4 > /dev/null; then
        echo "LEG $arm-$cell REFUSED: a foreign cell binary is running BEFORE the lock"
        return 99
    fi

    echo "LEG_START $arm-$cell $(date -Is) deadline=${deadline}s"
    flock /tmp/box-build.lock bash -c '
        set -o pipefail
        arm="$1"; cell="$2"; deadline="$3"; lex="$4"
        EXE="$5"; row="$6"; trace="$7"; err="$8"; BENCH="$9"
        if pgrep -a -f "hven_bench_cell|m3c_p2a_probe|m4_arm_probe|hven_sqp_corpus" \
             | grep -v -e "eval " -e pgrep -e "bash -c" -e chain_m4; then
            echo "FOREIGN CELL BINARY UNDER MY LOCK -- REFUSING"
            exit 99
        fi
        echo "PGREP_EMPTY $arm-$cell: no foreign cell binary under my lock"
        cd "$BENCH" || exit 93
        echo "FOREGROUND_START $arm-$cell $(date -Is)"
        MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 "$EXE" \
            --problem "$cell" --arm "$arm" --lex-full-pricing "$lex" \
            --deadline "$deadline" --ledger on --trace on --trace-out "$trace" \
            --capture-solve -1 --max-records 2000000 > "$row" 2> "$err"
        rc=$?
        echo "FOREGROUND_END $arm-$cell $(date -Is) rc=$rc"
        exit $rc
    ' _ "$arm" "$cell" "$deadline" "$lex" "$EXE" "$row" "$trace" "$err" "$BENCH"
    local rc=$?
    echo "LEG_END $arm-$cell $(date -Is) rc=$rc"
    if [ "$rc" -ne 0 ]; then
        # RULE.md section 8: a killed leg, or one exiting non-zero for any other
        # reason, is CENSORED. Its row is REMOVED so nothing downstream can read
        # a partial one as a measurement, and nothing is filled in later.
        echo "CENSORED $arm-$cell (rc=$rc) -- RULE.md section 8 fills nothing in later"
        rm -f "$row"
        head -3 "$err" 2>/dev/null | sed 's/^/    /'
    fi
    return 0
}

# ---------------------------------------------------------------------------
echo
echo "=========== PHASE 1: THE OFF CONTROLS (RULE.md section 4.1 step 1)"
for cell in $CELLS; do
    run_leg off "$cell"
done

echo
echo "--- GATING THE OFF CONTROLS against M3c's COMMITTED pass-1 rows"
python3 "$D/wrappers/gate_m4.py" \
    --p1-rows "$M3C/rows" \
    --leg-dir "$S/rows" \
    --cells "$(echo $CELLS | tr ' ' ',')" \
    --arms off \
    --rule-outcome off \
    --note "M7 W2 M4 -- THE OFF CONTROLS. Each leg is this task's own binary at
--arm off, compared against M3c's COMMITTED pass-1 row for the same cell. This
is RULE.md section 5: the ordinary (non-strict) mode, because the two rows come
from two DIFFERENTLY BUILT binaries and CLAUDE.md section 7's near-ulp
allowance is exactly the case for that. A cell that does not REPRODUCE enters no
arm (RULE.md section 8)." \
    --out "$LOG/gate-off.txt" > /dev/null 2>&1
echo "GATE_OFF_RC=$?"
grep -F "  off-" "$LOG/gate-off.txt" | tail -14
grep -E "^  REPRODUCES " "$LOG/gate-off.txt" | tail -1

PASSED=""
for cell in $CELLS; do
    if grep -F "  off-$cell " "$LOG/gate-off.txt" | grep -q "REPRODUCES"; then
        PASSED="$PASSED $cell"
    else
        echo "OFF_CONTROL_FAILED $cell -- this cell enters no arm"
    fi
done
echo "CELLS_CARRIED_FORWARD:$PASSED"
echo "$PASSED" > "$LOG/passed-cells.txt"

# ---------------------------------------------------------------------------
echo
echo "=========== PHASE 2: THE NEUTRALITY LEGS (RULE.md section 4.1 step 2)"
echo "the three TRAIN cells hven-sqp ALREADY SOLVES at DEFAULT; E, L and L+ must"
echo "reproduce each cell's own off leg BIT-FOR-BIT (the gate's STRICT mode)."
for cell in $NEUTRAL; do
    run_leg off "$cell"
done
for arm in $ARMS; do
    for cell in $NEUTRAL; do
        run_leg "$arm" "$cell"
    done
done

STOPPED_ARMS=""
for arm in $ARMS; do
    python3 "$D/wrappers/gate_m4.py" \
        --p1-rows "$M3C/rows" \
        --reference-dir "$S/rows" \
        --reference-pattern "off-{cell}.txt" \
        --strict \
        --exclude-prefix arm \
        --leg-dir "$S/rows" \
        --cells "$(echo $NEUTRAL | tr ' ' ',')" \
        --arms "$arm" \
        --rule-outcome off \
        --note "M7 W2 M4 -- THE NEUTRALITY LEG for arm $arm (RULE.md section 5.5).
The reference is that cell's OWN off leg from this same chain, not M3c's row:
these three cells never failed and M3c has no row for them. The bar is STRICT --
a WITHIN is a DIFFERENCE. Two runs of the same binary on the same cell at
MKL_NUM_THREADS=1 differ in nothing, and an arm claiming to be inert where no
tie and no zero ratio occurs owes bit equality, not a tolerance. A difference
here STOPS THIS ARM entirely (RULE.md section 8)." \
        --out "$LOG/gate-neutral-$arm.txt" > /dev/null 2>&1
    echo "GATE_NEUTRAL_${arm}_RC=$?"
    # FIXED STRINGS, NOT A PATTERN: `L+` carries a `+`, which an ERE would read
    # as a repetition operator, and the arm whose whole purpose is to be
    # distinguished from `L` would then match nothing and be reported stopped.
    grep -F "  $arm-" "$LOG/gate-neutral-$arm.txt" | tail -6
    grep -E "^  REPRODUCES " "$LOG/gate-neutral-$arm.txt" | tail -1
    n_ok=$(grep -F "  $arm-" "$LOG/gate-neutral-$arm.txt" | grep -c "REPRODUCES")
    n_want=$(echo $NEUTRAL | wc -w)
    if [ "$n_ok" != "$n_want" ]; then
        echo "ARM_STOPPED $arm -- neutrality: $n_ok of $n_want cells reproduce their own off leg"
        echo "             RULE.md section 8: this arm's results are UNINTERPRETABLE and are"
        echo "             reported as such rather than withheld."
        STOPPED_ARMS="$STOPPED_ARMS $arm"
    else
        echo "ARM_NEUTRAL $arm -- all $n_want cells reproduce their own off leg bit-for-bit"
    fi
done
echo "ARMS_STOPPED_BY_NEUTRALITY:$STOPPED_ARMS"
echo "$STOPPED_ARMS" > "$LOG/stopped-arms.txt"

# ---------------------------------------------------------------------------
echo
echo "=========== PHASE 3: THE ARMS (RULE.md section 4.1 step 3)"
for arm in $ARMS; do
    case " $STOPPED_ARMS " in
        *" $arm "*)
            echo "SKIPPING ARM $arm -- stopped by its neutrality leg"
            # ITS LEGS ARE STILL NOT RUN. RULE.md section 8 says the arm stops
            # ENTIRELY; running it anyway and labelling the rows uninterpretable
            # would be spending the box on numbers the rule already refuses.
            continue ;;
    esac
    for cell in $PASSED; do
        run_leg "$arm" "$cell"
    done
done

echo
echo "=========== SUMMARY"
echo "rows written:"
ls -1 "$S/rows" | sed 's/^/    /'
echo "traces written: $(ls -1 "$S/traces" 2>/dev/null | wc -l)"
echo "CHAIN_END $(date -Is)"
