#!/bin/bash
# W5 T8.9r-attrib CONTROL: the process-layout sensitivity of instructions:u.
# The SAME binary (the base arm, 102f729, retained under bin/) on the SAME cell
# with the SAME pin, varying ONLY the byte length of the --internal-out path,
# which is the only part of the child's argv under our control. If the counts
# were layout-free every row here would agree; they do not -- they fall into two
# clusters ~1.2e-4 apart, which is why this leg locks the layout across arms.
# The retained binary path is 70 bytes, the same length as the build path every
# arm was measured from, so pad 12 here is pad 12 there.
cd / && cd /home/ghecht/Projects/hven || exit 70
R=/home/ghecht/Projects/hven/.scratch/w5t89ra
. $R/common.sh
LOG=$R/logs/A12-layout-probe.log
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
BIN=$R/bin/102f729/hven_sqp_corpus
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock, verbatim:"
cat $R/logs/A12-pgrep-separate.txt
echo "--- inside lock ---"
box_pgrep
echo "FOREGROUND_START $(utc) layout probe"
echo "ENV_FOOTPRINT bytes=$(env | wc -c) md5=$(env | md5sum | cut -d' ' -f1) pwd=$PWD oldpwd=$OLDPWD shlvl=$SHLVL"
echo "binary: $BIN  ($(printf %s $BIN | wc -c) bytes)  sha256 $(sha256sum $BIN | cut -c1-64)"
echo "the leg's own binary path was $R/build/bench/hven_sqp_corpus ($(printf %s $R/build/bench/hven_sqp_corpus | wc -c) bytes)"
for cell in f7_n1000_bound_neutral f7_n5000_bound_neutral; do
  for p in 0 4 8 12 16 20; do
    PAD=""
    [ $p -gt 0 ] && PAD=$(head -c $p /dev/zero | tr '\0' 'q')
    D=$R/art/layout/a01$PAD
    run mkdir -p $D
    echo "--- cell=$cell pad=$p outdir_len=$(printf %s $D | wc -c) ---"
    for r in 1 2 3; do
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
          perf stat -e $PA -o $R/art/layout/probe.txt -- \
          $BIN --internal-run-one $cell --engine ipm \
               --internal-out $D/ipm-$cell-r$r.csv > /dev/null 2>&1
      fold $?
      echo "    r$r $(grep -a 'instructions:u' $R/art/layout/probe.txt | awk '{print $1}')  branches $(grep -a 'branches:u' $R/art/layout/probe.txt | awk '{print $1}')"
    done
  done
done
echo "for reference, the T8.9r artifact's own base-arm medians:"
echo "  ipm f7_n1000_bound_neutral  749,104,475"
echo "  ipm f7_n5000_bound_neutral  3,834,858,737"
echo "FOREGROUND_END $(utc) layout probe"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
