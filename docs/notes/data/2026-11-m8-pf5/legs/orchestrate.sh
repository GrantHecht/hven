#!/usr/bin/env bash
# PF-5 (../DECLARATION-PF-5.md) section 5 (r1: GATE_DIR names the output directory, gate-r1 for ../DECLARATION-PF-5-r1.md): the legs in order, each compute leg in its own box-lane acquisition; the
# chain stops at the first failed leg. Outputs under ../gate/. Run outside any lock:
#   bash orchestrate.sh <code commit> <corpus A sha> <corpus B sha> <lane A sha> <lane B sha> <ntests> [first leg]
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf5b
L=$WT/docs/notes/data/2026-11-m8-pf5/legs
G=$WT/docs/notes/data/2026-11-m8-pf5/${GATE_DIR:-gate}
BOX=$HOME/.local/bin/box-lane
CODE=${1:?}; CA=${2:?}; CB=${3:?}; LA=${4:?}; LB=${5:?}; NT=${6:?}; FIRST=${7:-P1}
BIN=$WT/.scratch/bin
mkdir -p "$G"
cd "$WT" || exit 1
echo "ORCHESTRATE_START $(date -Is) head $(git rev-parse HEAD)"
# The code paths of the leg HEAD must equal the code commit.
git diff --quiet "$CODE" HEAD -- src include tests bench examples bindings CMakeLists.txt cmake scripts || { echo CODE_PATHS_DIFFER; exit 2; }
git diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_DIRTY; exit 2; }
for f in A/hven_sqp_corpus:$CA B/hven_sqp_corpus:$CB A/hven_bench_cell:$LA B/hven_bench_cell:$LB; do
  [[ "$(sha256sum "$BIN/${f%%:*}" | cut -d' ' -f1)" == "${f#*:}" ]] || { echo "BINARY_SHA_MISMATCH ${f%%:*}"; exit 4; }
done
run=0; [ "$FIRST" = P1 ] && run=1
leg() { [ "$1" = "$FIRST" ] && run=1; [ $run = 1 ]; }
fail() { echo "LEG_FAIL $1 $(date -Is)"; echo "ORCHESTRATE_END FAIL"; exit 1; }
if leg P1; then
  for t in rel:Release dbg:Debug; do
    $BOX -- bash -c "bash $L/build.sh $WT ${t#*:} ${t%%:*} && bash $L/ctest.sh ${t%%:*} $G/ctest-${t%%:*}.log" > "$G/P1-${t%%:*}.out" 2>&1
    grep -q "100% tests passed, 0 tests failed out of $NT\$" "$G/ctest-${t%%:*}.log" || fail "P1 ${t%%:*}"
  done
  echo "LEG_PASS P1 $(date -Is)"
fi
if leg P2; then
  $BOX -- bash -c "bash $L/replay.sh $G/replay A $BIN/A/hven_sqp_corpus 1 && bash $L/replay.sh $G/replay B $BIN/B/hven_sqp_corpus 3" > "$G/P2.out" 2>&1 || fail "P2 run"
  ok=1; : > "$G/replay-compare.txt"
  for k in 1 2 3; do for m in walk ssn ipm; do
    python3 scripts/compare_replay.py "$G/replay/A-k1-$m.csv" "$G/replay/B-k$k-$m.csv" "A-k1 vs B-k$k $m" >> "$G/replay-compare.txt" 2>&1 || ok=0
  done; done
  [ $ok = 1 ] || fail P2
  echo "LEG_PASS P2 $(date -Is)"
fi
if leg P3; then
  $BOX -- bash "$L/corun.sh" "$L/jobs-train.txt" "$BIN/A/hven_bench_cell" "$LA" "$BIN/B/hven_bench_cell" "$LB" > "$G/P3.out" 2>&1 || fail "P3 run"
  python3 "$L/pf5_check.py" train "$G/train/A" "$G/train/B" "$WT/docs/notes/data/2026-11-m8-dn3/roster.tsv" > "$G/train-check.txt" 2>&1 || fail P3
  echo "LEG_PASS P3 $(date -Is)"
fi
if leg P4; then
  $BOX -- bash "$L/corun.sh" "$L/jobs-nlp.txt" "$BIN/A/hven_bench_cell" "$LA" "$BIN/B/hven_bench_cell" "$LB" > "$G/P4.out" 2>&1 || fail "P4 run"
  python3 "$L/pf5_check.py" nlp "$G/nlp/A" "$G/nlp/B" STEENBRC STEENBRG YORKNET STEENBRB STEENBRE STEENBRD BIGBANK ACOPP118 ACOPR57 DALLASM READING6 SAWPATH ZAMB2-8 SMBANK ACOPP57 > "$G/nlp-check.txt" 2>&1 || fail P4
  echo "LEG_PASS P4 $(date -Is)"
fi
if leg P5; then
  python3 "$L/instr_compare.py" "$G/train/A/cells" "$G/train/B/cells" > "$G/instr-train.txt" 2>&1
  python3 "$L/instr_compare.py" "$G/nlp/A" "$G/nlp/B" ACOPP118 YORKNET > "$G/instr-nlp.txt" 2>&1
  python3 "$L/p5_criteria.py" "$G/instr-train.txt" "$G/nlp/A" "$G/nlp/B" > "$G/P5.txt" 2>&1 || fail P5
  echo "LEG_PASS P5 $(date -Is)"
fi
if leg P6; then
  $BOX -- bash "$L/mutations.sh" "$G/mutation" > "$G/P6.out" 2>&1 || fail P6
  echo "LEG_PASS P6 $(date -Is)"
fi
if leg P7; then
  $BOX -- bash "$L/install_smoke.sh" "$G/install-smoke" > "$G/P7.out" 2>&1 || fail P7
  echo "LEG_PASS P7 $(date -Is)"
fi
echo "ORCHESTRATE_END PASS $(date -Is)"
