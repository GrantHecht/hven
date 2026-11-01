#!/usr/bin/env bash
# usage: hs.sh TAG [HVEN_D4_FIX value] -- the 21 hs-27 TRAIN problems (HOLDOUT never), sequential cells in ONE locked leg
set -o pipefail
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4; mkdir -p $D/hs
tag="$1"; fix="$2"; export D tag fix
$D/tools/leg.sh "hs-$tag" bash -c '
  cd /home/ghecht/Projects/hven-bench || exit 3
  for p in hs003 hs005 hs006 hs007 hs010 hs011 hs012 hs014 hs022 hs025 hs026 hs027 hs028 hs030 hs033 hs035 hs039 hs043 hs045 hs077 hs079; do
    env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 ${fix:+HVEN_D4_FIX=$fix} taskset -c 2 $D/bench-build/lanes/hven/hven_bench_cell --lane hven-sqp --problem $p --options default --budget-iters 100 > $D/hs/$tag-$p.out 2>/dev/null
    echo "$p rc=$? $(grep -m1 "^status=" $D/hs/$tag-$p.out) $(grep -m1 "^iterations=" $D/hs/$tag-$p.out) $(grep -m1 "^objective=" $D/hs/$tag-$p.out) $(grep -m1 "^side.qp_minor_iters=" $D/hs/$tag-$p.out) $(grep -m1 "^factorizations=" $D/hs/$tag-$p.out)"
  done' > $D/hs/$tag.summary 2>&1
grep -v "^PGREP\|^FOREG" $D/hs/$tag.summary | head -30
