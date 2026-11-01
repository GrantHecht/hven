#!/bin/bash
# The A1 measurement chain at the committed head: I1, I2, I3 (U0 OFF exact + ipm at ON), I3b.
# Every leg: pgrep first, its own flock /tmp/box-build.lock hold, MKL_NUM_THREADS=1, no timeout.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-ad5e6e56c9da5d8a5
B=$W/.scratch/a1i/build-rel
O=$W/.scratch/a1i/meas
L=$O/logs
mkdir -p "$O" "$L"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
CELLS="$(grep -v '^#' "$W/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
echo "HEAD $(git -C "$W" rev-parse HEAD)"
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
leg u0-ipm-on "$B/bench/hven_sqp_ipqp_e1_arm" --f7-path --margin-stop kExtendAndDemote \
    --cells "$CELLS" --out "$O/u0-ipm-on.csv" || fail=1
leg u0-ipm-off-arm "$B/bench/hven_sqp_ipqp_e1_arm" --f7-path --margin-stop kOff \
    --cells "$CELLS" --out "$O/u0-ipm-off-arm.csv" || fail=1
leg i3b-off "$B/bench/hven_sqp_ipqp_e1_arm" --f7-path --margin-stop kOff --out "$O/i3b-off.csv" || fail=1
leg i3b-on "$B/bench/hven_sqp_ipqp_e1_arm" --f7-path --margin-stop kExtendAndDemote \
    --out "$O/i3b-on.csv" || fail=1
leg i2-off "$B/bench/hven_sqp_ipqp_e1_arm" --gate --out "$O/i2-off.csv" || fail=1
leg i2-on2 "$B/bench/hven_sqp_ipqp_e1_arm" --gate --margin-stop kExtendAndDemote \
    --out "$O/i2-on2.csv" || fail=1
for arm in ON1 ON2X ON2; do
    A1_ARM=$arm A1_OUT="$O/i2-seam-$arm.csv" leg "i2-seam-$arm" "$B/tests/hven_sqp_seam_tests" \
        --gtest_also_run_disabled_tests \
        --gtest_filter=DISABLED_MeasurementArm.E1TwentyNineCellsUnderOneArm || fail=1
done
A1_OUT="$O/i2-diag-exponents.csv" \
A1_CELLS=e1_f7_n4000_af01_m1e-6,e1_f7_n4000_af10_m1e-6,e1_f7_n4000_af30_m1e-6,e1_f7_n20000_af01_m1e-6,e1_f7_n20000_af10_m1e-6,e1blk_f7_n20000_af01_m1e-6,e1blk_f7_n20000_af10_m1e-6,e1blk_f7_n20000_af30_m1e-6 \
    leg i2-diag "$B/tests/hven_sqp_seam_tests" --gtest_also_run_disabled_tests \
    --gtest_filter=DISABLED_MeasurementArm.FinalPassExponentsOfCommittedRowsByTruth || fail=1
A1_I1_OUT="$O/i1-table.csv" leg i1 "$B/tests/hven_ipqp_seam_tests" --gtest_also_run_disabled_tests \
    --gtest_filter='DISABLED_IpqpMarginI1.*' || fail=1
echo "CHAIN_DONE fail=$fail $(date -Is)"
