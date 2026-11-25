#!/usr/bin/env bash
# configure-only for: base (m8 331b31837), int (pf6 default), seq (pf6 HVEN_MKL_THREADING=sequential); then dump the link/compile lines for diffing.
set -u
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf6; L=$WT/docs/notes/data/2026-11-m8-pf6/legs; O=$WT/docs/notes/data/2026-11-m8-pf6/configure; mkdir -p $O
export CONFIGURE_ONLY=1
bash $L/build.sh $WT/.scratch/base Release build-base || exit 1
HVEN_CMAKE_EXTRA="" bash $L/build.sh $WT Release build-int || exit 1
HVEN_CMAKE_EXTRA="-DHVEN_MKL_THREADING=sequential" bash $L/build.sh $WT Release build-seq || exit 1
for a in base:$WT/.scratch/base/build-base int:$WT/build-int seq:$WT/build-seq; do
  n=${a%%:*}; d=${a#*:}
  ninja -C $d -t commands bench/hven_sqp_corpus | tail -1 > $O/link-$n.txt
  ninja -C $d -t commands hven_tests | tail -1 > $O/link-tests-$n.txt
  grep -h "MKL\|OMP\|openmp" $d.configure.log > $O/configure-mkl-$n.txt
  ninja -C $d -t commands src/CMakeFiles/hven.dir/linear/symmetric_factor_mkl.cpp.o 2>/dev/null | tail -1 > $O/compile-$n.txt || true
done
echo CONFIG3_DONE
