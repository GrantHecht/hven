#!/bin/bash
# A3 hven-side measurement chain (design page s5.1-5.2), base 534bc87f and head, both new-toolchain
# Release builds made in this session. Every leg: pgrep first (its own command), its own
# flock /tmp/box-build.lock hold, MKL_NUM_THREADS=1, no timeout. Compares are file processing.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff
S=$W/.scratch/a3
BB=$S/build-base-rel
BH=$S/build-head-rel
O=$S/meas
L=$O/logs
mkdir -p "$O" "$L"
export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
CELLS="$(grep -v '^#' "$W/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
echo "HEAD $(git -C "$W" rev-parse HEAD) DIRTY=$(git -C "$W" status --porcelain --untracked-files=no | wc -l)"
echo "BASE $(git -C "$S/base-src" rev-parse HEAD) DIRTY=$(git -C "$S/base-src" status --porcelain --untracked-files=no | wc -l)"
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
# G1: U0 three engines, base and head.
for which in base head; do
    if [ $which = base ]; then B=$BB; else B=$BH; fi
    for eng in walk ssn ipm; do
        leg "u0-$which-$eng" taskset -c 2 "$B/bench/hven_sqp_corpus" --engine "$eng" --cells "$CELLS" \
            --csv "$O/u0-$which-$eng.csv" || fail=1
    done
done
# G2 / G2b: E1 gate and families at OFF, base and head. --gate exits 1 on a RED beyond the named
# cell: recorded, not a chain failure.
leg e1-base-off "$BB/bench/hven_sqp_ipqp_e1_arm" --gate --out "$O/e1-base-off.csv"
echo "e1-base-off gate rc=$?"
leg fam-base-off "$BB/bench/hven_sqp_ipqp_e1_arm" --families --out "$O/fam-base-off.csv" || fail=1
# The arms at the head (s5.2): OFF, S1, S3, K1, KN on --gate, --families, --ladder.
arm_flags() {
    case "$1" in
        off) echo "" ;;
        s1) echo "--converge-slack 1" ;;
        s3) echo "--converge-slack 1000" ;;
        k1) echo "--ladder-skip-after 1" ;;
        kn) echo "--ladder-skip-after 1000000" ;;
    esac
}
for arm in off s1 s3 k1 kn; do
    F=$(arm_flags $arm)
    # shellcheck disable=SC2086
    leg "e1-head-$arm" "$BH/bench/hven_sqp_ipqp_e1_arm" --gate $F --out "$O/e1-head-$arm.csv"
    echo "e1-head-$arm gate rc=$?"
    # shellcheck disable=SC2086
    leg "fam-head-$arm" "$BH/bench/hven_sqp_ipqp_e1_arm" --families $F --out "$O/fam-head-$arm.csv" || fail=1
    # shellcheck disable=SC2086
    leg "lad-head-$arm" "$BH/bench/hven_sqp_ipqp_e1_arm" --ladder $F --out "$O/lad-head-$arm.csv" || fail=1
done
for eng in walk ssn ipm; do
    python3 -B "$W/scripts/compare_replay.py" "$O/u0-base-$eng.csv" "$O/u0-head-$eng.csv" "u0-$eng" \
        > "$O/u0-exact-$eng.txt" 2>&1
    echo "u0-$eng compare rc=$? :: $(tail -1 "$O/u0-exact-$eng.txt")"
done
echo "CHAIN_MEAS_DONE fail=$fail $(date -Is)"
