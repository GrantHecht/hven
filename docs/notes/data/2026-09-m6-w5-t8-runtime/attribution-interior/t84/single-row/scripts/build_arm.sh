#!/bin/bash
# W5 T8.9r-attrib5: build ONE arm. Extraction and build at ONE absolute path,
# reused (and emptied) for every arm, so no arm's binary carries a different
# embedded source path from another's -- the T8.9r artifact's own recipe.
# $1 = NN  $2 = sha  $3 = expected libhven.a sha256
cd / && cd /home/ghecht/Projects/hven || exit 70
R=/home/ghecht/Projects/hven/.scratch/w5t89ra5
. $R/common.sh
NN=${1:?NN} SHA=${2:?sha} WANT=${3:?want}
LOG=$R/logs/A$NN-build-$SHA.log
{
echo "ARM $NN sha=$SHA"
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock; build/solve processes:"
grep -E 'cmake|ninja|ctest|hven_' $R/logs/A$NN-pgrep-separate.txt || echo PGREP_EMPTY_BUILD
echo "--- inside lock ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_' || echo PGREP_EMPTY_BUILD
echo "FOREGROUND_START $(utc) arm=$NN sha=$SHA"
echo "ENV_FOOTPRINT bytes=$(env | wc -c) md5=$(env | md5sum | cut -d' ' -f1) pwd=$PWD shlvl=$SHLVL"
echo "=== extract $(utc) ==="
run bash $R/extract.sh $SHA $R/arm
echo "=== apply the adapted lever $(utc) ==="
run python3 $R/adapt_arm.py $R/arm
echo "adapted diff vs this arm's own extraction (must equal adapted-p.patch):"
rm -rf $R/armcheck; run bash $R/extract.sh $SHA $R/armcheck
diff -u --label "a/bench" --label "b/bench" -r $R/armcheck/bench $R/arm/bench > $R/adapted-$SHA.patch
if cmp -s $R/adapted-$SHA.patch $R/adapted-p.patch; then
  echo "ADAPTED PATCH IDENTICAL to adapted-p.patch ($(wc -l < $R/adapted-$SHA.patch) lines)"
else
  echo "ADAPTED PATCH DIFFERS from adapted-p.patch"; fold 8
fi
rm -rf $R/armcheck
echo "=== configure $(utc) ==="
rm -rf $R/build-arm; mkdir -p $R/build-arm
CCACHE_DISABLE=1 cmake -S $R/arm -B $R/build-arm -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=ON -DHVEN_A4_GATE=OFF 2>&1 | tail -8
fold ${PIPESTATUS[0]}
echo "=== build $(utc) ==="
CCACHE_DISABLE=1 cmake --build $R/build-arm --target hven_sqp_corpus -j 8 2>&1 | tail -6
fold ${PIPESTATUS[0]}
echo "=== hashes $(utc) ==="
GOT=$(sha256sum $R/build-arm/libhven.a | cut -d' ' -f1)
echo "libhven.a  expected $WANT"
echo "libhven.a  measured $GOT"
if [ "$GOT" = "$WANT" ]; then echo "LIBHVEN MATCH -- the lever touches no library code"; else echo "LIBHVEN MISMATCH -- STOP"; fold 9; fi
sha256sum $R/build-arm/bench/hven_sqp_corpus
fold $?
mkdir -p $R/bin/$SHA
run cp $R/build-arm/bench/hven_sqp_corpus $R/bin/$SHA/hven_sqp_corpus
run cp $R/build-arm/libhven.a $R/bin/$SHA/libhven.a
echo "FOREGROUND_END $(utc) arm=$NN"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
rc=$LEG_RC; echo "WRAPPER_EXIT=$rc"; exit $rc
