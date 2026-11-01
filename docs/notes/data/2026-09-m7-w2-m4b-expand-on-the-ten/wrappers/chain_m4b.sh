#!/usr/bin/env bash
# M7 W2 M4b -- ARM E, ALONE, OVER THE TEN. Detached with `setsid`; nothing polls it.
#
# Governed by ../RULE.md, committed BEFORE this chain's first leg and never
# edited after it. Every term below is that rule's section 10.2; every stop is
# its section 8; the order is its section 10.1.
#
#   STEP 0  THE PINS -- the binary's sha256 against RULE.md section 3.1, the ten
#           COPIED OFF rows against rows-off/SHA256-off-rows.txt, and the cell
#           roster read out of the harness's own source. A failure here stops
#           everything BEFORE any leg.
#   STEP 1  ARM E over the ten, in M4's own cell order.
#
# There is NO OFF leg in this task (RULE.md section 4): M4's ten OFF controls
# passed its identity gate 10/10 and are REUSED BY COPY. There is no phase 3 and
# no extension.
#
# ONE SOLVER AT A TIME. The box lock is taken per leg and released between legs;
# `pgrep` runs as its own command BEFORE the lock and again INSIDE it. No leg
# signals a process it did not start. Wall-clock is informational (CLAUDE.md
# section 7); the deadlines here are STOPS.
set -o pipefail

ROOT=/home/ghecht/Projects/hven
D="$ROOT/docs/notes/data/2026-09-m7-w2-m4b-expand-on-the-ten"
A="$ROOT/docs/notes/data/2026-09-m7-w2-m4-anticycling-arm"
S="$ROOT/.scratch/m7w2m4b"
EXE="$ROOT/.scratch/m7w2m4/arm/build/m4_arm_probe"
EXE_SHA=c1be17d2d47e7930440450d72d233da56a8fc374bceb68062baea924fe50a720
BENCH=/home/ghecht/Projects/hven-bench
LOG="$S/logs"

CELLS="ACOPP57 DALLASM STEENBRB ACOPP118 ACOPR57 STEENBRC STEENBRG YORKNET SAWPATH BRITGAS"
ARM=E

mkdir -p "$S/rows" "$S/traces" "$LOG"

echo "CHAIN_START $(date -Is)"
echo "HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"
echo "RULE  $D/RULE.md"
echo "EXE   $EXE"

for p in "$EXE" "$D/RULE.md" "$D/rows-off/SHA256-off-rows.txt" "$BENCH"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; echo "CHAIN_END $(date -Is) rc=90"; exit 90; }
done

# --- PIN 1: THE BINARY. RULE.md section 8 row 1. Nothing is ever rebuilt here.
got=$(sha256sum "$EXE" | cut -d' ' -f1)
echo "EXE_SHA256 $got"
if [ "$got" != "$EXE_SHA" ]; then
    echo "REFUSED: the pinned binary does not match RULE.md section 3.1"
    echo "    expected $EXE_SHA"
    echo "    got      $got"
    echo "CHAIN_END $(date -Is) rc=91"
    exit 91
fi
echo "PIN_BINARY OK -- the M4 probe, unrebuilt"

# --- PIN 2: THE COPIED OFF ROWS. RULE.md sections 4 and 8 row 2.
echo "--- the ten COPIED OFF rows, against their own manifest"
if ( cd "$D/rows-off" && sha256sum -c SHA256-off-rows.txt ) > "$LOG/pin-off-rows.txt" 2>&1; then
    echo "PIN_OFF_ROWS OK -- $(grep -c ': OK$' "$LOG/pin-off-rows.txt") rows verified"
else
    echo "REFUSED: a copied OFF row does not match its manifest"
    grep -v ': OK$' "$LOG/pin-off-rows.txt" | head -5 | sed 's/^/    /'
    echo "CHAIN_END $(date -Is) rc=91"
    exit 91
fi
# and they must still be byte-identical to M4's own committed rows.
off_bad=0
for cell in $CELLS; do
    cmp -s "$A/rows/off-$cell.txt" "$D/rows-off/off-$cell.txt" \
        || { echo "REFUSED: rows-off/off-$cell.txt is not M4's committed row"; off_bad=1; }
done
[ "$off_bad" -eq 0 ] || { echo "CHAIN_END $(date -Is) rc=91"; exit 91; }
echo "PIN_OFF_ROWS_ARE_M4S OK -- all ten byte-identical to M4's committed rows"

# --- PIN 3: THE CELL ROSTER, read out of the harness's own source, READ ONLY.
# A misspelled name would otherwise produce an rc=2 refusal this chain would
# class as CENSORED (RULE.md section 8), losing a cell to a typo.
echo "--- the cell roster, checked before any leg"
ROSTER_SRC="$BENCH/lanes/cutest/cutest_source.cpp"
[ -r "$ROSTER_SRC" ] || { echo "REFUSED: cannot read $ROSTER_SRC"; echo "CHAIN_END $(date -Is) rc=92"; exit 92; }
ROSTER=$(sed -n '/kProblems = {/,/};/p' "$ROSTER_SRC" | grep -o '"[A-Za-z0-9-]*"' | tr -d '"')
n_roster=$(echo "$ROSTER" | grep -c .)
if [ "$n_roster" -lt 30 ]; then
    echo "REFUSED: the roster read back only $n_roster name(s) -- a roster check"
    echo "         that read nothing would pass everything"
    echo "CHAIN_END $(date -Is) rc=92"; exit 92
fi
if echo "$ROSTER" | grep -qx "NOT_A_CELL"; then
    echo "REFUSED: the roster check accepts a name that cannot be in the set"
    echo "CHAIN_END $(date -Is) rc=92"; exit 92
fi
roster_bad=0
for cell in $CELLS; do
    echo "$ROSTER" | grep -qx "$cell" || { echo "REFUSED: $cell is not in the roster"; roster_bad=1; }
done
[ "$roster_bad" -eq 0 ] || { echo "CHAIN_END $(date -Is) rc=92"; exit 92; }
echo "all $(echo $CELLS | wc -w) cell names are in the $n_roster-name roster"

# ---------------------------------------------------------------------------
# run_leg CELL -- RULE.md section 10.2's terms, in ONE place.
# ---------------------------------------------------------------------------
run_leg() {
    local cell="$1"
    local deadline=600
    [ "$cell" = "SAWPATH" ] && deadline=300
    local row="$S/rows/$ARM-$cell.txt"
    local trace="$S/traces/$ARM-$cell.csv"
    local err="$LOG/$ARM-$cell.err"

    # THE pgrep AS ITS OWN COMMAND, BEFORE THE LOCK.
    if pgrep -a -f "hven_bench_cell|m3c_p2a_probe|m4_arm_probe|hven_sqp_corpus" \
         | grep -v -e "eval " -e pgrep -e chain_m4 > /dev/null; then
        echo "LEG $ARM-$cell REFUSED: a foreign cell binary is running BEFORE the lock"
        return 99
    fi

    echo "LEG_START $ARM-$cell $(date -Is) deadline=${deadline}s"
    flock /tmp/box-build.lock bash -c '
        set -o pipefail
        arm="$1"; cell="$2"; deadline="$3"
        EXE="$4"; row="$5"; trace="$6"; err="$7"; BENCH="$8"
        if pgrep -a -f "hven_bench_cell|m3c_p2a_probe|m4_arm_probe|hven_sqp_corpus" \
             | grep -v -e "eval " -e pgrep -e "bash -c" -e chain_m4; then
            echo "FOREIGN CELL BINARY UNDER MY LOCK -- REFUSING"
            exit 99
        fi
        echo "PGREP_EMPTY $arm-$cell: no foreign cell binary under my lock"
        cd "$BENCH" || exit 93
        echo "FOREGROUND_START $arm-$cell $(date -Is)"
        MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 "$EXE" \
            --problem "$cell" --arm "$arm" --lex-full-pricing off \
            --deadline "$deadline" --ledger on --trace on --trace-out "$trace" \
            --capture-solve -1 --max-records 2000000 > "$row" 2> "$err"
        rc=$?
        echo "FOREGROUND_END $arm-$cell $(date -Is) rc=$rc"
        exit $rc
    ' _ "$ARM" "$cell" "$deadline" "$EXE" "$row" "$trace" "$err" "$BENCH"
    local rc=$?
    echo "LEG_END $ARM-$cell $(date -Is) rc=$rc"
    if [ "$rc" -ne 0 ]; then
        # RULE.md section 8: a killed leg, or one exiting non-zero for any other
        # reason, is CENSORED. Its ROW IS REMOVED so nothing downstream can read
        # a partial one as a measurement, and nothing is filled in later.
        echo "CENSORED $ARM-$cell (rc=$rc) -- RULE.md section 8 fills nothing in later"
        rm -f "$row"
        head -3 "$err" 2>/dev/null | sed 's/^/    /'
        return 0
    fi
    # RULE.md section 8 row 5: the switch must be reachable, on every leg.
    if ! grep -qx "arm_compiled_in=1" "$row"; then
        echo "REFUSED: $ARM-$cell has no arm_compiled_in=1 -- the switch is not reachable"
        echo "CHAIN_END $(date -Is) rc=94"
        exit 94
    fi
    # and E must leave the DROP counter at zero: it touches no drop rule.
    # A non-zero value is a FINDING about the instrument (RULE.md section 6) and
    # is REPORTED, never silently passed and never a re-run.
    local lexd
    lexd=$(grep -m1 '^arm_lex_drop_switches=' "$row" | cut -d= -f2)
    [ "$lexd" = "0" ] || echo "    FINDING $ARM-$cell: arm_lex_drop_switches=$lexd on an EXPAND leg"
    grep -m1 -E '^(status|deadline_fired|qp_minor_iters|factorizations|major_iters)=' "$row" >/dev/null
    echo "    $(grep -m1 '^status=' "$row")  $(grep -m1 '^deadline_fired=' "$row")  $(grep -m1 '^qp_minor_iters=' "$row")  $(grep -m1 '^major_iters=' "$row")"
    return 0
}

echo
echo "=========== ARM E OVER THE TEN (RULE.md section 10.1 step 2)"
for cell in $CELLS; do
    run_leg "$cell"
done

echo
echo "=========== SUMMARY"
echo "rows written:"
ls -1 "$S/rows" 2>/dev/null | sed 's/^/    /'
echo "traces written: $(ls -1 "$S/traces" 2>/dev/null | wc -l)"
echo "CENSORED cells: $(( $(echo $CELLS | wc -w) - $(ls -1 "$S/rows" 2>/dev/null | wc -l) ))"
echo "TRUNCATED (deadline_fired=1):"
grep -l '^deadline_fired=1' "$S"/rows/*.txt 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo "CHAIN_END $(date -Is)"
