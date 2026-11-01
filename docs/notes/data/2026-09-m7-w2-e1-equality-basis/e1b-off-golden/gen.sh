#!/usr/bin/env bash
# M7 W2 E1b T5: build the OFF golden generator against E1's pre-merge base 8fdeed3e
# (an extract: `git archive 8fdeed3e | tar -x`, dep/eigen + dep/fmt copied at the same
# submodule commits, tests/sqp/support/e1b_off_golden.h + tests/sqp/e1b_off_golden_gen.cpp
# added, and a 4-line add_executable appended to tests/sqp/CMakeLists.txt), in Release and
# Debug, and print each configuration's fingerprint. Run under /tmp/box-build.lock.
set -euo pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2e1b2
SRC=$S/base-8fdeed3e
for cfg in Release Debug; do
  B=$S/base-build-$cfg
  cmake -S $SRC -B $B -G Ninja -DCMAKE_BUILD_TYPE=$cfg -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DHVEN_BUILD_BENCH=OFF -DHVEN_BUILD_TESTS=ON \
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$R/build/_deps/googletest-src > $S/t5/cfg-$cfg.log 2>&1
  ninja -C $B e1b_off_golden_gen > $S/t5/build-$cfg.log 2>&1
  MKL_NUM_THREADS=1 $B/tests/sqp/e1b_off_golden_gen > $S/t5/golden-$cfg.txt
  echo "GEN_$cfg rc=0 lines=$(wc -l < $S/t5/golden-$cfg.txt)"
done
cmp $S/t5/golden-Release.txt $S/t5/golden-Debug.txt && echo "RELEASE_EQ_DEBUG" || echo "RELEASE_NE_DEBUG"
