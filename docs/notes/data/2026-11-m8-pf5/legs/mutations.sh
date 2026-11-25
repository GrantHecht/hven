#!/usr/bin/env bash
# PF-5 (../DECLARATION-PF-5.md): copy of ../../2026-11-m8-pf4/r2/legs/mutations.sh re-pointed to the pf5b worktree (paths only); older lines are historical.
# PF-4 r2 (../../DECLARATION-PF-4-r2.md): copy of ../../legs/mutations.sh re-pointed to the pf4-r2 worktree and r2/ (paths only); older lines are historical.
# PF-4 P5 mutation leg (../DECLARATION-PF-4.md; PF-5: its own ids, targets and filters): WA-1's mutations_r7.sh pattern. Control, then every mutation of mutate.py,
# each rebuilt in .scratch/build-pf5/rel (the four test executables below) and run on the pinning tests; the tree is
# restored after each and rebuilt at the end. A mutation is KILLED iff at least one pinning test fails (or the run times out).
# usage: mutations.sh <out-dir> [MUST ids...] -- [REC ids...]   (defaults: the declaration's sets)
#   Run under (r2): ~/.local/bin/box-lane -- bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf5b
OUT=${1:?out dir}; shift
L=$WT/docs/notes/data/2026-11-m8-pf5/legs
MUST="X1 D1 F1 F2 M1 M2 M3 C1 T1 Q1"; REC="S1"   # PF-5: S1 recorded (no SQP-level test pins the hit count)
if [ $# -gt 0 ]; then MUST=""; REC=""; t=MUST; for a in "$@"; do if [ "$a" = -- ]; then t=REC; continue; fi; if [ $t = MUST ]; then MUST="$MUST $a"; else REC="$REC $a"; fi; done; fi
[ -n "${BOX_LANE:-}" ] || { echo "NOT_IN_BOX_LANE (r2: every leg runs inside ~/.local/bin/box-lane)"; exit 3; }; echo "BOX_LANE=$BOX_LANE phys=[$BOX_LANE_PHYS] jobs=$BOX_LANE_JOBS"
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/.scratch/tmp GIT_CEILING_DIRECTORIES=$WT/.scratch/tmp
cd "$WT" || exit 1
git diff --quiet --ignore-submodules=all -- src include || { echo TREE_DIRTY_BEFORE; exit 2; }
for m in $MUST $REC; do python3 "$L/mutate.py" "$m" > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY $m"; python3 "$L/mutate.py" restore; exit 2; }; python3 "$L/mutate.py" restore > /dev/null; done
echo "PREFLIGHT every mutation applies and restores: MUST=[$MUST] REC=[$REC]"
OUT=$(realpath -m "$OUT"); mkdir -p "$OUT"
B=$WT/.scratch/build-pf5/rel
TARGETS="hven_kkt_tests hven_sqp_tests hven_qp_seam_tests"
run_one() { # id
  local id=$1 r=0 rr
  [ "$id" = control ] || python3 "$L/mutate.py" "$id" || return 9
  [ "$id" = control ] || git -C "$WT" diff -- src include > "$OUT/$id.diff"
  timeout 1500 ninja -C "$B" -j"${BOX_LANE_JOBS:-4}" $TARGETS > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate.py" restore; return 8; }
  : > "$OUT/$id.tests.log"
  timeout 600 "$B/tests/kkt/hven_kkt_tests" --gtest_filter='RetainedAnalyses.*:PF5/*:KktFactor.*' >> "$OUT/$id.tests.log" 2>&1; rr=$?; [ $rr -ne 0 ] && r=$rr
  timeout 900 "$B/tests/qp/hven_qp_seam_tests" --gtest_filter='QpRetainedAnalyses.*' >> "$OUT/$id.tests.log" 2>&1; rr=$?; [ $rr -ne 0 ] && r=$rr
  timeout 900 "$B/tests/sqp/hven_sqp_tests" --gtest_filter='SqpBackendAnalysisTally.*' >> "$OUT/$id.tests.log" 2>&1; rr=$?; [ $rr -ne 0 ] && r=$rr
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
timeout 1500 ninja -C "$B" -j"${BOX_LANE_JOBS:-4}" $TARGETS > "$OUT/final.build.log" 2>&1
git -C "$WT" diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
