#!/bin/bash
# W5 T8.9r-attrib3 -- the two EXPERIMENT arms. Each is a `git archive` extraction of
# its base commit with ONE patch applied, built by the SAME recipe, from an EMPTY
# build directory at the SAME absolute path the six bisect arms used.
#   x1 = 9cebbbe + experiment-1.patch  (the culprit, hot members put back at the
#        PARENT's offsets: result_ moved to the end of the class, a 320-byte pad
#        left where it stood)
#   x2 = 8ae1618 + experiment-2.patch  (the parent, hot members moved to the
#        CULPRIT's offsets: a 320-byte pad inserted before nlp_)
R=/home/ghecht/Projects/hven/.scratch/w5t89ra3
. $R/common.sh
REPO=/home/ghecht/Projects/hven
LOG=$R/logs/B4-build-exp.log
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/build-exp.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) build-exp"
for pair in x1:9cebbbe:experiment-1.patch x2:8ae1618:experiment-2.patch; do
  NAME=${pair%%:*}; rest=${pair#*:}; BASE=${rest%%:*}; PATCHF=${rest#*:}
  echo "=== $NAME base=$BASE patch=$PATCHF extract $(utc) ==="
  rm -rf $R/arm $R/build
  mkdir -p $R/arm
  ( cd $REPO && git archive $BASE ) | tar -x -C $R/arm
  fold $?
  mkdir -p $R/arm/dep
  for d in eigen fmt; do rm -rf $R/arm/dep/$d; cp -a $REPO/dep/$d $R/arm/dep/$d; fold $?; done
  ( cd $R/arm && patch -p1 --batch < $R/exp/$PATCHF ); fold $?
  echo "PATCH_APPLIED $NAME $(sha256sum $R/exp/$PATCHF | cut -d' ' -f1)"
  echo "=== configure $NAME $(utc) ==="
  CCACHE_DISABLE=1 cmake -S $R/arm -B $R/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
    -DCMAKE_C_COMPILER=/usr/bin/clang -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=ON \
    -DHVEN_A4_GATE=OFF 2>&1 | tail -10
  fold ${PIPESTATUS[0]}
  echo "=== build $NAME $(utc) ==="
  CCACHE_DISABLE=1 cmake --build $R/build --target hven_sqp_corpus -j 8 2>&1 | tail -25
  fold ${PIPESTATUS[0]}
  mkdir -p $R/bin/$NAME
  cp $R/build/bench/hven_sqp_corpus $R/bin/$NAME/hven_sqp_corpus; fold $?
  cp $R/build/libhven.a            $R/bin/$NAME/libhven.a;        fold $?
  OBJ=$(find $R/build -name 'interior_point_solver.cpp.o' | head -1)
  [ -n "$OBJ" ] && cp "$OBJ" $R/bin/$NAME/interior_point_solver.cpp.o
  echo "SHA256 $NAME exe $(sha256sum $R/bin/$NAME/hven_sqp_corpus | cut -d' ' -f1)"
  echo "SHA256 $NAME lib $(sha256sum $R/bin/$NAME/libhven.a | cut -d' ' -f1)"
  echo "=== done $NAME rc_so_far=$LEG_RC $(utc) ==="
done
echo "FOREGROUND_END $(utc) build-exp"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
