#!/usr/bin/env bash
# QPC driver Y (runner glue): the legs that produce no TRAIN/CAP/NLP-46 ensemble rows, run after the E-R commit:
# C (ctest Release+Debug), N1 (replay), M (mutations), M1R (diagnostic leg; moved AFTER the E-R commit -- it is the one declared-
# early leg that produces TRAIN-238 member-0 rows), Sm (install smoke, both front ends).  Each its own box-lane acquisition.
set -o pipefail
source "$(dirname "$0")/env.sh"
mkdir -p $RUN/logs $G/gate-QPC
cd $W || exit 1
echo "Y start $(date -Is) pid=$$"
leg() { # leg <name> <cmd...>; marker file makes the driver resumable
  local n=$1; shift
  [ -f $RUN/Y-$n.done ] && { echo "skip $n"; return 0; }
  echo "== $n $(date -Is)"
  "$@" > $RUN/logs/Y-$n.log 2>&1; local rc=$?
  echo "== $n rc=$rc $(date -Is)"; tail -5 $RUN/logs/Y-$n.log
  cp $RUN/logs/Y-$n.log $G/gate-QPC/leg-$n.log
  if [ $rc -ne 0 ]; then echo $rc > $RUN/Y-$n.failed; gcommit "docs(qpc): leg $n FAILED evidence (rc=$rc)" docs/notes/data/2026-11-m8-qpc/gate-QPC/leg-$n.log; stop "leg $n rc=$rc"; fi
  touch $RUN/Y-$n.done
}
leg C   ~/.local/bin/box-lane -- bash $G/legs/legC.sh $W $G/gate-QPC/C
[ -d $RUN/R-src ] || git -C $W worktree add --detach $RUN/R-src $R_PIN > /dev/null 2>&1
leg N1  ~/.local/bin/box-lane -- bash $G/legs/legN1.sh $W $RUN/R-src $G/gate-QPC/N1
leg M   ~/.local/bin/box-lane -- bash $G/legs/mutations_qpc.sh $G/gate-QPC/M $W $W/.scratch/build-rel
leg M1R ~/.local/bin/box-lane -- bash $G/legs/legM1R.sh $H_PIN $HB $RUN/m1r-work $G/gate-QPC/M1R
export HVEN_INSTALL_SMOKE_WORKDIR=$RUN/smoke-gcc
leg SmG ~/.local/bin/box-lane -- bash $W/scripts/check_install_smoke.sh
export HVEN_INSTALL_SMOKE_WORKDIR=$RUN/smoke-clang
leg SmC ~/.local/bin/box-lane -- bash $W/scripts/check_install_smoke.sh -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang
gcommit "docs(qpc): legs C, N1, M, M1R, Sm evidence (raw outputs)" docs/notes/data/2026-11-m8-qpc/gate-QPC || echo "commit failed (resolve by hand)"
echo "Y done $(date -Is)"
