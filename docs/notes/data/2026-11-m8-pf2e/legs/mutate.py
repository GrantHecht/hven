#!/usr/bin/env python3
"""PF-2e: apply (or restore) one declared mutation (../DECLARATION-PF-2e.md section 4).
usage: [MUT_TREE=<dir>] mutate.py <id> | restore.   Each anchor must occur the stated number of times.
restore runs `git checkout --` on the mutated files: run it only on a committed (clean) tree, as mutations.sh checks."""
import os
import subprocess
import sys

WS = "src/qp/walk/ws_independence.cpp"
WH = "include/hven/detail/qp/walk/ws_independence.h"
WE = "src/qp/walk/walk_engine.cpp"
REACH = "        return reread_probe ? reach > kDepTol : reach >= kDepUndecidable;\n"

# id: (file, old, new, expected occurrences)
MUT = {
    # P1: the step-contraction threshold 0.5 -> 0.01 (the reach keeps 0.5): cuts contracting refinements
    "P1": (WS, "!(rho_cur >= kDepStagnation * rho_prev)", "!(rho_cur >= 0.01 * rho_prev)", 1),
    # P2: kDepStagnation 0.5 -> 2.0: a stagnant (q ~ 1) step is never cut
    "P2": (WH, "constexpr double kDepStagnation = 0.5;", "constexpr double kDepStagnation = 2.0;", 1),
    # P3: the re-read's reach kDepTol -> kDepUndecidable
    "P3": (WS, REACH, "        return reread_probe ? reach > kDepUndecidable : reach >= kDepUndecidable;\n", 1),
    # P4: the test's reach kDepUndecidable -> kDepTol
    "P4": (WS, REACH, "        return reread_probe ? reach > kDepTol : reach >= kDepTol;\n", 1),
    # P5: the zero-solve pre-check (the test's own last step as the probe) removed
    "P5": (WS, "    if (reread_probe && steps < kDepRefineSteps && stagnant_cut(kDepRefineSteps - steps)) {\n",
           "    if (false && reread_probe && steps < kDepRefineSteps && stagnant_cut(kDepRefineSteps - steps)) {\n", 1),
    # P6: the resume ignores the test's steps (final iterate kept, its step count forced 0)
    "P6": (WS, "        steps = first->resume_steps;\n", "        steps = 0;\n", 1),
    # P7: test_candidate's re-read without the probe
    "P7": (WE, "detail::grey_reread(qp, ws, view, a, opts, t);",
           "detail::certificate_coefficients(qp, ws, view, a, opts, &t);", 1),
    # P8: the test probe also on screen tests
    "P8": (WS, "probe == DepProbe::kTest && target > 0.0 && !t.screen &&", "probe == DepProbe::kTest && target > 0.0 &&", 1),
    # P9 (r1): the border exclusion removed (both probes)
    "P9": (WS, " && !sys.bordered;", ";", 2),
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
        print(f"MUTATION_DOES_NOT_APPLY {arg}: anchor occurs {n} times (want {want}) in {f}")
        return 3
    open(p, "w").write(s.replace(old, new))
    print(f"APPLIED {arg} to {f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
