#!/bin/bash
# W5 T8.9r-attrib3 -- the six intra-T8.4 arms, each a `git archive` extraction built
# from an EMPTY build directory at ONE absolute path. The flags are the T8.9r
# artifact's own scripts/build_arms.sh flags, verbatim.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
. $R/arms.sh
REPO=/home/ghecht/Projects/hven
LOG=$R/logs/B1-build.log
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/build.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) build"
for a in $ARMS; do
  NN=${a%%:*}; rest=${a#*:}; TAG=${rest%%:*}; SHA=${rest#*:}
  echo "=== arm b$NN $TAG $SHA extract $(utc) ==="
  rm -rf $R/arm $R/build
  mkdir -p $R/arm
  ( cd $REPO && git archive $SHA ) | tar -x -C $R/arm
  fold $?
  # the submodules are NOT in the archive; link the repo's own pinned checkouts,
  # which are the two commits attribution/arms.txt names.
  mkdir -p $R/arm/dep
  for d in eigen fmt; do
    rm -rf $R/arm/dep/$d
    cp -a $REPO/dep/$d $R/arm/dep/$d
    fold $?
  done
  echo "SUBMODULE_PIN eigen $(cd $REPO/dep/eigen && git rev-parse HEAD)"
  echo "SUBMODULE_PIN fmt   $(cd $REPO/dep/fmt   && git rev-parse HEAD)"
  echo "=== configure b$NN $(utc) ==="
  CCACHE_DISABLE=1 cmake -S $R/arm -B $R/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=ON -DHVEN_A4_GATE=OFF 2>&1 | tail -20
  fold ${PIPESTATUS[0]}
  echo "=== build b$NN $(utc) ==="
  CCACHE_DISABLE=1 cmake --build $R/build --target hven_sqp_corpus -j 8 2>&1 | tail -15
  fold ${PIPESTATUS[0]}
  mkdir -p $ARMBIN/$SHA
  cp $R/build/bench/hven_sqp_corpus $ARMBIN/$SHA/hven_sqp_corpus; fold $?
  cp $R/build/libhven.a            $ARMBIN/$SHA/libhven.a;        fold $?
  # the IPM driver's own object, kept for the symbol/size reading of Step B
  OBJ=$(find $R/build -name 'interior_point_solver.cpp.o' | head -1)
  [ -n "$OBJ" ] && cp "$OBJ" $ARMBIN/$SHA/interior_point_solver.cpp.o
  echo "OBJPATH b$NN ${OBJ:-NONE}"
  echo "SHA256 b$NN $TAG $SHA exe $(sha256sum $ARMBIN/$SHA/hven_sqp_corpus | cut -d' ' -f1)"
  echo "SHA256 b$NN $TAG $SHA lib $(sha256sum $ARMBIN/$SHA/libhven.a      | cut -d' ' -f1)"
  echo "SRCCOUNT b$NN $(grep -c '\.cpp' $R/arm/src/CMakeLists.txt)"
  echo "=== done b$NN rc_so_far=$LEG_RC $(utc) ==="
done
echo "FOREGROUND_END $(utc) build"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
