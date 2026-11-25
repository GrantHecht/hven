#!/usr/bin/env python3
"""RF-1c (a fresh table: M1 and M2 are this round's must-kills, R1 is reported): apply (or restore) one declared mutation. usage: [MUT_TREE=<dir>] mutate_rf1c_r2.py <id> | restore.
Ids are declared in ../DECLARATION-RF-1c.md section 3 (earlier entries: ../DECLARATION-RF-1-AB2-r4.md section 3). Each anchor must occur exactly the stated number of times.
(gate C's mutate.py, its table replaced.)"""
import os
import subprocess
import sys

S = "src/sqp/sqp_solver.cpp"
R = "include/hven/detail/sqp/globalization/restoration.h"

# id: (file, old, new, expected occurrences)
MUT = {
    # M1: the pass floor dropped (the constant 0 restores the pre-RF-1c window exactly: pop while size >= 2, test size > 0)
    "M1": (R, "inline constexpr std::size_t kRestorationStallMinPasses = 5;",
              "inline constexpr std::size_t kRestorationStallMinPasses = 0;", 1),
    # M2: the fresh window per regularized restart dropped
    "M2": (S, "            phase_stall.new_run(); // a restart starts a fresh window; the best stays (RF-1c)\n",
              "            (void)phase_stall;\n", 1),
    # REPORTED, not must-kill: new_run also forgets the running best
    "R1": (R, "    void new_run() { samples_.clear(); }",
              "    void new_run() { samples_.clear(); best_ = std::numeric_limits<double>::infinity(); }", 1),
}

ROOT = os.environ.get("MUT_TREE") or subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()


def main():
    arg = sys.argv[1]
    if arg == "restore":
        files = sorted({v[0] for v in MUT.values()})
        subprocess.check_call(["git", "-C", ROOT, "checkout", "--"] + files)
        print("RESTORED")
        return 0
    f, old, new, want = MUT[arg]
    p = f"{ROOT}/{f}"
    s = open(p).read()
    n = s.count(old)
    if n != want:
        print(f"ANCHOR {arg}: {n} occurrences, want {want}")
        return 1
    open(p, "w").write(s.replace(old, new))
    print(f"APPLIED {arg}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
