#!/usr/bin/env bash
# PF-2b mutation leg (../DECLARATION-PF-2b.md): control, then every MUST mutation of mutate.py, each rebuilt in
# .scratch/build-pf2b/rel (the two test executables below) and run on the pinning tests; the tree is restored after each and
# rebuilt at the end. KILLED iff a pinning test fails. Run under: ~/.local/bin/box-lane -- bash mutations.sh <out> [ids...] [-- REC ids]
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf2b
OUT=${1:?out dir}; shift
L=$WT/docs/notes/data/2026-11-m8-pf2b/legs
MUST="M1 M2 M3 M4 M5 S1 S2 S3 S4 A1 A2 A3 A4 A5 C1 C2 C3 C4 C5 C6 C7 C8 C9 C10 C11 C12"; REC=""
if [ $# -gt 0 ]; then MUST=""; REC=""; t=MUST; for a in "$@"; do if [ "$a" = -- ]; then t=REC; continue; fi; if [ $t = MUST ]; then MUST="$MUST $a"; else REC="$REC $a"; fi; done; fi
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/tmp GIT_CEILING_DIRECTORIES=$WT/.scratch/tmp
cd "$WT" || exit 1
git diff --quiet --ignore-submodules=all -- src include || { echo TREE_DIRTY_BEFORE; exit 2; }
for m in $MUST $REC; do python3 "$L/mutate.py" "$m" > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY $m"; python3 "$L/mutate.py" restore; exit 2; }; python3 "$L/mutate.py" restore > /dev/null; done
echo "PREFLIGHT every mutation applies and restores: MUST=[$MUST] REC=[$REC]"
OUT=$(realpath -m "$OUT"); mkdir -p "$OUT"
B=$WT/.scratch/build-pf2b/rel
TARGETS="hven_linear_tests hven_sqp_tests"
run_one() { # id
  local id=$1 r=0 rr
  [ "$id" = control ] || python3 "$L/mutate.py" "$id" || return 9
  [ "$id" = control ] || git -C "$WT" diff -- src include > "$OUT/$id.diff"
  timeout 2400 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate.py" restore; return 8; }
  : > "$OUT/$id.tests.log"
  timeout 600 "$B/tests/linear/hven_linear_tests" --gtest_filter='SolveTally.*' >> "$OUT/$id.tests.log" 2>&1; rr=$?; [ $rr -ne 0 ] && r=$rr
  timeout 600 "$B/tests/sqp/hven_sqp_tests" --gtest_filter='SqpBackendSolveTally.*' >> "$OUT/$id.tests.log" 2>&1; rr=$?; [ $rr -ne 0 ] && r=$rr
  grep -a "^\[  FAILED  \] [A-Za-z]" "$OUT/$id.tests.log" | sed 's/^.*\] //; s/ (.*//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate.py" restore > /dev/null
  echo "MUT $id rc=$r killers=$(wc -l < "$OUT/$id.killers.txt")"
  sed 's/^/   /' "$OUT/$id.killers.txt"
  if [ "$id" = control ]; then [ $r -eq 0 ]; else [ $r -ne 0 ]; fi
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in $MUST; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
for m in $REC; do run_one $m > "$OUT/$m.verdict.txt" 2>&1; echo "$m rc=$? (recorded); killers: $(wc -l < "$OUT/$m.killers.txt")"; done
timeout 2400 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/final.build.log" 2>&1
git -C "$WT" diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
