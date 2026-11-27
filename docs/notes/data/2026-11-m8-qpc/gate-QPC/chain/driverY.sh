#!/usr/bin/env bash
# QPC driver Y (runner glue): the legs that produce no TRAIN/CAP/NLP-46 ensemble rows, run after the E-R commit:
# C (ctest Release+Debug), N1 (replay), M (mutations), M1R (diagnostic leg; moved AFTER the E-R commit -- it is the one declared-
# early leg that produces TRAIN-238 member-0 rows), Sm (install smoke, both front ends).  Each its own box-lane acquisition.
set -o pipefail
source "$(dirname "$0")/env.sh"
source "$(dirname "$0")/r2_checks.sh"   # DECLARATION-QPC-r2 r2-3
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
# N3 (CENSUS-57, C584(1)): H and R each on the M7-close baseline with the repo's run_walk_census.sh + census_compare.py, whole leg --exclusive
# (deadline-status cells solo); R = 99712ec32 corpus binary built by N1 (the N1 base tree), H = the qpc tree's.
n3run() { # n3run <label> <binary> <out>
  rm -rf $3; mkdir -p $3
  if [ $1 = H ]; then export HVEN_BENCH_QPC_SIDE=$3/qpc-side.csv; else unset HVEN_BENCH_QPC_SIDE; fi
  echo "binary $2 sha256 $(sha256sum $2 | cut -d' ' -f1)"
  bash $W/scripts/run_walk_census.sh --binary $2 --out $3/run --baseline $W/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv --compare $W/scripts/census_compare.py
  local rc=$?; for f in compare.txt serial_confirm_list.txt walk_census.csv; do [ -f $3/run/$f ] && cp $3/run/$f $3/$f; done; return $rc
}
leg N3H ~/.local/bin/box-lane --exclusive -- bash -c "$(declare -f n3run); W=$W; n3run H $W/.scratch/build-rel/bench/hven_sqp_corpus $G/gate-QPC/N3/H"
leg N3R ~/.local/bin/box-lane --exclusive -- bash -c "$(declare -f n3run); W=$W; n3run R $RUN/R-src/.scratch/build-rel/bench/hven_sqp_corpus $G/gate-QPC/N3/R"
leg N3  python3 $G/gate-QPC/chain/n3_report.py $G/gate-QPC/N3/H $G/gate-QPC/N3/R
leg M   ~/.local/bin/box-lane -- bash $G/legs/mutations_qpc.sh $G/gate-QPC/M $W $W/.scratch/build-rel
r2_check_decoded     # r2-3(a)
leg M1R ~/.local/bin/box-lane -- bash $G/legs/legM1R-r2.sh $H_PIN $HB $RUN/m1r-work $G/gate-QPC/M1R
export HVEN_INSTALL_SMOKE_WORKDIR=$RUN/smoke-gcc
leg SmG ~/.local/bin/box-lane -- bash $W/scripts/check_install_smoke.sh
export HVEN_INSTALL_SMOKE_WORKDIR=$RUN/smoke-clang
leg SmC ~/.local/bin/box-lane -- bash $W/scripts/check_install_smoke.sh -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang
gcommit "docs(qpc): legs C, N1, M, M1R, Sm evidence (raw outputs)" docs/notes/data/2026-11-m8-qpc/gate-QPC || echo "commit failed (resolve by hand)"
touch $RUN/Y.ALLDONE
maybe_launch_D
echo "Y done $(date -Is)"
