#!/usr/bin/env bash
# FOLLOW-UP TO THE STOPPED CENSUS LEG (written AFTER the data, and said so): CLAUDE.md section 7 -- "a deviation observed under a co-run is a
# candidate, not a regression; re-run the deviating cell ALONE and reproduce it before calling it one". This is NOT a gate leg and changes no
# criterion: the census verdict (undeclared movers => FAIL) stands as the chain recorded it. This only characterizes the T1 (6-wide co-run)
# undeclared movers: each is re-run SOLO (taskset -c 0, MKL_NUM_THREADS=1, alone under the box lock), and "reproduced" means the solo row's 13
# asserted columns are byte-identical to the co-run row AND still differ from the baseline outside the declared coordinates.
# The T2 cells already ran solo in the census; the one T3 undeclared mover (f7_n5000_path_corrupted, wall-dependent, 3600 s phases) is NOT re-run.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; source "$HERE/../wrappers-close/env.sh"
C="$F/census"; D="$C/confirm"; mkdir -p "$D"; BIN="$F/build-release/bench/hven_sqp_corpus"
T23="f7_n10000_path_physics f7_n5000_path_neutral f7_n5000_path_warm f7_n10000_path_activity f7_n10000_path_corrupted f7_n10000_path_neutral f7_n10000_path_warm f7_n20000_path_activity f7_n20000_path_corrupted f7_n20000_path_neutral f7_n20000_path_physics f7_n20000_path_warm f7_n5000_path_corrupted"
# candidates = gated MISMATCH cells, minus the declared movers, minus the T2/T3 cells
python3 -B - "$C/compare-gated.txt" "$REC/derivation/expected-census-movers.txt" "$T23" > "$D/candidates.txt" <<'PY'
import re, sys
mism = re.findall(r'^MISMATCH (\S+)$', open(sys.argv[1]).read(), re.M)
decl = {l.split()[0] for l in open(sys.argv[2]) if l.strip()}
t23 = set(sys.argv[3].split())
for c in mism:
    if c not in decl and c not in t23: print(c)
PY
echo "candidates: $(wc -l < "$D/candidates.txt")"; worst=0
while read -r cell; do
  taskset -c 0 env MKL_NUM_THREADS=1 "$BIN" --engine walk --cells "$cell" --csv "$D/$cell.csv" > "$D/$cell.out" 2>&1; rc=$?
  co="$(grep -h "^$cell," "$C/rows/$cell.csv" | cut -d, -f1-13)"; so="$(grep -h "^$cell," "$D/$cell.csv" | cut -d, -f1-13)"
  if [ $rc -ne 0 ]; then echo "SOLO $cell rc=$rc (NOT a result)"; worst=1; continue; fi
  if [ "$co" = "$so" ]; then v=REPRODUCED_BYTE_IDENTICAL_TO_THE_CORUN_ROW; else v=DIFFERS_FROM_THE_CORUN_ROW; fi
  echo "SOLO $cell $v"; [ "$v" = DIFFERS_FROM_THE_CORUN_ROW ] && { echo "  corun: $co"; echo "  solo : $so"; }
done < "$D/candidates.txt"
echo "CONFIRM_WORST_RC=$worst"; exit $worst
