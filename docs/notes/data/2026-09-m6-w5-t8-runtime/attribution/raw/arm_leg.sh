#!/bin/bash
# W5 T8.9r-attrib: ONE arm's whole leg -- extract, build, hash, retain, perf.
# $1 = NN (01..11)   $2 = sha   $3 = label (t8-0base, t8-1, ...)
# Invoked ALWAYS as: flock /tmp/box-build.lock bash arm_leg.sh NN SHA LABEL
#
# LAYOUT LOCK. The measured child's instruction count carries a process-layout
# term (see the leg's reading): the count is BIMODAL in the total byte
# footprint of the child's argv+environ, the two clusters sitting ~1.2e-4
# apart on the ipm cells. Every arm therefore runs a child whose argv is
# byte-identical except for the two digits of its arm number, from a shell at
# a fixed cwd and a fixed nesting depth. The "_argv_pad_12" component is the
# 12 bytes that make this leg's total argv footprint reproduce the T8.9r
# artifact's, so the absolute counts are comparable with it cell by cell.
cd / && cd /home/ghecht/Projects/hven || exit 70
R=/home/ghecht/Projects/hven/.scratch/w5t89ra
REPO=/home/ghecht/Projects/hven
. $R/common.sh
NN=${1:?NN} SHA=${2:?sha} LABEL=${3:?label}
LOG=$R/logs/A$NN-$LABEL.log
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
CELLS="f7_n1000_bound_neutral f7_n5000_bound_neutral f7_n20000_bound_neutral"
MODES="ipm ssn walk"
ROUNDS="1 2 3 4 5"
EIGEN=bc3b39870ecb690a623a3f49149a358b95c5781d
FMT=407c905e45ad75fc29bf0f9bb7c5c2fd3475976f
OUT=$R/art/raw/a$NN
CSV=$R/art/csv/a${NN}_argv_pad_12
mkdir -p $OUT $CSV $R/bin/$SHA
{
echo "ARM $NN  label=$LABEL  sha=$SHA"
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock, verbatim:"
cat $R/logs/A$NN-pgrep-separate.txt
echo "--- inside lock ---"
box_pgrep
echo "FOREGROUND_START $(utc) arm=$NN sha=$SHA"
echo "ENV_FOOTPRINT bytes=$(env | wc -c) md5=$(env | md5sum | cut -d' ' -f1) pwd=$PWD oldpwd=$OLDPWD shlvl=$SHLVL"

echo "=== submodule gitlink check $(utc) ==="
GOTE=$(git -C $REPO rev-parse $SHA:dep/eigen); fold $?
GOTF=$(git -C $REPO rev-parse $SHA:dep/fmt);   fold $?
echo "arm eigen=$GOTE"
echo "arm fmt=$GOTF"
if [ "$GOTE" != "$EIGEN" ] || [ "$GOTF" != "$FMT" ]; then
  echo "SUBMODULE PIN MISMATCH -- arm does not name the artifact's two commits"; fold 9
else
  echo "SUBMODULE PIN OK -- same two commits as the T8.9r artifact"
fi

echo "=== extract $(utc) ==="
rm -rf $R/arm; mkdir -p $R/arm
git -C $REPO archive --format=tar $SHA | tar -x -C $R/arm
fold ${PIPESTATUS[0]}; fold ${PIPESTATUS[1]}
mkdir -p $R/arm/dep/eigen $R/arm/dep/fmt
git -C $REPO/dep/eigen archive --format=tar $EIGEN | tar -x -C $R/arm/dep/eigen
fold ${PIPESTATUS[0]}; fold ${PIPESTATUS[1]}
git -C $REPO/dep/fmt archive --format=tar $FMT | tar -x -C $R/arm/dep/fmt
fold ${PIPESTATUS[0]}; fold ${PIPESTATUS[1]}
echo "extracted: $(find $R/arm -type f | wc -l) files; .git entries present: $(ls -a $R/arm | grep -c '^\.git$')"

echo "=== configure $(utc) ==="
rm -rf $R/build; mkdir -p $R/build
CCACHE_DISABLE=1 cmake -S $R/arm -B $R/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DHVEN_BUILD_TESTS=OFF -DHVEN_BUILD_BENCH=ON -DHVEN_A4_GATE=OFF 2>&1 | tail -20
fold ${PIPESTATUS[0]}
echo "=== build $(utc) ==="
CCACHE_DISABLE=1 cmake --build $R/build --target hven_sqp_corpus -j 8 2>&1 | tail -15
fold ${PIPESTATUS[0]}

echo "=== hashes $(utc) ==="
sha256sum $R/build/bench/hven_sqp_corpus $R/build/libhven.a
fold $?
echo "=== retain binaries under bin/$SHA $(utc) ==="
run cp $R/build/bench/hven_sqp_corpus $R/bin/$SHA/hven_sqp_corpus
run cp $R/build/libhven.a $R/bin/$SHA/libhven.a

echo "=== perf: 5 rounds x 3 modes x 3 cells $(utc) ==="
echo "events: $PA"
BIN=$R/build/bench/hven_sqp_corpus
echo "child argv byte length (ipm/n1000/r1): $(printf '%s --internal-run-one f7_n1000_bound_neutral --engine ipm --internal-out %s/ipm-f7_n1000_bound_neutral-r1.csv' $BIN $CSV | wc -c)"
for r in $ROUNDS; do
  for mode in $MODES; do
    for cell in $CELLS; do
      echo "RUN  $(utc) arm=$NN round=$r mode=$mode cell=$cell"
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
          perf stat -e $PA -o $OUT/$mode-$cell-r$r.txt -- \
          $BIN --internal-run-one $cell --engine $mode \
               --internal-out $CSV/$mode-$cell-r$r.csv \
          > /dev/null 2>&1
      fold $?
    done
  done
done
echo "perf files written: $(ls $OUT | wc -l)   csv rows written: $(ls $CSV | wc -l)"
echo "FOREGROUND_END $(utc) arm=$NN"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
