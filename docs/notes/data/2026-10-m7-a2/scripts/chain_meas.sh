#!/bin/bash
# The A2 hven-side measurement chain at the committed head (design page s6.1-6.3):
# G1 U0 exact (three engines), G2 + E1 arms (--gate), T10 families (--families).
# Every leg: pgrep first (its own command), its own flock /tmp/box-build.lock hold,
# MKL_NUM_THREADS=1, no timeout. Comparisons are file processing after the legs.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-aa38d0451de957235
B=$W/.scratch/a2/build-rel
O=$W/.scratch/a2/meas
L=$O/logs
mkdir -p "$O" "$L"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
CELLS="$(grep -v '^#' "$W/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
echo "HEAD $(git -C "$W" rev-parse HEAD) DIRTY=$(git -C "$W" status --porcelain --untracked-files=no | wc -l)"
leg() {
    local name=$1
    shift
    if pgrep -a hven_bench_cell; then echo "PGREP_NONEMPTY $name"; else echo "PGREP_EMPTY $name"; fi
    echo "FOREGROUND_START $name $(date -Is)"
    flock /tmp/box-build.lock "$@" > "$L/$name.log" 2>&1
    local rc=$?
    echo "FOREGROUND_END $name rc=$rc $(date -Is)"
    return $rc
}
fail=0
for eng in walk ssn ipm; do
    leg "u0-$eng" taskset -c 2 "$B/bench/hven_sqp_corpus" --engine "$eng" --cells "$CELLS" \
        --csv "$O/u0-head-$eng.csv" || fail=1
done
# --gate exits 1 on a RED beyond the named cell: recorded, not a chain failure.
leg e1-off "$B/bench/hven_sqp_ipqp_e1_arm" --gate --out "$O/e1-off.csv"
echo "e1-off gate rc=$?"
leg e1-m0 "$B/bench/hven_sqp_ipqp_e1_arm" --gate --mu0-rule kComplementarity --out "$O/e1-m0.csv"
echo "e1-m0 gate rc=$?"
leg e1-mf "$B/bench/hven_sqp_ipqp_e1_arm" --gate --mu-floor --out "$O/e1-mf.csv"
echo "e1-mf gate rc=$?"
leg e1-m0mf "$B/bench/hven_sqp_ipqp_e1_arm" --gate --mu0-rule kComplementarity --mu-floor \
    --out "$O/e1-m0mf.csv"
echo "e1-m0mf gate rc=$?"
leg fam-off "$B/bench/hven_sqp_ipqp_e1_arm" --families --out "$O/fam-off.csv" || fail=1
leg fam-m0 "$B/bench/hven_sqp_ipqp_e1_arm" --families --mu0-rule kComplementarity \
    --out "$O/fam-m0.csv" || fail=1
leg fam-mf "$B/bench/hven_sqp_ipqp_e1_arm" --families --mu-floor --out "$O/fam-mf.csv" || fail=1
leg fam-m0mf "$B/bench/hven_sqp_ipqp_e1_arm" --families --mu0-rule kComplementarity --mu-floor \
    --out "$O/fam-m0mf.csv" || fail=1
R=$W/docs/notes/data/2026-10-m7-scw-gates/gates/replay
for eng in walk ssn ipm; do
    python3 -B "$W/scripts/compare_replay.py" "$R/head-$eng.csv" "$O/u0-head-$eng.csv" "u0-$eng" \
        > "$O/u0-exact-$eng.txt" 2>&1
    echo "u0-$eng compare rc=$? :: $(tail -1 "$O/u0-exact-$eng.txt")"
done
echo "CHAIN_MEAS_DONE fail=$fail $(date -Is)"
