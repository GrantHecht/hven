#!/usr/bin/env python3
"""Apply (or restore) one declared mutation to src/qp/qp_engine.cpp. usage: mutate.py <id> | restore.
M1 no-trigger:  the row disjunct of polish_footprint_open never opens the entry test.
M2 hold-off:    the terminal site's gray-zone hold never fires (stationary pass certifies at the classifier's 10x).
M3 no-floor:    a row-triggered entry does not floor the acceptance test's stationarity comparison.
M4 no-count:    the optimal_row_gray_exits counter is never incremented."""
import subprocess
import sys

P = "src/qp/qp_engine.cpp"
MUT = {
    "M1": ("    row_triggered =\n        !(worst_row_ratio(qp, ws, x, ai_row_norm1, lambda_i, ae_row_norm1, lambda_e, opts) <= 1.0);\n",
           "    row_triggered = false;\n"),
    "M2": ("            const bool gray_hold = !dropped_one && !stagnation_only",
           "            const bool gray_hold = false && !dropped_one && !stagnation_only"),
    "M3": ("        row_triggered\n            ? std::max(before.stationarity,",
           "        false\n            ? std::max(before.stationarity,"),
    "M4": ("                                ++counters.optimal_row_gray_exits;",
           "                                (void)counters;"),
}
arg = sys.argv[1]
if arg == "restore":
    subprocess.check_call(["git", "checkout", "--", P])
    print("RESTORED")
    sys.exit(0)
old, new = MUT[arg]
s = open(P).read()
if s.count(old) != 1:
    sys.exit(f"MUTATION_{arg}_DOES_NOT_APPLY (count {s.count(old)})")
open(P, "w").write(s.replace(old, new))
print(f"APPLIED {arg}")
