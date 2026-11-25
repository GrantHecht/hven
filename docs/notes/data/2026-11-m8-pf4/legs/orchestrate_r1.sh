#!/usr/bin/env bash
# PF-4 r1 proof chain (../DECLARATION-PF-4-r1.md): orchestrate.sh with P1-P3 removed (run 1 passed them on this code and
# these binaries, gate/) and P4a calling ../runtime/run_walk_r1.sh; outputs under gate-r1/. P0 checks, P4a, P4b, P5, P6.
# usage: orchestrate.sh <code sha> <corpus A sha256> <corpus B sha256> <lane A sha256> <lane B sha256> <ctest count>
#   Run under: setsid -w flock /tmp/box-build.lock bash orchestrate.sh ... > ../gate-r1/orchestrate.out 2>&1
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4
D0=$WT/docs/notes/data/2026-11-m8-pf4
D=$D0/gate-r1
L=$D0/legs
CODE=${1:?code sha}; CA=${2:?}; CB=${3:?}; LA=${4:?}; LB=${5:?}; NTESTS=${6:?ctest count}
DEVR=$WT/.scratch/dev/replay; DEVA=$WT/.scratch/dev/train-A; DEVB=$WT/.scratch/dev/train-B
if [ -n "$(pgrep -x hven_bench_cell)" ]; then echo "PGREP_BUSY"; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
PATHS="src include tests bench examples bindings CMakeLists.txt cmake scripts"
git -C "$WT" diff --quiet --ignore-submodules=all "$CODE" HEAD -- $PATHS || { echo "CODE_PATHS_DIFFER_FROM $CODE"; exit 2; }
[ -z "$(git -C "$WT" status --porcelain --untracked-files=no --ignore-submodules=all -- $PATHS)" ] || { echo CODE_PATHS_DIRTY; exit 2; }
echo "code paths == $CODE, clean; HEAD $(git -C "$WT" rev-parse HEAD)"
chk() { [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ] || { echo "SHA_MISMATCH $1"; exit 4; }; }
chk "$WT/.scratch/bin/A/hven_sqp_corpus" "$CA"; chk "$WT/.scratch/bin/B/hven_sqp_corpus" "$CB"
chk "$WT/.scratch/bin/A/hven_bench_cell" "$LA"; chk "$WT/.scratch/bin/B/hven_bench_cell" "$LB"
mkdir -p "$D/suites" "$D/replay" "$D/train" "$D/mutation" "$D/install-smoke" "$D/runtime" "$D/nlp"
leg() { local name=$1; shift
  echo "LEG $name start $(date -Is)"
  "$@"; local rc=$?
  echo "LEG $name rc=$rc $(date -Is)"
  if [ $rc -ne 0 ]; then echo "STOP at $name"; echo "FOREGROUND_END $(date -Is)"; exit $rc; fi; }
suites_ok() { grep -q "100% tests passed, 0 tests failed out of $NTESTS" "$1" || { echo "P1_CRITERION_NOT_MET $1"; return 1; }; }
replay_ok() { # three B repetitions per mode: vs arm A's dev rows exactly the declared mover count, vs B's dev rows 0 differences
  local bad=0
  for K in 1 2 3; do for M in walk ssn ipm; do
    python3 "$WT/scripts/compare_replay.py" "$DEVR/A-k1-$M.csv" "$D/replay/B-k$K-$M.csv" "A-vs-B-k$K-$M" > "$D/replay/cmp-A-k$K-$M.txt" 2>&1; tail -1 "$D/replay/cmp-A-k$K-$M.txt"
    grep -q "differences: $(sed -n "s/^$M //p" "$D0/movers-replay.txt")\$" "$D/replay/cmp-A-k$K-$M.txt" || bad=1
    python3 "$WT/scripts/compare_replay.py" "$DEVR/B-k1-$M.csv" "$D/replay/B-k$K-$M.csv" "B-vs-Bdev-k$K-$M" > "$D/replay/cmp-Bdev-k$K-$M.txt" 2>&1; tail -1 "$D/replay/cmp-Bdev-k$K-$M.txt"
    grep -q "differences: 0$" "$D/replay/cmp-Bdev-k$K-$M.txt" || bad=1
  done; done
  [ $bad -eq 0 ] && echo REPLAY_AS_DECLARED
  return $bad
}
train_ok() { python3 "$L/train_check.py" "$D/train/B" "$WT/docs/notes/data/2026-11-m8-dn3" "$DEVA" "$DEVB" "$D0/movers-train.txt" > "$D/train/train_check.txt" 2>&1; local rc=$?
  grep -E '^(SUMMARY|MOVER|  FAIL|I4)' "$D/train/train_check.txt" | head -80
  grep -q "^MOVER WORSE" "$D/train/train_check.txt" && { echo "P3_WORSE_MOVER"; return 1; }
  python3 "$L/instr_compare.py" "$DEVA/cells" "$D/train/B/cells" > "$D/train/instr.txt" 2>&1; grep '^TOTAL' "$D/train/instr.txt"
  python3 - "$D/train/instr.txt" <<'PY' || rc=1
import re, sys
t = open(sys.argv[1]).read()
ipm = re.search(r"TOTAL hven-ipm\s+rows\s+(\d+)\s+A \S+\s+B \S+\s+B/A (\S+)", t)
sqp = re.search(r"TOTAL hven-sqp\s+rows\s+(\d+)\s+A \S+\s+B \S+\s+B/A (\S+)", t)
ok = bool(ipm and sqp and int(ipm.group(1)) == 238 and int(sqp.group(1)) == 238
          and 0.995 <= float(ipm.group(2)) <= 1.005 and float(sqp.group(2)) <= 1.005)
print("P3_INSTRUCTIONS " + ("ipm B/A %s in [0.995, 1.005], sqp B/A %s <= 1.005" % (ipm.group(2), sqp.group(2)) if ok else "CRITERION_NOT_MET"))
sys.exit(0 if ok else 1)
PY
  return $rc; }
nlp_ok() { python3 "$L/nlp_check.py" "$D/nlp" "$WT/.scratch/dev/nlp" "$D0/movers-nlp.txt" > "$D/nlp/nlp_check.txt" 2>&1; local rc=$?
  grep -E '^(rows|  FAIL|NLP)' "$D/nlp/nlp_check.txt"
  python3 "$L/instr_compare.py" "$D/nlp/A" "$D/nlp/B" STEENBRC STEENBRG YORKNET BIGBANK ACOPP118 DALLASM READING6 SMBANK ACOPP57 > "$D/nlp/instr.txt" 2>&1
  grep '^TOTAL' "$D/nlp/instr.txt"   # reported, no threshold (DECLARATION-PF-4.md section 4)
  return $rc; }
leg P4a bash "$D0/runtime/run_walk_r1.sh" "$CB" "$CA" "$D/runtime"
leg P4b bash "$L/nlp_instr.sh" "$D/nlp" "$WT/.scratch/bin/A/hven_bench_cell" "$LA" "$WT/.scratch/bin/B/hven_bench_cell" "$LB"
leg P4b-criterion nlp_ok
leg P5 bash "$L/mutations.sh" "$D/mutation"
leg P6 bash "$L/install_smoke.sh" "$D/install-smoke"
echo "ALL_LEGS_PASSED"
echo "FOREGROUND_END $(date -Is)"
