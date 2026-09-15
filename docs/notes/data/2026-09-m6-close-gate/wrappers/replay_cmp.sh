#!/usr/bin/env bash
# THE CLOSE-GATE 27-CELL COMPARISON: this gate's HEAD arm against the BASE arm
# W5 T9 produced and retained (binary accbff0e60b0 -- the W4 close, code
# d4d78f9), engine by engine, plus the committed t10b IPM baseline as the
# CONTROL against BOTH arms.  NO GATE (brief section 0): byte-exact is the bar.
# Every status OR-FOLDED.
#
# THE BASE CSVs read here are the COMMITTED artifact copies under
# docs/notes/data/2026-09-m6-w5-acceptance/replay/, which were verified
# byte-identical (sha256) to the retained .scratch/w5t9/replay/ copies at the
# start of this gate.  They are READ ONLY and never rewritten.
set -uo pipefail
source /home/ghecht/Projects/hven/.scratch/m6-close/env.sh
D=$F/replay
BASE=${HVEN_CLOSE_BASE:-$R/docs/notes/data/2026-09-m6-w5-acceptance/replay}
CMP=${HVEN_CLOSE_CMP:-$R/scripts/compare_replay.py}
T10B=$R/bench/baselines/2026-09-02-t10b-ipm/ipm_baseline.csv
echo "COMPARATOR: $CMP"
echo "COMPARATOR sha256: $(sha256sum "$CMP" | cut -d' ' -f1)"
echo "BASE dir  : $BASE"
worst=0
run_cmp() { # A B label
  python3 -B "$CMP" "$1" "$2" "$3"; local rc=$?
  worst=$((worst | rc))
  echo "  CMP_RC=$rc  (worst so far: $worst)"
}
for eng in walk ssn ipm; do
  run_cmp "$BASE/base-$eng.csv" "$D/head-$eng.csv" \
          "W4 CLOSE BASE accbff0e-$eng vs M6 CLOSE GATE HEAD-$eng"
done
run_cmp "$T10B" "$D/head-ipm.csv" "committed t10b ipm baseline vs CLOSE HEAD-ipm (the CONTROL)"
run_cmp "$T10B" "$BASE/base-ipm.csv" "committed t10b ipm baseline vs BASE-ipm (the CONTROL, as W5 T9 ran it)"
echo "--- stamps and schema ---"
grep -hE '^# (binary|schema|budget_table_hash):' "$BASE/base-ipm.csv" "$D/head-ipm.csv"
echo "REPLAY_CMP_WORST_RC=$worst"
exit $worst
