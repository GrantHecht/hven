#!/usr/bin/env bash
# THE DECLARED RE-DERIVATION (CLAUDE.md section 7). The U0 27-cell three-arm
# replay re-run at the SHIPPED defaults (qp_failure_budget = 3), compared BOTH
# ways: against the M6 close HEAD's CSVs -- the pin this task breaks on purpose
# -- and against this task's OWN heads taken at the OFF-default HEAD, which
# isolates exactly what the default literal moved and nothing else.
set -uo pipefail
R=/home/ghecht/Projects/hven
S=$R/.scratch/m7w2m1
BIN=$S/rel/bench/hven_sqp_corpus
D=$S/replay-shipped
mkdir -p "$D" "$S/logs"
CELLS="$(grep -v '^#' "$R/docs/notes/data/2026-08-m6-w1-acceptance/u0-replay-cells.txt" | tr -d '\n')"
for eng in walk ssn ipm; do
  MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 \
    "$BIN" --engine "$eng" --cells "$CELLS" --csv "$D/head-$eng.csv" \
    > "$S/logs/replay-shipped-$eng.out" 2>&1
  echo "ran head-$eng rc=$?"
done
{
  echo "=== THE DECLARED RE-DERIVATION: U0 27-cell three-arm replay at the SHIPPED defaults ==="
  echo "hven HEAD:        $(git -C "$R" rev-parse HEAD)"
  echo "shipped defaults: $(grep -m1 'Index qp_failure_budget' "$R/include/hven/drivers/sqp_solver_types.h" | sed 's/^ *//')  $(grep -m1 'bool qp_cap_retry' "$R/include/hven/drivers/sqp_solver_types.h" | sed 's/^ *//')"
  echo "binary:           $(sha256sum "$BIN" | cut -d' ' -f1)"
  echo
  echo "--- A. against the M6 CLOSE HEAD CSVs (the PIN this task breaks on purpose) ---"
  for eng in walk ssn ipm; do
    python3 -B "$R/scripts/compare_replay.py" \
      "$R/docs/notes/data/2026-09-m6-close-gate/replay/head-$eng.csv" "$D/head-$eng.csv" \
      "M6 CLOSE HEAD-$eng vs M7 W2 M1 SHIPPED HEAD-$eng"
    echo "  CMP_RC=$?"
  done
  echo
  echo "--- B. against THIS TASK's OWN heads at the OFF-default HEAD 4d27fb02 ---"
  echo "    (same code, same binary options, ONE literal different: the isolation)"
  for eng in walk ssn ipm; do
    python3 -B "$R/scripts/compare_replay.py" \
      "$R/docs/notes/data/2026-09-m7-w2-m1-qp-cap/replay/head-$eng.csv" "$D/head-$eng.csv" \
      "M1 OFF-DEFAULT HEAD-$eng vs M1 SHIPPED HEAD-$eng"
    echo "  CMP_RC=$?"
  done
} > "$D/comparison.txt" 2>&1
cat "$D/comparison.txt"
