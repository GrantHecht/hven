#!/bin/bash
# W5 T8.9r-attrib3 -- experiment 3's three padded PARENT arms. Each is a `git archive`
# extraction of 8ae1618 with N bytes of unreachable .text inserted at the head of
# src/drivers/interior_point_solver.cpp, built by the same recipe from an EMPTY build
# directory at the same absolute path. 9712 is the exact amount the culprit grew that
# object by; 4096 and 16384 bracket it.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
REPO=/home/ghecht/Projects/hven
LOG=$R/logs/B5-build-exp3.log
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/build-exp3.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) build-exp3"
for N in 4096 9712 16384; do
  NAME=p$N
  echo "=== $NAME base=8ae1618 pad=$N extract $(utc) ==="
  rm -rf $R/arm $R/build; mkdir -p $R/arm
  ( cd $REPO && git archive 8ae1618 ) | tar -x -C $R/arm; fold $?
  mkdir -p $R/arm/dep
  for d in eigen fmt; do rm -rf $R/arm/dep/$d; cp -a $REPO/dep/$d $R/arm/dep/$d; fold $?; done
  ( cd $R/arm && patch -p1 --batch < $R/exp/experiment-3-$N.patch ); fold $?
  CCACHE_DISABLE=1 cmake -S $R/arm -B $R/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=ON -DHVEN_A4_GATE=OFF 2>&1 | tail -6
  fold ${PIPESTATUS[0]}
  CCACHE_DISABLE=1 cmake --build $R/build --target hven_sqp_corpus -j 8 2>&1 | tail -8
  fold ${PIPESTATUS[0]}
  mkdir -p $R/bin/$NAME
  cp $R/build/bench/hven_sqp_corpus $R/bin/$NAME/hven_sqp_corpus; fold $?
  OBJ=$(find $R/build -name 'interior_point_solver.cpp.o' | head -1)
  echo "OBJ_TEXT $NAME $(size $OBJ | tail -1 | awk '{print $1}')"
  echo "SHA256 $NAME exe $(sha256sum $R/bin/$NAME/hven_sqp_corpus | cut -d' ' -f1)"
  echo "=== done $NAME rc_so_far=$LEG_RC $(utc) ==="
done
echo "FOREGROUND_END $(utc) build-exp3"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
