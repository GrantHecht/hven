#!/bin/bash
# A3 chain A at the committed head: head Release (re-taken) + Debug builds, the three suite legs,
# the hven-side measurement chain, then the two sticky probes (scratch builds of patched copies).
# Each build/run is its own lock hold (build.sh / suite.sh / chain_meas.sh / leg.sh).
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff
S=$W/.scratch/a3
echo "CHAIN_A_START $(date -Is) HEAD $(git -C "$W" rev-parse HEAD) DIRTY=$(git -C "$W" status --porcelain --untracked-files=no | wc -l)"
fail=0
bash "$S/build.sh" head rel || fail=1
bash "$S/build.sh" head dbg || fail=1
if [ "$fail" = 0 ]; then
    for s in rel dbg long; do
        bash "$S/suite.sh" "$s" || fail=1
        grep -E 'tests passed|tests failed' "$S/logs/suite-$s.log"
    done
fi
echo "SUITES_DONE fail=$fail $(date -Is)"
bash "$S/chain_meas.sh"
# Probe P (design page s3.3, DESCRIPTIVE): patched scratch copies of the committed head.
for form in pu pc; do
    P=$S/probe-$form
    if [ ! -d "$P" ]; then
        git -C "$W" worktree add -q --detach "$P" HEAD || { echo "PROBE $form worktree FAILED"; continue; }
        cp -a "$W/dep/eigen/." "$P/dep/eigen/"
        cp -a "$W/dep/fmt/." "$P/dep/fmt/"
        rm -f "$P/dep/eigen/.git" "$P/dep/fmt/.git"
        python3 -B "$S/sticky_patch.py" "$P" "$form" || { echo "PROBE $form patch FAILED"; continue; }
        git -C "$P" diff > "$S/meas/probe-$form.patch"
    fi
    if pgrep -a hven_bench_cell; then echo "PGREP_NONEMPTY probe-build-$form"; else echo "PGREP_EMPTY probe-build-$form"; fi
    echo "FOREGROUND_START probe-build-$form $(date -Is)"
    flock /tmp/box-build.lock bash -c "set -o pipefail; cmake -S $P -B $S/build-probe-$form -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DHVEN_BUILD_TESTS=ON -DHVEN_BUILD_BENCH=ON > $S/logs/configure-probe-$form.log 2>&1 && cmake --build $S/build-probe-$form -j14 --target hven_sqp_ipqp_e1_arm > $S/logs/build-probe-$form.log 2>&1"
    echo "FOREGROUND_END probe-build-$form rc=$? $(date -Is)"
    bash "$S/leg.sh" "lad-probe-$form" "$S/build-probe-$form/bench/hven_sqp_ipqp_e1_arm" --ladder --out "$S/meas/lad-probe-$form.csv"
    bash "$S/leg.sh" "fam-probe-$form" "$S/build-probe-$form/bench/hven_sqp_ipqp_e1_arm" --families --out "$S/meas/fam-probe-$form.csv"
done
python3 -B "$S/score_meas.py" "$S/meas" "$W/docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv" > "$S/meas/scored.txt" 2>&1
echo "score rc=$?"
echo "CHAIN_A_DONE fail=$fail $(date -Is)"
