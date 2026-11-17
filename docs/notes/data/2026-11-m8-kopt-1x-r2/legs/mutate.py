#!/usr/bin/env python3
"""Apply (or restore) one declared mutation. usage: mutate.py <id> | restore.
Ma no-row-contract:  the polish acceptance does not read the candidate's row ratio (src/qp/qp_engine.cpp).
Mb repair-spends:    a successful rank-repair `continue` spends the gray hold (the base's bookkeeping).
Mc no-return:        the certifying site never returns to the loop (the gray exit is counted and certified).
Md ten-x-target:     the gray-zone refinement targets the classifier's 10x even when asked for 1x.
Me1 no-bound:        the return bound removed (returns until the minor cap).
Me2 double-bound:    kGrayReturnBudget doubled (include/hven/detail/qp/qp_engine.h)."""
import re
import subprocess
import sys

E = "src/qp/qp_engine.cpp"
H = "include/hven/detail/qp/qp_engine.h"


def sub1(path, old, new):
    s = open(path).read()
    if s.count(old) != 1:
        sys.exit(f"MUTATION_DOES_NOT_APPLY {path} (count {s.count(old)}): {old[:60]!r}")
    open(path, "w").write(s.replace(old, new))


arg = sys.argv[1]
if arg == "restore":
    subprocess.check_call(["git", "checkout", "--", E, H])
    print("RESTORED")
    sys.exit(0)
if arg == "Ma":
    sub1(E, "    const bool row_contract =\n        cand_ratio <= std::max(1.0, inc_ratio) && (!require_clean || cand_ratio <= 1.0);\n",
         "    const bool row_contract = true;\n")
elif arg == "Mb":
    sub1(E, "gray_hold_issued);\n#endif\n                    continue;",
         "gray_hold_issued);\n#endif\n                    gray_hold_next = gray_hold_issued;\n                    continue;")
elif arg == "Mc":
    sub1(E, "if (gray_returns_left > 0 && iter + 1 < eff_max_iter) {", "if (false) {")
elif arg == "Md":
    sub1(E, "    if (margin <= 1.0) {", "    if (false) {")
elif arg == "Me1":
    sub1(E, "if (gray_returns_left > 0 && iter + 1 < eff_max_iter) {", "if (iter + 1 < eff_max_iter) {")
elif arg == "Me2":
    sub1(H, "constexpr Index kGrayReturnBudget = 16;", "constexpr Index kGrayReturnBudget = 32;")
else:
    sys.exit("UNKNOWN")
print(f"APPLIED {arg}")
