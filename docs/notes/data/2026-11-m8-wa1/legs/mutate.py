#!/usr/bin/env python3
"""WA-1: apply (or restore) one declared mutation of the code commit. usage: mutate.py <id> | restore.
M1 branch-off:   the footprint branch never entered (the pre-WA-1 guard).                       MUST-KILL
M2 no-merit-rise: the decision ignores whether the merit's slope rises.                          MUST-KILL
M3 no-merit-cap: the merit's minimiser along -p is never finite (always ride).                    MUST-KILL
M4 no-blocker:   the blocker never wins over the merit's minimiser / row budget (move through it). MUST-KILL
M8 no-row-budget: the row budget never stops the step (r2).                                     MUST-KILL
M5 move-sign:    the move goes along +p (x += a p).                                               MUST-KILL
M6 apply-sign:   the walk applies the move with the wrong sign (x -= move p).                     RECORDED (move path: no fixture reaches it)
M7 no-pins:      the pins half of the footprint branch's entry test removed.                      RECORDED (equivalent: bounds are eliminated)"""
import subprocess
import sys

E = "src/qp/walk/walk_engine.cpp"
H = "include/hven/detail/qp/walk/walk_engine.h"


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
if arg == "M1":
    sub1(E, "} else if (eff_opts.dual_regularization > 0.0 && ride_holds_pins(ws, p)) {",
         "} else if (false && eff_opts.dual_regularization > 0.0 && ride_holds_pins(ws, p)) {")
elif arg == "M2":
    sub1(H, "    if (!(merit_slope > slope_tol)) {\n        return d;\n    }", "    if (false) {\n        return d;\n    }")
elif arg == "M3":
    sub1(H, "merit_curv > 0.0 ? merit_slope / merit_curv : std::numeric_limits<double>::infinity();",
         "std::numeric_limits<double>::infinity();")
elif arg == "M4":
    sub1(H, "if ((blocked && a_block <= a_stop) || !std::isfinite(a_stop)) {", "if (!std::isfinite(a_stop)) {")
elif arg == "M8":
    sub1(H, "    const double a_stop = std::min(a_merit, a_rows);", "    const double a_stop = a_merit;")
elif arg == "M5":
    sub1(H, "        d.move = -a_stop;", "        d.move = a_stop;")   # r4: the anchor follows r2's code (was -a_merit)
elif arg == "M6":
    sub1(E, "                x += footprint_move * p;", "                x -= footprint_move * p;")
elif arg == "M7":
    sub1(E, "} else if (eff_opts.dual_regularization > 0.0 && ride_holds_pins(ws, p)) {",
         "} else if (eff_opts.dual_regularization > 0.0) {")
else:
    sys.exit("UNKNOWN")
print(f"APPLIED {arg}")
