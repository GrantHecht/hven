#!/usr/bin/env bash
# PF-4 r2 dev session 1 (not a leg): pf4-r2 at code head $1 on origin/m8 331b31837. Build rel, ctest rel; arm A corpus
# from git archive 331b31837; lanes A (331b31837) and B ($1); dev replay A, B; dev TRAIN A, B (stat); dev nlp35;
# mutation pre-flight (all, R1/R2 included); Debug build + ctest.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf4-r2
L=$WT/docs/notes/data/2026-11-m8-pf4/r2/legs
B_SHA=${1:?code sha}; A_SHA=331b3183747b5c1849651706f648de2c337ddf75
D=$WT/.scratch/dev; mkdir -p $D
echo "S1_START $(date -Is)"
bash $L/build.sh "$WT" Release rel; echo "BUILD_REL rc=$?"
bash $L/ctest.sh rel "$D/ctest-rel.log"; echo "CTEST_REL rc=$?"; grep -E "tests passed|Failed|\*\*\*" $D/ctest-rel.log | head
if [ ! -d "$WT/.scratch/armA-src" ]; then
  mkdir -p "$WT/.scratch/armA-src" && git -C "$WT" archive $A_SHA | tar -x -C "$WT/.scratch/armA-src"
  for m in eigen fmt; do rmdir "$WT/.scratch/armA-src/dep/$m" 2>/dev/null; cp -r "$WT/dep/$m" "$WT/.scratch/armA-src/dep/$m"; done
fi
bash $L/build.sh "$WT/.scratch/armA-src" Release armA hven_sqp_corpus; echo "BUILD_A rc=$?"
NEVER=1 bash $L/lane_build.sh A $A_SHA; echo "LANE_A rc=$?"
NEVER=1 bash $L/lane_build.sh B "$B_SHA"; echo "LANE_B rc=$?"
cp -p $WT/.scratch/build-pf4/armA/bench/hven_sqp_corpus $WT/.scratch/bin/A/hven_sqp_corpus
cp -p $WT/.scratch/build-pf4/rel/bench/hven_sqp_corpus $WT/.scratch/bin/B/hven_sqp_corpus
sha256sum $WT/.scratch/bin/{A,B}/hven_*
bash $L/replay.sh $D/replay A $WT/.scratch/bin/A/hven_sqp_corpus 1; echo "REPLAY_A rc=$?"
bash $L/replay.sh $D/replay B $WT/.scratch/bin/B/hven_sqp_corpus 1; echo "REPLAY_B rc=$?"
for M in walk ssn ipm; do python3 $WT/scripts/compare_replay.py $D/replay/A-k1-$M.csv $D/replay/B-k1-$M.csv dev-$M | tail -4; done
SA=$(sha256sum $WT/.scratch/bin/A/hven_bench_cell | cut -d' ' -f1); SB=$(sha256sum $WT/.scratch/bin/B/hven_bench_cell | cut -d' ' -f1)
bash $L/train_enum.sh $D/train-A $WT/.scratch/bin/A/hven_bench_cell $SA; echo "TRAIN_A rc=$?"
bash $L/train_enum.sh $D/train-B $WT/.scratch/bin/B/hven_bench_cell $SB; echo "TRAIN_B rc=$?"
python3 $L/train_check.py $D/train-B $WT/docs/notes/data/2026-11-m8-dn3 $D/train-A > $D/train_check.txt 2>&1; grep -E "^SUMMARY|^MOVER" $D/train_check.txt | head -80
python3 $L/instr_compare.py $D/train-A/cells $D/train-B/cells > $D/train_instr.txt 2>&1; head -12 $D/train_instr.txt
bash $L/nlp_instr.sh $D/nlp $WT/.scratch/bin/A/hven_bench_cell $SA $WT/.scratch/bin/B/hven_bench_cell $SB; echo "NLP rc=$?"
python3 $L/nlp_check.py $D/nlp > $D/nlp_check.txt; grep -E "^MOVER|rows|NLP" $D/nlp_check.txt
python3 $L/instr_compare.py $D/nlp/A $D/nlp/B STEENBRC STEENBRG YORKNET BIGBANK ACOPP118 DALLASM SMBANK ACOPP57 READING6 > $D/nlp_instr.txt; head -40 $D/nlp_instr.txt
bash $L/mutations.sh $D/mut B1 B2 B3 B4 B5 B6 B7 T2 T3 T4 T5 T6 R1 R2 --; echo "MUT rc=$?"
bash $L/build.sh "$WT" Debug dbg; echo "BUILD_DBG rc=$?"
bash $L/ctest.sh dbg "$D/ctest-dbg.log"; echo "CTEST_DBG rc=$?"; grep -E "tests passed" $D/ctest-dbg.log
echo "S1_END $(date -Is)"
