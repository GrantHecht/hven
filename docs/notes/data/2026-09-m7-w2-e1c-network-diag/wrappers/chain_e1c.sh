#!/usr/bin/env bash
# M7 W2 E1c -- THE ON/OFF CHAIN over the four network cells. Detached with
# `setsid`; nothing polls it. Brief: .superpowers/m7-w2-e1c-diag-brief.md
# (questions Q1-Q5 PRE-REGISTERED there, before any leg).
#
# LEGS, in this order: equality basis ON (the option NEVER ASSIGNED -- hven's
# own default at 43800aae, which is ON; each row must print
# `equality_basis_touched=0` and `opt_equality_basis=1`) on DALLASM, STEENBRB,
# STEENBRC, STEENBRG; then OFF (`--equality-basis off`; the row must print
# `opt_equality_basis=0`) on the same four. Every leg: DEFAULT options
# otherwise, `--arm off` (M5's arms are retired and never armed), the ledger on,
# the observer armed over EVERY solve (`--capture-solve -1`), a 600 s solver
# deadline inside `timeout 900`, MKL_NUM_THREADS=1 OMP_NUM_THREADS=1,
# `taskset -c 2`.
#
# ONE SOLVER AT A TIME: the box lock is taken per leg and released between
# legs; `pgrep` runs as its own command BEFORE the lock and again INSIDE it. No
# leg signals a process it did not start. A leg that exits non-zero is CENSORED
# and its row REMOVED. Wall-clock is informational (CLAUDE.md section 7).
set -o pipefail

PREFLIGHT_ONLY=0
[ "$1" = "--preflight-only" ] && PREFLIGHT_ONLY=1

ROOT=/home/ghecht/Projects/hven
D="$ROOT/docs/notes/data/2026-09-m7-w2-e1c-network-diag"
S="$ROOT/.scratch/m7w2e1c"
EXE="$S/build/e1c_probe"
EXE_SHA=3edc37dace4c961ba350f7dac7869134830670bdc0b51061a818b97a7998f09a
BENCH=/home/ghecht/Projects/hven-bench
LOG="$S/logs"
CELLS="DALLASM STEENBRB STEENBRC STEENBRG"
PAT="hven_bench_cell|m3c_p2a_probe|m4_arm_probe|m5_arm_probe|e1c_probe|hven_sqp_corpus"

mkdir -p "$S/rows" "$S/traces" "$LOG"
echo "CHAIN_START $(date -Is)"
echo "HEAD $(git -C "$ROOT" rev-parse --short HEAD)  dirty-files=$(git -C "$ROOT" status --porcelain | wc -l)"

for p in "$EXE" "$BENCH/lanes/cutest/cutest_source.cpp"; do
    [ -e "$p" ] || { echo "REFUSED: missing $p"; echo "CHAIN_END $(date -Is) rc=90"; exit 90; }
done
got=$(sha256sum "$EXE" | cut -d' ' -f1)
echo "EXE_SHA256 $got"
[ "$got" = "$EXE_SHA" ] || { echo "REFUSED: not the smoked binary"; echo "CHAIN_END $(date -Is) rc=91"; exit 91; }
echo "PIN_BINARY OK -- the smoked probe, unrebuilt"

ROSTER=$(sed -n '/kProblems = {/,/};/p' "$BENCH/lanes/cutest/cutest_source.cpp" | grep -o '"[A-Za-z0-9-]*"' | tr -d '"')
n_roster=$(echo "$ROSTER" | grep -c .)
[ "$n_roster" -ge 30 ] || { echo "REFUSED: roster read $n_roster names"; echo "CHAIN_END $(date -Is) rc=92"; exit 92; }
echo "$ROSTER" | grep -qx NOT_A_CELL && { echo "REFUSED: roster check is vacuous"; exit 92; }
for c in $CELLS; do echo "$ROSTER" | grep -qx "$c" || { echo "REFUSED: $c not in roster"; exit 92; }; done
echo "all four cells are in the $n_roster-name roster"
for c in $CELLS; do
    for eqb in on off; do
        [ -e "$S/rows/$eqb-$c.txt" ] && { echo "REFUSED: a row already exists: $S/rows/$eqb-$c.txt"; echo "CHAIN_END $(date -Is) rc=93"; exit 93; }
    done
done

run_leg() {
    local eqb="$1" cell="$2"
    local row="$S/rows/$eqb-$cell.txt" trace="$S/traces/$eqb-$cell.csv" err="$LOG/$eqb-$cell.err"
    local extra=""
    [ "$eqb" = "off" ] && extra="--equality-basis off"
    if pgrep -a -f "$PAT" | grep -v -e "eval " -e pgrep -e chain_e1c > /dev/null; then
        echo "LEG $eqb-$cell REFUSED: a foreign cell binary is running BEFORE the lock"
        return 99
    fi
    echo "LEG_START $eqb-$cell $(date -Is)"
    flock /tmp/box-build.lock bash -c '
        set -o pipefail
        eqb="$1"; cell="$2"; EXE="$3"; row="$4"; trace="$5"; err="$6"; BENCH="$7"; extra="$8"; PAT="$9"
        if pgrep -a -f "$PAT" | grep -v -e "eval " -e pgrep -e "bash -c" -e chain_e1c; then
            echo "FOREIGN CELL BINARY UNDER MY LOCK -- REFUSING"; exit 99
        fi
        echo "PGREP_EMPTY $eqb-$cell: no foreign cell binary under my lock"
        cd "$BENCH" || exit 93
        echo "FOREGROUND_START $eqb-$cell $(date -Is)"
        # shellcheck disable=SC2086
        MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 "$EXE" \
            --problem "$cell" --arm off $extra \
            --deadline 600 --ledger on --trace on --trace-out "$trace" \
            --capture-solve -1 --max-records 2000000 > "$row" 2> "$err"
        rc=$?
        echo "FOREGROUND_END $eqb-$cell $(date -Is) rc=$rc"
        exit $rc
    ' _ "$eqb" "$cell" "$EXE" "$row" "$trace" "$err" "$BENCH" "$extra" "$PAT"
    local rc=$?
    echo "LEG_END $eqb-$cell $(date -Is) rc=$rc"
    if [ "$rc" -ne 0 ]; then
        echo "CENSORED $eqb-$cell (rc=$rc) -- row removed; nothing is filled in later"
        rm -f "$row"
        head -3 "$err" 2>/dev/null | sed 's/^/    /'
        return 0
    fi
    local want_opt=1 want_touch=0
    [ "$eqb" = "off" ] && { want_opt=0; want_touch=1; }
    if ! grep -qx "opt_equality_basis=$want_opt" "$row" || ! grep -qx "equality_basis_touched=$want_touch" "$row" \
       || ! grep -qx "arm=off" "$row"; then
        echo "REFUSED: $eqb-$cell's row does not carry the switch it was asked for"
        echo "CHAIN_END $(date -Is) rc=94"; exit 94
    fi
    echo "    $(grep -m1 '^status=' "$row")  $(grep -m1 '^deadline_fired=' "$row")  $(grep -m1 '^major_iters=' "$row")  $(grep -m1 '^qp_minor_iters=' "$row")  $(grep -m1 '^feasibility_e=' "$row")"
    echo "    $(grep -m1 '^eqb_computed=' "$row")  $(grep -m1 '^eqb_dropped_count=' "$row")  $(grep -m1 '^eqb_dropped_peak=' "$row")  $(grep -m1 '^eqb_recomputes=' "$row")  $(grep -m1 '^eqb_switched_off=' "$row")  $(grep -m1 '^eqb_inconsistent_row=' "$row")"
    echo "    $(grep -m1 '^eqb_ce_dropped_max_abs=' "$row")  $(grep -m1 '^eqb_ce_retained_max_abs=' "$row")  $(grep -m1 '^restoration_entries=' "$row")  $(grep -m1 '^summary_records=' "$row")  $(grep -m1 '^summary_truncated=' "$row")"
    return 0
}

if [ "$PREFLIGHT_ONLY" -eq 1 ]; then
    echo "PREFLIGHT_OK: binary pinned, roster checked, no stale rows. No solver ran."
    echo "CHAIN_END $(date -Is) rc=0"; exit 0
fi

echo; echo "=========== ON (the option never assigned: hven's default)"
for c in $CELLS; do run_leg on "$c"; done
echo; echo "=========== OFF (--equality-basis off)"
for c in $CELLS; do run_leg off "$c"; done

echo; echo "=========== SUMMARY"
ls -1 "$S/rows" | sed 's/^/    /'
n_got=$(ls -1 "$S/rows" | wc -l)
echo "legs attempted: 8   rows surviving: $n_got   CENSORED: $(( 8 - n_got ))"
echo "TRUNCATED (deadline_fired=1):"
grep -l '^deadline_fired=1' "$S"/rows/*.txt 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo "observer truncation (summary_truncated=1):"
grep -l '^summary_truncated=1' "$S"/rows/*.txt 2>/dev/null | sed 's/^/    /' || echo "    (none)"
( cd "$S" && sha256sum rows/*.txt traces/*.csv ) > "$LOG/chain-sha256.txt" 2>/dev/null
echo "CHAIN_DONE"
echo "CHAIN_END $(date -Is)"
