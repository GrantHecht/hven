#!/usr/bin/env bash
# WA-1 r7 mutation leg (../DECLARATION-WA1-r7.md; the copy of mutations_r5.sh with mutate_r7.py and the r7 sets): control, then every mutation of mutate_r7.py, each rebuilt in build-wa/rel
# (hven_qp_seam_tests; RS-6a layout) and run on the pinning tests; the tree is restored after each and rebuilt at the end. A mutation is KILLED iff at
# least one pinning test fails (or the run times out). MUST lists the must-kill ids, REC the recorded ones (declared with their argument).
# usage: mutations_r7.sh <out-dir> [MUST ids...] -- [REC ids...]   (defaults: the declaration's sets)
#   Run under: flock /tmp/box-build.lock setsid -w bash mutations_r7.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/wa1
OUT=${1:?out dir}; shift
L=$WT/docs/notes/data/2026-11-m8-wa1/legs
MUST="N1 N2 N3 N4 N5 N6 N8 N9 N10 N11 N12 N14 N15 N17 N18 N19 N22 N23"; REC="N20 N21"
if [ $# -gt 0 ]; then MUST=""; REC=""; t=MUST; for a in "$@"; do if [ "$a" = -- ]; then t=REC; continue; fi; if [ $t = MUST ]; then MUST="$MUST $a"; else REC="$REC $a"; fi; done; fi
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-wa/tmp GIT_CEILING_DIRECTORIES=$WT/build-wa/tmp
cd "$WT" || exit 1
git diff --quiet --ignore-submodules=all -- src include || { echo TREE_DIRTY_BEFORE; exit 2; }
for m in $MUST $REC; do python3 "$L/mutate_r7.py" "$m" > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY $m"; python3 "$L/mutate_r7.py" restore; exit 2; }; python3 "$L/mutate_r7.py" restore > /dev/null; done
echo "PREFLIGHT every mutation applies and restores: MUST=[$MUST] REC=[$REC]"
mkdir -p "$OUT"
B=$WT/build-wa/rel
TESTS='QpWa1AscentFootprint.*:QpW1AscentGuard.*:QpYrkRefusedExchange.*:QpPlatformRetake.YorknetRestorationQp26*'
run_one() { # id
  local id=$1
  [ "$id" = control ] || python3 "$L/mutate_r7.py" "$id" || return 9
  [ "$id" = control ] || git -C "$WT" diff -- src include > "$OUT/$id.diff"
  timeout 1500 ninja -C "$B" -j16 hven_qp_seam_tests > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate_r7.py" restore; return 8; }
  timeout 900 "$B/tests/qp/hven_qp_seam_tests" --gtest_filter="$TESTS" > "$OUT/$id.seam.log" 2>&1; local r=$?
  grep -a "^\[  FAILED  \] [A-Za-z]" "$OUT/$id.seam.log" | sed 's/^.*\] //; s/ (.*//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate_r7.py" restore > /dev/null
  echo "MUT $id rc=$r killers=$(wc -l < "$OUT/$id.killers.txt")"
  sed 's/^/   /' "$OUT/$id.killers.txt"
  if [ "$id" = control ]; then [ $r -eq 0 ]; else [ $r -ne 0 ]; fi
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in $MUST; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
for m in $REC; do run_one $m > "$OUT/$m.verdict.txt" 2>&1; echo "$m rc=$? (recorded); killers: $(wc -l < "$OUT/$m.killers.txt")"; done
timeout 1500 ninja -C "$B" -j16 hven_qp_seam_tests > "$OUT/final.build.log" 2>&1
git -C "$WT" diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
