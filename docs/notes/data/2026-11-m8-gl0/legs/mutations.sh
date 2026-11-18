#!/usr/bin/env bash
# mutation list and one more binary (hven_sqp_tests, filtered to the suites that read the R4/trace changes). A `git archive`
# copy of the code head is built Release; each mutation (mutate.py) is applied to the COPY alone (the worktree is never
# touched), the binaries are run, the failing test names are the kill list, and the mutated file is restored from the head.
# usage: mutations.sh <code-head sha> <out-dir>     Run as: flock /tmp/box-build.lock bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/gl0
D=$WT/docs/notes/data/2026-11-m8-gl0
HEAD_SHA=${1:?code head sha}
OUT=${2:?out dir}
RAW=${RAW_DIR:-$WT/.scratch/gl0-mutation}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/build-gl0/tmp GIT_CEILING_DIRECTORIES=$WT
cd "$WT" || exit 1
mkdir -p "$OUT" "$RAW"
SRC=$WT/.scratch/build-gl0/mut-src
rm -rf "$WT/.scratch/build-gl0/mut" "$WT/.scratch/build-gl0/mut.configure.log" "$SRC"
mkdir -p "$SRC"
git archive "$HEAD_SHA" | tar -x -C "$SRC" || exit 1
rm -rf "$SRC/dep/eigen" "$SRC/dep/fmt"
cp -a dep/eigen "$SRC/dep/eigen" && cp -a dep/fmt "$SRC/dep/fmt" || exit 1
FILES="src/sqp/sqp_solver.cpp src/sqp/globalization/funnel.cpp src/trace/trace_writer.cpp include/hven/detail/sqp/sqp_major_row.h"
build() { SRC=$SRC bash "$D/legs/build.sh" Release mut hven_drivers_tests hven_sqp_tests > "$RAW/build-$1.log" 2>&1; }
runall() {
  local B=$WT/.scratch/build-gl0/mut/tests
  {     timeout 900 "$B/solver/hven_drivers_tests" --gtest_brief=1 --gtest_fail_fast 2>&1; echo "drivers rc=$?";
    timeout 900 "$B/sqp/hven_sqp_tests" --gtest_brief=1 --gtest_fail_fast \
      --gtest_filter='SqpGl0.*:FunnelRejectCause.*:JsonLinesTraceSink.*:SqpConsole.*:*Counters*' 2>&1; echo "sqp rc=$?";
    true; } > "$RAW/run-$1.log"
  sed -nE 's/^\[  FAILED  \] ([A-Za-z0-9_]+\.[A-Za-z0-9_\/]+)( .*)?$/\1/p' "$RAW/run-$1.log" | sort -u
}
echo "id,mutation,build,killed_by" > "$OUT/mutation-table.csv"
declare -A DESC=(
 [c1]="ceiling rejection counted as Armijo" [c2]="Armijo failure counted as h-type funnel failure"
 [c3]="h-type funnel failure counted as ceiling" [c4]="routed QP failure counted as other"
 [c5]="strategy swaps its Armijo and h-type causes" [c6]="strategy reports no cause for a ceiling rejection"
 [f1]="h-type acceptance counted as f-type" [r1]="switching ratio unwired (not stored on the row)"
 [r2]="ratio divides by the trial h instead of the iterate h" [r3]="ratio drops the delta factor"
 [r4]="trace writer drops the switching_ratio key" [r5]="record builder drops the ratio"
 [h1]="histogram off by one" [h2]="histogram last bucket saturates one early"
 [h3]="routed-failure site never advances the run"
 [n1]="ratio call site restored to the plain division" [n1b]="helper zero-denominator branch removed" [n2]="rollback advances the histogram run" [x1]="cause read after the SOC judgement (reported)")
build control; echo "control build rc=$?"
ctl=$(runall control | tr '\n' ' '); ctl="$ctl$(grep -E '^(seam|drivers|interior|sqp) rc=[1-9]' "$RAW/run-control.log" | tr '\n' ' ')"
echo "CONTROL failures: [${ctl}]"
echo "control,none,ok,${ctl:-NONE}" >> "$OUT/mutation-table.csv"
for id in ${MUT_IDS:-c1 c2 c3 c4 c5 c6 f1 r1 r2 r3 r4 r5 h1 h2 h3 n1 n1b n2 x1}; do
  python3 "$D/legs/mutate.py" "$SRC" "$id" || { echo "MUTATE_FAIL $id"; echo "$id,${DESC[$id]},APPLY_FAIL," >> "$OUT/mutation-table.csv"; continue; }
  if build "$id"; then
    killed=$(runall "$id" | tr '\n' ' ')
    killed="$killed$(grep -E '^(seam|drivers|interior|sqp) rc=[1-9]' "$RAW/run-$id.log" | tr '\n' ' ')"
    echo "$id,${DESC[$id]//,/;},ok,${killed:-SURVIVES}" >> "$OUT/mutation-table.csv"
    echo "MUT $id: ${killed:-SURVIVES}"
  else
    echo "$id,${DESC[$id]},BUILD_FAIL," >> "$OUT/mutation-table.csv"; echo "MUT $id BUILD_FAIL"
  fi
  for f in $FILES; do git show "$HEAD_SHA:$f" > "$SRC/$f"; done
done
for f in $FILES; do git show "$HEAD_SHA:$f" | diff -q - "$SRC/$f" > /dev/null || echo "RESTORE_MISMATCH $f"; done
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench || echo "WORKTREE_MOVED"
echo "FOREGROUND_END $(date -Is)"
