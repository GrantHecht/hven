#!/usr/bin/env bash
# PF-2d gate chain (../DECLARATION-PF-2d.md section 3): P1 ctest rel+dbg, P2 replay x3 (A and B), P3 TRAIN A and B co-run
# + check, P4 mutations, P5 install smoke. Stops at the first failed leg. Run under: ~/.local/bin/box-lane -- bash orchestrate.sh
# usage: orchestrate.sh <code sha> <ctest count>
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf2d
D0=$WT/docs/notes/data/2026-11-m8-pf2d; D=$D0/gate; L=$D0/legs
CODE=${1:?code sha}; NTESTS=${2:?}
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS"
PATHS="src include tests bench examples bindings CMakeLists.txt cmake scripts"
git -C "$WT" diff --quiet --ignore-submodules=all "$CODE" HEAD -- $PATHS || { echo "CODE_PATHS_DIFFER_FROM $CODE"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no --ignore-submodules=all -- $PATHS)" ] || { echo CODE_PATHS_DIRTY; exit 2; }
chk() { [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$(grep -F "$2" $D0/BINARIES.txt | awk '{print $NF}')" ] || { echo "SHA_MISMATCH $1"; exit 4; }; }
chk $WT/.scratch/bin/A/hven_sqp_corpus "corpus A"; chk $WT/.scratch/bin/B/hven_sqp_corpus "corpus B"
chk $WT/.scratch/bin/A/hven_bench_cell "lane A"; chk $WT/.scratch/bin/B/hven_bench_cell "lane B"
mkdir -p $D/suites $D/replay $D/train/A $D/train/B $D/mutation $D/install-smoke
leg() { local name=$1; shift; echo "LEG $name start $(date -Is)"; "$@"; local rc=$?; echo "LEG $name rc=$rc $(date -Is)"
  if [ $rc -ne 0 ]; then echo "STOP at $name"; exit $rc; fi; }
suites_ok() { grep -q "100% tests passed, 0 tests failed out of $NTESTS" "$1" || { echo "P1_CRITERION_NOT_MET $1"; return 1; }; }
replay_run() { # A and B co-run, each one process pinned to its own physical core of the lane (counters only; CLAUDE.md section 7)
  read -r p1 p2 _ <<< "$BOX_LANE_PHYS"
  taskset -c $p1 bash $L/replay.sh $D/replay A $WT/.scratch/bin/A/hven_sqp_corpus 3 > $D/replay/run-A.out 2>&1 &
  local a=$!
  taskset -c $p2 bash $L/replay.sh $D/replay B $WT/.scratch/bin/B/hven_sqp_corpus 3 > $D/replay/run-B.out 2>&1 &
  local b=$!; wait $a; local ra=$?; wait $b; local rb=$?; echo "replay rc A=$ra B=$rb"; [ $ra -eq 0 ] && [ $rb -eq 0 ]; }
replay_ok() { local bad=0
  for K in 1 2 3; do for M in walk ssn ipm; do
    python3 $WT/scripts/compare_replay.py $D/replay/A-k$K-$M.csv $D/replay/B-k$K-$M.csv "A-vs-B-k$K-$M" > $D/replay/cmp-k$K-$M.txt 2>&1; tail -1 $D/replay/cmp-k$K-$M.txt
    grep -q "differences: 0$" $D/replay/cmp-k$K-$M.txt || bad=1
  done; done; [ $bad -eq 0 ] && echo REPLAY_AS_DECLARED; return $bad; }
train_run() { # A on phys core 1, B on phys core 2 of the lane, co-run
  read -r p1 p2 _ <<< "$BOX_LANE_PHYS"
  PIN_CORE=$p1 RPIN_CORE=$p1 bash $L/train_enum.sh $D/train/A $WT/.scratch/bin/A/hven_bench_cell $(grep -F "lane A" $D0/BINARIES.txt | awk '{print $NF}') > $D/train/enum-A.out 2>&1 &
  local a=$!
  PIN_CORE=$p2 RPIN_CORE=$p2 bash $L/train_enum.sh $D/train/B $WT/.scratch/bin/B/hven_bench_cell $(grep -F "lane B" $D0/BINARIES.txt | awk '{print $NF}') > $D/train/enum-B.out 2>&1 &
  local b=$!; wait $a; local ra=$?; wait $b; local rb=$?; echo "enum rc A=$ra B=$rb"; [ $ra -eq 0 ] && [ $rb -eq 0 ]; }
train_ok() { python3 $L/train_check.py $D/train/B $WT/docs/notes/data/2026-11-m8-dn3 $D/train/A > $D/train/train_check.txt 2>&1
  grep -E '^(SUMMARY|MOVER|  FAIL|REPORT)' $D/train/train_check.txt | head -40
  grep -q "^SUMMARY rows 476; movers vs arm A 0 .*missing/capped 0" $D/train/train_check.txt || { echo P3_ROWS_DIFFER_OR_MISSING; return 1; }
  python3 $L/instr_compare.py $D/train/A/cells $D/train/B/cells > $D/train/instr.txt 2>&1; grep '^TOTAL' $D/train/instr.txt
  python3 - $D/train/instr.txt <<'PY'
import re, sys
t = open(sys.argv[1]).read()
sqp = re.search(r"TOTAL hven-sqp\s+rows\s+(\d+)\s+A \S+\s+B \S+\s+B/A (\S+)", t)
ok = bool(sqp and int(sqp.group(1)) == 238 and float(sqp.group(2)) <= 1.005)
print("P3_INSTRUCTIONS " + ("sqp B/A %s <= 1.005" % sqp.group(2) if ok else "CRITERION_NOT_MET"))
sys.exit(0 if ok else 1)
PY
}
leg P1-rel bash $L/ctest.sh rel $D/suites/ctest-rel.log
leg P1-rel-criterion suites_ok $D/suites/ctest-rel.log
leg P1-dbg bash $L/ctest.sh dbg $D/suites/ctest-dbg.log
leg P1-dbg-criterion suites_ok $D/suites/ctest-dbg.log
leg P2 replay_run
leg P2-criterion replay_ok
leg P3 train_run
leg P3-criterion train_ok
leg P4 bash $L/mutations.sh $D/mutation
leg P5 bash $L/install_smoke.sh $D/install-smoke
echo ALL_LEGS_PASSED
echo "FOREGROUND_END $(date -Is)"
