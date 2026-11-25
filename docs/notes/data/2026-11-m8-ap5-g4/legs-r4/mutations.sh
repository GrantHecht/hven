#!/usr/bin/env bash
# M8 AP-5 G4 r4 (../DECLARATION-r4.md): copied from ../legs-r3/mutations.sh with -r3 -> -r4 and g4r3 -> g4r4 in code lines only; nothing else.
# M8 AP-5 G4 r3 (../DECLARATION-r3.md): copied from ../legs/mutations.sh; changes: legs-r3 build.sh and mutate.py, ids s..y z Z added, build-g4/mut wiped before the control, raw .scratch/g4r3-mutation. Older provenance lines below are historical.
# M8 AP-5 G4 (../DECLARATION.md): the mutation table, on the pattern of ../../2026-11-m8-ap5-g2/legs-r3/mutations.sh. A scratch
# copy of the code head (git archive, dep trees copied) is built Release (build-g4/mut) for hven_sqp_seam_tests,
# hven_drivers_tests and hven_interior_tests; each mutation (mutate.py) is applied to the COPY alone (the worktree is never
# touched), the three binaries are run in full, the failing test names are the kill list, and the mutated file is restored from
# the head.
# usage: mutations.sh <code-head sha> <out-dir>     Run as: flock /tmp/box-build.lock bash mutations.sh ...
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a2559003d3a33fac4
D=$WT/docs/notes/data/2026-11-m8-ap5-g4
HEAD_SHA=${1:?code head sha}
OUT=${2:?out dir}
RAW=$WT/.scratch/g4r4-mutation
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
export MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$WT/build-g4/tmp
cd "$WT" || exit 1
mkdir -p "$OUT" "$RAW"
SRC=$WT/build-g4/mut-src
# r3: the reused Release tree is wiped first. `git archive` stamps every file with the commit time, so an object built
# from a later edit of a file can look newer than the restored source and survive into the control build otherwise.
rm -rf "$WT/build-g4/mut" "$WT/build-g4/mut.configure.log"
rm -rf "$SRC"
mkdir -p "$SRC"
git archive "$HEAD_SHA" | tar -x -C "$SRC" || exit 1
rm -rf "$SRC/dep/eigen" "$SRC/dep/fmt"
cp -a dep/eigen "$SRC/dep/eigen" && cp -a dep/fmt "$SRC/dep/fmt" || exit 1
FILES="src/drivers/sqp_solver.cpp src/drivers/ipm_solver.cpp src/qp/qp_engine.cpp src/qp/ssn_engine.cpp src/qp/ipqp_engine.cpp"
build() { SRC=$SRC bash "$D/legs-r4/build.sh" Release mut hven_sqp_seam_tests hven_drivers_tests hven_interior_tests > "$RAW/build-$1.log" 2>&1; }
runall() {
  local B=$WT/build-g4/mut/tests
  { timeout 900 "$B/hven_sqp_seam_tests" --gtest_brief=1 2>&1; echo "seam rc=$?";
    timeout 900 "$B/drivers/hven_drivers_tests" --gtest_brief=1 2>&1; echo "drivers rc=$?";
    timeout 900 "$B/interior/hven_interior_tests" --gtest_brief=1 2>&1; echo "interior rc=$?"; } > "$RAW/run-$1.log"
  sed -nE 's/^\[  FAILED  \] ([A-Za-z0-9_]+\.[A-Za-z0-9_\/]+)( .*)?$/\1/p' "$RAW/run-$1.log" | sort -u
}
echo "id,mutation,build,killed_by" > "$OUT/mutation-table.csv"
declare -A DESC=(
 [a]="ipm ladder not handed the flag (no ladder poll)" [b]="ipm ladder stop without re-pushing the event's row"
 [c]="ipm ladder stop labelled a callback stop" [d]="walk poll ticks but the loop continues"
 [e]="ssn poll ticks but the loop continues" [f]="ipqp poll ticks but the loop continues"
 [g]="sqp no abandon after the main subproblem (check B)" [h]="sqp no abandon after the elastic ladder (check C)"
 [i]="sqp stopped row not restored to the exit test's row" [j]="sqp abandoned major still counted"
 [k]="sqp SOC re-solve left armed" [l]="sqp lazy SSN engine not armed" [m]="sqp lazy IPQP engine not armed"
 [n]="sqp SSN route runs on after a stop" [o]="sqp IPQP route runs on after a stop"
 [p]="sqp certified fallback runs on after a stop" [q]="sqp flag not cleared at solve exit"
 [r]="sqp abandon does not label the token"
 [s]="sqp stopped SSN subproblem's factorizations not charged" [t]="sqp stopped IPQP subproblem's factorizations not charged"
 [u]="sqp no stop check after the subproblem build" [v]="sqp SSN warm grade: no stop check inside the grade"
 [w]="sqp walk not re-armed after the SOC re-solve" [x]="sqp abandon keeps the abandoned kernel's row mode"
 [y]="sqp SSN warm grade: the routing lambda's stop check off"
 [z]="ipm best scoring removed from honour_stop (the ladder exit's included)"
 [Z]="sqp stop exit substitutes the best on a token-labelled exit (kStopReturnBest past the token label)")
build control; echo "control build rc=$?"
ctl=$(runall control | tr '\n' ' '); ctl="$ctl$(grep -E '^(seam|drivers|interior) rc=[1-9]' "$RAW/run-control.log" | tr '\n' ' ')"
echo "CONTROL failures: [${ctl}]"
echo "control,none,ok,${ctl:-NONE}" >> "$OUT/mutation-table.csv"
for id in a b c d e f g h i j k l m n o p q r s t u v w x y z Z; do
  python3 "$D/legs-r4/mutate.py" "$SRC" "$id" || { echo "MUTATE_FAIL $id"; echo "$id,${DESC[$id]},APPLY_FAIL," >> "$OUT/mutation-table.csv"; continue; }
  if build "$id"; then
    killed=$(runall "$id" | tr '\n' ' ')
    killed="$killed$(grep -E '^(seam|drivers|interior) rc=[1-9]' "$RAW/run-$id.log" | tr '\n' ' ')"
    echo "$id,${DESC[$id]},ok,${killed:-SURVIVES}" >> "$OUT/mutation-table.csv"
    echo "MUT $id: ${killed:-SURVIVES}"
  else
    echo "$id,${DESC[$id]},BUILD_FAIL," >> "$OUT/mutation-table.csv"; echo "MUT $id BUILD_FAIL"
  fi
  for f in $FILES; do git show "$HEAD_SHA:$f" > "$SRC/$f"; done
done
for f in $FILES; do git show "$HEAD_SHA:$f" | diff -q - "$SRC/$f" > /dev/null || echo "RESTORE_MISMATCH $f"; done
git -C "$WT" diff --quiet "$HEAD_SHA" -- src include tests bench || echo "WORKTREE_MOVED"
echo "FOREGROUND_END $(date -Is)"
