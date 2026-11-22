#!/usr/bin/env bash
# RF-1 gate C legs C2 (TRAIN) and C3 (replay), co-run on two pinned physical cores of one box lane (counters only,
# MKL_NUM_THREADS=1; CLAUDE.md section 7). Run under: ~/.local/bin/box-lane -- bash c_orchestrate.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/rf1
D=$WT/docs/notes/data/2026-11-m8-rf1; L=$D/legs; BIN=$WT/.scratch/bin/C
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS"
read -r p1 p2 _ <<< "$BOX_LANE_PHYS"
want() { grep -F "$1" $D/BINARIES.txt | awk '{print $NF}'; }
[ "$(sha256sum $BIN/hven_sqp_corpus | cut -d' ' -f1)" = "$(want 'corpus C')" ] || { echo SHA_MISMATCH corpus; exit 4; }
PIN_CORE=$p1 RPIN_CORE=$p1 bash $L/train_enum.sh $D/gate-C/train/C $BIN/hven_bench_cell "$(want 'lane C')" > $D/gate-C/train/enum-C.out 2>&1 & tp=$!
taskset -c $p2 bash $L/replay.sh $D/gate-C/replay C $BIN/hven_sqp_corpus 1 > $D/gate-C/replay/run-C.out 2>&1 & rp=$!
wait $rp; echo "replay rc=$? $(date -Is)"; wait $tp; echo "train rc=$? $(date -Is)"
for M in walk ssn ipm; do
  python3 $WT/scripts/compare_replay.py $WT/docs/notes/data/2026-11-m8-pf2d/gate/replay/L-k1-$M.csv $D/gate-C/replay/C-k1-$M.csv "REF-vs-C-k1-$M" > $D/gate-C/replay/cmp-C-k1-$M.txt 2>&1
  echo "$M: $(tail -1 $D/gate-C/replay/cmp-C-k1-$M.txt)"
done
python3 $L/c_relabel_check.py $D/gate-C/train/C $WT/docs/notes/data/2026-11-m8-dn3 $WT/docs/notes/data/2026-11-m8-pf2d/gate/train/L > $D/gate-C/train/c2_check.txt 2>&1
echo "c2 rc=$?"; grep -E "^C2_" $D/gate-C/train/c2_check.txt
echo "FOREGROUND_END $(date -Is)"
