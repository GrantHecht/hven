#!/bin/bash
# W5 T8.9r-attrib2 -- THE DIFFERENCING INSTRUCTION LEG. ONE ROUND = ONE BATCH.
#
# WHY IT EXISTS. The wall localises the interior leg's +2.49 % to T8.4, and
# T8.4 is a ROW-ADDING pair (19 -> 21 rows: the two `solve_optimize` rows), so
# its WHOLE-PROCESS instruction count carries no verdict -- and the two added
# rows are ~1 % of the process's wall, the same size as the step itself, so the
# caveat is not academic.  Worse, this leg's whole-process counts scatter by up
# to 3.7 % run to run, and the scatter is entirely in the R3 POSITIONAL WARM-UP
# ROW, which the wall reading already excludes but which `perf stat` cannot.
#
# THE INSTRUMENT. Two processes per arm per round, back to back:
#   Q (subtrahend) --cells f7_n1000_bound_physics
#   P (minuend)    --cells <the four>
# P - Q is EXACTLY the nine large base rows (n5000 x3, n10000 x3, n20000 x3).
# Every unconditional row, every variant row AND the R3 warm-up row is in BOTH
# and cancels -- so the differenced quantity is LIKE-FOR-LIKE at all eleven
# arms even across the row-adding pairs, which is what the whole-process count
# is not.
#
# THIS IS NOT fix1's I4, AND THE DEFECT THAT SANK I4 IS THE ONE REPAIRED HERE.
# I4 differenced a cell process against a `--cells hs071_x1_fixed` process, so
# the subtrahend's FIRST solve was a DIFFERENT, TINY row that paid MKL's first
# call and the allocator's first growth which the minuend's first solve had
# already paid -- nine differenced quantities came out NEGATIVE. Here BOTH
# processes' first solve is the SAME row, f7_n1000_bound_physics/MakeParameter,
# in the same position, so that term is common and cancels by construction.
#
# THE FLOOR IS MEASURED, NOT ASSUMED. Arm 02 (T8.1) has a libhven.a
# BYTE-IDENTICAL to arm 01's (attribution/arms.txt), so its differenced step is
# a TRUE ZERO. Whatever it reads is this instrument's floor.
#
# ARGV LOCK. Q's cell spec is 71 bytes shorter than P's, so Q's --csv basename
# carries 71 bytes of padding: both children's argv footprint is then the same
# 263 bytes as the wall and pass-A legs', and the leg PRINTS both to prove it.
# $1 = round
R=/home/ghecht/Projects/hven/.scratch/w5t89ra2
. $R/common.sh
. $R/arms.sh
ROUND=${1:?round}
BATCH="diff-r$ROUND"
LOG=$R/logs/D$ROUND-$BATCH.log
PA=instructions:u,branches:u,cycles:u,branch-misses:u,L1-icache-load-misses:u
C1="f7_n1000_bound_physics"
PAD=xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
mkdir -p $R/art/csv $R/art/raw $R/art/perf
set -- $ARMS
N=$#
{
echo "PGREP_SEPARATE $(utc) -- taken OUTSIDE the lock by the caller, verbatim:"
cat $R/logs/$BATCH.pgrep
echo "--- inside the lock from here ---"
echo "PGREP_INLOCK $(utc)"
pgrep -af 'cmake|ninja|ctest|hven_|codex|clang' || echo PGREP_EMPTY
echo "FOREGROUND_START $(utc) diff round=$ROUND"
echo "events: $PA"
echo "ENV_FOOTPRINT bytes=$(env | wc -c) md5=$(env | md5sum | cut -d' ' -f1) pwd=$PWD shlvl=$SHLVL"
echo "PAD bytes=${#PAD}"
batch_start $BATCH
OFF=$((ROUND-1))
ORDER=""
for i in $(seq 0 $((N-1))); do
  j=$(( (i + OFF) % N + 1 )); eval "a=\${$j}"; ORDER="$ORDER $a"
done
echo "ARM_ORDER round=$ROUND:$ORDER"
for a in $ORDER; do
  NN=${a%%:*}; rest=${a#*:}; TASK=${rest%%:*}; SHA=${rest#*:}
  BIN=$ARMBIN/$SHA/hven_sqp_corpus
  PCSV=$R/art/csv/P-a$NN-r$ROUND.csv
  QCSV=$R/art/csv/Q-a$NN-r$ROUND$PAD.csv
  LP=$(printf '%s --engine interior --cells %s --csv %s' $BIN $CELLS $PCSV | wc -c)
  LQ=$(printf '%s --engine interior --cells %s --csv %s' $BIN $C1 $QCSV | wc -c)
  echo "ARGV_LEN arm=$NN P=$LP Q=$LQ"
  if [ "$LP" != "$LQ" ]; then echo "ARGV LOCK BROKEN at arm $NN"; fold 9; fi
  echo "RUN  $(utc) diffP arm=$NN task=$TASK sha=$SHA round=$ROUND"
  timed_run "$BATCH/a$NN-P" "$R/art/raw/P-a$NN-r$ROUND.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf stat -e $PA -o $R/art/perf/P-a$NN-r$ROUND.txt -- \
      $BIN --engine interior --cells $CELLS --csv $PCSV
  echo "RUN  $(utc) diffQ arm=$NN task=$TASK sha=$SHA round=$ROUND"
  timed_run "$BATCH/a$NN-Q" "$R/art/raw/Q-a$NN-r$ROUND.stdout" -- \
      env MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
      perf stat -e $PA -o $R/art/perf/Q-a$NN-r$ROUND.txt -- \
      $BIN --engine interior --cells $C1 --csv $QCSV
  echo "DONE $(utc) arm=$NN rc_so_far=$LEG_RC Prows=$(($(grep -vc '^#' $PCSV 2>/dev/null || echo 1)-1)) Qrows=$(($(grep -vc '^#' $QCSV 2>/dev/null || echo 1)-1))"
  box_guard "$BATCH/a$NN-after"
done
batch_end $BATCH
echo "FOREGROUND_END $(utc) diff round=$ROUND"
echo "WRAPPER_EXIT=$LEG_RC"
} > $LOG 2>&1
exit $LEG_RC
