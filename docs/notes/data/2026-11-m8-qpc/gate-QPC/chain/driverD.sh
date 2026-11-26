#!/usr/bin/env bash
# V-PDIV (leg D; REPORTED, never a bar; runs LAST, launched by the last finisher of the bar legs -- env.sh maybe_launch_D).
# qpc-pdiv = H (f0ba4e101) + the three pdiv diagnostic commits the declaration names (a70c79e34, 1b3b5e06d, c3b4b8f1d), a LOCAL branch,
# never pushed (worktree .scratch/qpcrun/pdiv-src). R side = 99712ec32 + the same three commits = pdiv commit c3b4b8f1d, whose install
# /home/ghecht/Projects/hven-bench-pdiv/.hven/install-c3b4b8f1ded9 already exists (read-only here). Lanes built with this harness's
# scripts/i19_build_lane.sh (ens 27c02687). Sample: the pdiv worktree's .scratch/runs/tasks1.tsv + tasks2.tsv, driven by its runcells.py.
# If the reproduction control fails the endpoint is reported VOID; no claim rests on D.
set -o pipefail
source "$(dirname "$0")/env.sh"
PD=/home/ghecht/Projects/hven/.claude/worktrees/pdiv/.scratch; D=$G/gate-QPC/D; OUT=$RUN/D; mkdir -p $D $OUT
cd $HB || exit 1
echo "D start $(date -Is) pid=$$"
QSHA=$(git -C $RUN/pdiv-src rev-parse HEAD); RSHA=c3b4b8f1ded9532148bdee7f2ff3769c57ac8416
git -C $W merge-base --is-ancestor $H_PIN $QSHA || { echo "D VOID: qpc-pdiv is not H + commits"; exit 3; }
grep -q "^HVEN_PIN_SHA $RSHA" /home/ghecht/Projects/hven-bench-pdiv/.hven/install-${RSHA:0:12}/HVEN_PIN.txt || { echo "D VOID: R-side install pin mismatch"; exit 3; }
echo "qpc-pdiv $QSHA ($(git -C $RUN/pdiv-src log --oneline $H_PIN..$QSHA | wc -l) commits on H)" | tee $D/builds.txt
git -C $RUN/pdiv-src log --format='%h %s' $H_PIN..$QSHA | cut -c1-120 >> $D/builds.txt
~/.local/bin/box-lane -- bash -c "bash $HB/scripts/i19_build_install.sh $QSHA $W $RUN/pdiv-src && bash $HB/scripts/i19_build_lane.sh PDQ $QSHA $HB/.hven/install-${QSHA:0:12} && bash $HB/scripts/i19_build_lane.sh PDR3 $RSHA /home/ghecht/Projects/hven-bench-pdiv/.hven/install-${RSHA:0:12}" > $RUN/logs/D-build.log 2>&1 \
  || { echo "D NOT RUN: diagnostic build failed ($RUN/logs/D-build.log)"; cp $RUN/logs/D-build.log $D/; exit 3; }
LQ=$HB/build-i19-PDQ/lanes/hven/hven_bench_cell; LR=$HB/build-i19-PDR3/lanes/hven/hven_bench_cell
{ echo "lane PDQ $(sha256sum $LQ | cut -d' ' -f1)"; echo "lane PDR3 $(sha256sum $LR | cut -d' ' -f1)"; } | tee -a $D/builds.txt
for side in R Q; do
  L=$LR; [ $side = Q ] && L=$LQ
  for t in tasks1 tasks2; do
    ~/.local/bin/box-lane -- python3 $PD/tools/runcells.py $L $PD/runs/$t.tsv $OUT/$side/$t > $RUN/logs/D-run-$side-$t.log 2>&1 || { echo "D: runcells $side $t failed"; }
  done
done
VOID=0
$PY $G/gate-QPC/chain/d_control.py $OUT/R/tasks1 $PD/runs/tasks1.tsv -- $STORE H0 > $D/control-R-tasks1.txt 2>&1 || VOID=1
$PY $G/gate-QPC/chain/d_control.py $OUT/R/tasks2 $PD/runs/tasks2.tsv -- $STORE H0 > $D/control-R-tasks2.txt 2>&1 || VOID=1
$PY $G/gate-QPC/chain/d_control.py $OUT/Q/tasks1 $PD/runs/tasks1.tsv -- $STORE QPC > $D/control-Q-tasks1.txt 2>&1 || VOID=1
$PY $G/gate-QPC/chain/d_control.py $OUT/Q/tasks2 $PD/runs/tasks2.tsv -- $STORE QPC > $D/control-Q-tasks2.txt 2>&1 || VOID=1
echo "reproduction control VOID=$VOID (1 = endpoint VOID)" | tee $D/CONTROL.txt
for side in R Q; do for t in tasks1 tasks2; do
  python3 $PD/tools/qpflip.py $OUT/$side/$t > $D/qpflip-$side-$t.txt 2>&1
  python3 $PD/tools/harm.py $OUT/$side/$t > $D/harm-$side-$t.txt 2>&1
  for tag in $(ls $OUT/$side/$t | grep '__H0\.row$' | sed 's/__H0\.row$//'); do for v in P X1 X2; do
    [ -f $OUT/$side/$t/${tag}__$v.jsonl ] && { echo "## $tag $v"; python3 $PD/tools/cmp.py $OUT/$side/$t $tag $v 2>&1; }
  done; done > $D/cmp-$side-$t.txt 2>&1
done; done
cp $RUN/logs/D-run-*.log $D/ 2>/dev/null
gcommit "docs(qpc): leg D (V-PDIV, reported) raw outputs; reproduction control VOID=$VOID" docs/notes/data/2026-11-m8-qpc/gate-QPC/D || echo "D commit failed"
touch $RUN/D.ALLDONE; echo "D done $(date -Is)"
