#!/bin/bash
# W5 T8.9r-attrib4 -- the three built arms. Each is a `git archive` extraction of
# e51a7e0, with ONE patch applied or none, built from an EMPTY build directory at
# ONE absolute path by the T8.9r artifact's build_arms.sh flags, verbatim.
R=/home/ghecht/Projects/hven/.scratch/w5t89ra4
. $R/common.sh
. $R/arms_build.sh
REPO=/home/ghecht/Projects/hven
SHA=e51a7e0
LOG=$R/logs/B1-build.log
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/build.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) build"
for spec in $BUILD_ARMS; do
  NAME=${spec%%:*}; rest=${spec#*:}; BASE=${rest%%:*}; PATCHF=${rest#*:}
  echo "=== arm $NAME base=$BASE patch=$PATCHF extract $(utc) ==="
  rm -rf $R/arm $R/build
  mkdir -p $R/arm
  ( cd $REPO && git archive $BASE ) | tar -x -C $R/arm
  fold $?
  mkdir -p $R/arm/dep
  for d in eigen fmt; do
    rm -rf $R/arm/dep/$d
    cp -a $REPO/dep/$d $R/arm/dep/$d
    fold $?
  done
  echo "SUBMODULE_PIN eigen $(cd $REPO/dep/eigen && git rev-parse HEAD)"
  echo "SUBMODULE_PIN fmt   $(cd $REPO/dep/fmt   && git rev-parse HEAD)"
  if [ "$PATCHF" != NONE ]; then
    ( cd $R/arm && patch -p1 --batch < $R/exp/$PATCHF ); fold $?
    echo "PATCH_APPLIED $NAME $PATCHF $(sha256sum $R/exp/$PATCHF | cut -d' ' -f1)"
  else
    echo "PATCH_APPLIED $NAME NONE -- the recipe calibration arm"
  fi
  echo "=== configure $NAME $(utc) ==="
  CCACHE_DISABLE=1 cmake -S $R/arm -B $R/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DCMAKE_C_COMPILER=/usr/bin/clang \
    -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=ON -DHVEN_A4_GATE=OFF 2>&1 | tail -20
  fold ${PIPESTATUS[0]}
  echo "=== build $NAME $(utc) ==="
  CCACHE_DISABLE=1 cmake --build $R/build --target hven_sqp_corpus -j 8 2>&1 | tail -20
  fold ${PIPESTATUS[0]}
  mkdir -p $ARMBIN/$NAME
  cp $R/build/bench/hven_sqp_corpus $ARMBIN/$NAME/hven_sqp_corpus; fold $?
  cp $R/build/libhven.a            $ARMBIN/$NAME/libhven.a;        fold $?
  OBJ=$(find $R/build -name 'interior_point_solver.cpp.o' | head -1)
  [ -n "$OBJ" ] && cp "$OBJ" $ARMBIN/$NAME/interior_point_solver.cpp.o
  echo "SHA256 $NAME exe $(sha256sum $ARMBIN/$NAME/hven_sqp_corpus | cut -d' ' -f1)"
  echo "SHA256 $NAME lib $(sha256sum $ARMBIN/$NAME/libhven.a      | cut -d' ' -f1)"
  echo "SRCCOUNT $NAME $(grep -c '\.cpp' $R/arm/src/CMakeLists.txt)"
  echo "=== done $NAME rc_so_far=$LEG_RC $(utc) ==="
done
echo "FOREGROUND_END $(utc) build"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
