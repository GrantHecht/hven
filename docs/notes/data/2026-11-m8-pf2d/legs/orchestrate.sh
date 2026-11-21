#!/usr/bin/env bash
# PF-2d gate chain (../DECLARATION-PF-2d.md section 3 and -r1): P2 replay x3 (A, L, F, G co-run on four pinned physical cores),
# P3 TRAIN (A, L, F, G co-run) + checks. (P1 ctest, P4 mutations, P5 install smoke, P6 measure have their own runs.)
# Run under: ~/.local/bin/box-lane -- bash orchestrate.sh
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/pf2d
D0=$WT/docs/notes/data/2026-11-m8-pf2d; D=$D0/gate; L=$D0/legs; BIN=$WT/.scratch/bin
echo "FOREGROUND_START $(date -Is) lane=$BOX_LANE phys=$BOX_LANE_PHYS"
chk() { [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$(grep -F "$2" $D0/BINARIES.txt | awk '{print $NF}')" ] || { echo "SHA_MISMATCH $1"; exit 4; }; }
for t in A L F G; do chk $BIN/$t/hven_sqp_corpus "corpus $t"; chk $BIN/$t/hven_bench_cell "lane $t"; done
mkdir -p $D/replay $D/train/A $D/train/L $D/train/F $D/train/G
read -r p1 p2 p3 p4 _ <<< "$BOX_LANE_PHYS"
PINS=($p1 $p2 $p3 $p4); ARMS=(A L F G)
leg() { local name=$1; shift; echo "LEG $name start $(date -Is)"; "$@"; local rc=$?; echo "LEG $name rc=$rc $(date -Is)"
  if [ $rc -ne 0 ]; then echo "STOP at $name"; exit $rc; fi; }
replay_run() { local pids=() rc=0 i
  for i in 0 1 2 3; do taskset -c ${PINS[$i]} bash $L/replay.sh $D/replay ${ARMS[$i]} $BIN/${ARMS[$i]}/hven_sqp_corpus 3 > $D/replay/run-${ARMS[$i]}.out 2>&1 & pids+=($!); done
  for p in "${pids[@]}"; do wait $p || rc=1; done; echo "replay rc=$rc"; return $rc; }
replay_ok() { local bad=0 st=0
  for X in L F G; do for K in 1 2 3; do for M in walk ssn ipm; do
    python3 $WT/scripts/compare_replay.py $D/replay/A-k$K-$M.csv $D/replay/$X-k$K-$M.csv "A-vs-$X-k$K-$M" > $D/replay/cmp-$X-k$K-$M.txt 2>&1; echo "$X k$K $M: $(tail -1 $D/replay/cmp-$X-k$K-$M.txt)"
    if [ $X = L ]; then grep -q "differences: 0$" $D/replay/cmp-$X-k$K-$M.txt || bad=1; fi
  done; done; done
  [ $bad -eq 0 ] && echo REPLAY_L_AS_DECLARED; return $bad; }
train_run() { local pids=() rc=0 i
  for i in 0 1 2 3; do t=${ARMS[$i]}
    PIN_CORE=${PINS[$i]} RPIN_CORE=${PINS[$i]} bash $L/train_enum.sh $D/train/$t $BIN/$t/hven_bench_cell $(grep -F "lane $t" $D0/BINARIES.txt | awk '{print $NF}') > $D/train/enum-$t.out 2>&1 & pids+=($!); done
  for p in "${pids[@]}"; do wait $p || rc=1; done; echo "enum rc=$rc"; return $rc; }
train_ok() { export PF2D_IGNORE_TALLY=1; local bad=0
  for X in L F G; do
    python3 $L/train_check.py $D/train/$X $WT/docs/notes/data/2026-11-m8-dn3 $D/train/A > $D/train/train_check-$X.txt 2>&1
    python3 $L/train_movers.py $D/train/$X $WT/docs/notes/data/2026-11-m8-dn3 $D/train/A > $D/train/movers-$X.txt 2>&1
    echo "== $X vs A"; grep -E '^(SUMMARY|MOVERS_SUMMARY)' $D/train/train_check-$X.txt $D/train/movers-$X.txt
    python3 $L/instr_compare.py $D/train/A/cells $D/train/$X/cells > $D/train/instr-$X.txt 2>&1; grep '^TOTAL' $D/train/instr-$X.txt
  done
  grep -q "^SUMMARY rows 476; movers vs arm A 0 .*missing/capped 0" $D/train/train_check-L.txt || { echo P3_L_ROWS_DIFFER_OR_MISSING; bad=1; }
  # F and G are evidence arms (declaration r2): their movers are reported above, not asserted.
  python3 - $D/train/instr-L.txt <<'PY'
import re, sys
t = open(sys.argv[1]).read()
m = re.search(r"TOTAL hven-sqp\s+rows\s+(\d+)\s+A \S+\s+L?\s*B \S+\s+B/A (\S+)", t)
ok = bool(m and int(m.group(1)) == 238 and float(m.group(2)) <= 1.000)
print("P3_INSTRUCTIONS_L " + ("L/A %s <= 1.000" % m.group(2) if ok else "CRITERION_NOT_MET (see instr-L.txt)"))
sys.exit(0 if ok else 1)
PY
  [ $? -eq 0 ] || bad=1
  return $bad; }
leg P2 replay_run
leg P2-criterion replay_ok
leg P3 train_run
leg P3-criterion train_ok
echo ALL_LEGS_PASSED
echo "FOREGROUND_END $(date -Is)"
