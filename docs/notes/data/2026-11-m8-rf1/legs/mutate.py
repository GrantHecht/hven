#!/usr/bin/env python3
"""RF-1 gate C: apply (or restore) one declared mutation. usage: [MUT_TREE=<dir>] mutate.py <id> | restore.
Ids are declared in ../DECLARATION-RF-1-C.md. Each anchor must occur exactly the stated number of times.
(pf2d's legs/mutate.py, its table replaced.)"""
import os
import subprocess
import sys

S = "src/sqp/sqp_solver.cpp"
O = "src/sqp/sqp_options.cpp"

# id: (file, old, new, expected occurrences)
MUT = {
    "C1": (S, "    if (!is_restoration_sub_driver_ &&\n        detail::sqp_exit_is_labelable(out.status, last_stop_reason_) &&",
              "    if (detail::sqp_exit_is_labelable(out.status, last_stop_reason_) &&", 1),  # label in the sub-driver too
    "C2": (S, "kkt.stationarity <= t.acceptable_stationarity_tol", "kkt.stationarity < t.acceptable_stationarity_tol", 1),
    "C3": (S, "kkt.complementarity <= t.acceptable_complementarity_tol &&", "true &&", 1),
    "C4": (S, "feasibility_e <= t.acceptable_equality_tol &&", "true &&", 1),
    "C5": (S, "feasibility_i <= t.acceptable_inequality_tol;", "true;", 1),
    "C6": (S, "return kkt.finite && kkt.stationarity", "return kkt.stationarity", 1),
    "C7": (S, "    return status == SolveStatus::kStalled && reason == StopReason::kSubproblemIterationLimit;",
              "    return (status == SolveStatus::kStalled && reason == StopReason::kSubproblemIterationLimit) ||\n           status == SolveStatus::kIterationLimit;", 1),
    "C8": (S, "                                        mj.qs.status == QpStatus::kIterationLimit;",
              "                                        false && mj.qs.status == QpStatus::kIterationLimit;", 1),
    "C9": (S, "        publish_stop(StopReason::kNone, std::move(origin));", "        publish_stop(last_stop_reason_, std::move(origin));", 1),
    "C10": (S, "        publish_stop(StopReason::kNone, std::move(origin));", "        publish_stop(StopReason::kNone, std::string());", 1),
    "C11": (S, "        out.status = SolveStatus::kAcceptable;\n        out.best_iterate.reset();\n", "        out.status = SolveStatus::kAcceptable;\n", 1),
    "C12": (O, "        if (!(floor <= v)) {", "        if (false) {", 1),
    # REPORTED, not must-kill (declaration section 4): the minor-cap split's top-level guard.
    "R1": (S, "            const bool minor_cap_exit = !is_restoration_sub_driver_ &&\n", "            const bool minor_cap_exit =\n", 1),
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
