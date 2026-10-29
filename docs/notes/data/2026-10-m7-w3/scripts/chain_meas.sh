#!/bin/bash
# W3 in-tree measurement chain (design page s5.1-5.4). Every solver run its own leg under the box lock.
set -o pipefail
W=/home/ghecht/Projects/hven/.claude/worktrees/w3
S=$W/.scratch/w3
LEG=$S/scripts/leg.sh
M=$S/meas; mkdir -p $M
SUM=$S/logs/chain-meas.txt
TC=$W/docs/notes/data/2026-10-m7-toolchain-2026-10-27
HB=$S/build-head-rel/bench; BB=$S/build-base-rel/bench; FB=$S/build-farkas-rel/bench
T10=$S/build-tau10-rel/bench; T1E3=$S/build-tau1e3-rel/bench
U0="$(grep -v '^#' $W/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt | tr -d '\n')"
C44="$(grep -v '^#' $W/docs/notes/data/2026-10-m7-w3/corpus44-cells.txt | tr -d '\n')"
echo "MEAS CHAIN START $(date -Is) pid=$$" | tee -a $SUM
run() { local n=$1; shift; $LEG "$n" "$@" >> $SUM 2>&1; echo "$n rc=$? $(date -Is)" | tee -a $SUM; }
corpus() { # BIN ENGINE CELLS OUT [levers...]
    local bin=$1 eng=$2 cells=$3 out=$4; shift 4
    taskset -c 2 "$bin" --engine "$eng" --cells "$cells" --csv "$out" "$@"; }
hs() { # BIN ENGINE OUT [levers...]
    local bin=$1 eng=$2 out=$3; shift 3
    taskset -c 2 "$bin" --hs --engine "$eng" --csv "$out" "$@"; }
export -f corpus hs
FAR="--ssn-infeasibility-rule farkas"; R1="--ssn-sigma-rule residual-armed"; R5="--ssn-certify-from-face"

# --- G1 U0 (head, three engines) ; T-a's head side is head-walk
for e in walk ssn ipm; do run g1-u0-$e corpus $HB/hven_sqp_corpus $e "$U0" $M/u0-head-$e.csv; done
# --- G2 E1 OFF
run g2-e1 taskset -c 2 $HB/hven_sqp_ipqp_e1_arm --gate --out $M/e1-head-off.csv
# --- G-H1 (HS default levers, head vs base, walk + ssn)
for e in walk ssn; do
    run gh1-hs-head-$e hs $HB/hven_sqp_corpus $e $M/hs-head-$e-off.csv
    run gh1-hs-base-$e hs $BB/hven_sqp_corpus $e $M/hs-base-$e-off.csv
done
# --- R arms, head, corpus-44 + HS-27 (ssn). HS OFF = gh1's hs-head-ssn-off.
run c44-head-off corpus $HB/hven_sqp_corpus ssn "$C44" $M/c44-head-off.csv
run c44-head-r1  corpus $HB/hven_sqp_corpus ssn "$C44" $M/c44-head-r1.csv $R1
run c44-head-r4  corpus $HB/hven_sqp_corpus ssn "$C44" $M/c44-head-r4.csv $FAR
run c44-head-r5  corpus $HB/hven_sqp_corpus ssn "$C44" $M/c44-head-r5.csv $R5
run hs-head-r1 hs $HB/hven_sqp_corpus ssn $M/hs-head-ssn-r1.csv $R1
run hs-head-r4 hs $HB/hven_sqp_corpus ssn $M/hs-head-ssn-r4.csv $FAR
run hs-head-r5 hs $HB/hven_sqp_corpus ssn $M/hs-head-ssn-r5.csv $R5
# --- R4 at 1e-4 (scratch farkas1e-4) + its OFF control (G-S)
run c44-f4-off corpus $FB/hven_sqp_corpus ssn "$C44" $M/c44-f4-off.csv
run c44-f4-r4  corpus $FB/hven_sqp_corpus ssn "$C44" $M/c44-f4-r4.csv $FAR
run hs-f4-off hs $FB/hven_sqp_corpus ssn $M/hs-f4-ssn-off.csv
run hs-f4-r4  hs $FB/hven_sqp_corpus ssn $M/hs-f4-ssn-r4.csv $FAR
# --- probe (P7-B count + seed)
run probe-head-farkas     bash -c "taskset -c 2 $HB/hven_sqp_ssn_safeguard_probe lever-farkas 10000 20260807 > $M/probe-head-lever-farkas.txt"
run probe-f4-farkas       bash -c "taskset -c 2 $FB/hven_sqp_ssn_safeguard_probe lever-farkas 10000 20260807 > $M/probe-f4-lever-farkas.txt"
run probe-head-farkas-m11 bash -c "taskset -c 2 $HB/hven_sqp_ssn_safeguard_probe lever-farkas-m11 10000 20260807 > $M/probe-head-lever-farkas-m11.txt"
run probe-f4-farkas-m11   bash -c "taskset -c 2 $FB/hven_sqp_ssn_safeguard_probe lever-farkas-m11 10000 20260807 > $M/probe-f4-lever-farkas-m11.txt"
run probe-head-sigma      bash -c "taskset -c 2 $HB/hven_sqp_ssn_safeguard_probe lever-sigma 10000 20260807 > $M/probe-head-lever-sigma.txt"
# --- tau in-tree (walk): U0 + HS-27 at T10/T1e3 (head side = u0-head-walk, hs-head-walk-off)
for t in tau10:$T10 tau1e3:$T1E3; do n=${t%%:*}; b=${t#*:}
    run u0-$n-walk corpus $b/hven_sqp_corpus walk "$U0" $M/u0-$n-walk.csv
    run hs-$n-walk hs $b/hven_sqp_corpus walk $M/hs-$n-walk.csv
done
# --- tau probe: in-tree binaries (head = 100, the two scratch builds), then the recipe's five points
run tauprobe-head   bash -c "taskset -c 2 $HB/hven_sqp_tau_bar_sweep_probe > $M/tauprobe-intree-head.txt"
run tauprobe-tau10  bash -c "taskset -c 2 $T10/hven_sqp_tau_bar_sweep_probe > $M/tauprobe-intree-tau10.txt"
run tauprobe-tau1e3 bash -c "taskset -c 2 $T1E3/hven_sqp_tau_bar_sweep_probe > $M/tauprobe-intree-tau1e3.txt"
run tauprobe-recipe bash $S/scripts/tau_recipe.sh
echo "MEAS CHAIN END $(date -Is)" | tee -a $SUM
# --- G3 Release suite at the head (ctest -j4 under setsid)
run g3-suite-rel setsid ctest --test-dir $S/build-head-rel -j4 --no-tests=error --output-on-failure
echo "MEAS CHAIN END2 $(date -Is)" | tee -a $SUM
