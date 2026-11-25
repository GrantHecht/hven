#!/usr/bin/env python3
"""RST-B: apply (or restore) one declared mutation. usage: [MUT_TREE=<dir>] mutate_rstb.py <id> | restore | list.
Ids are declared in ../DECLARATION-RST-B.md section 4. Each anchor must occur exactly the stated number of times
(RF-1c's mutate_rf1c_r3.py, its table replaced)."""
import os
import subprocess
import sys

S = "src/sqp/sqp_solver.cpp"
R = "include/hven/detail/sqp/globalization/restoration.h"

# id: (file, old, new, expected occurrences)
MUT = {
    # B1a: a failure step counted as progress
    "B1a": (S, "            st.resto.progress.note_accept(!mj.failure_step);\n",
               "            st.resto.progress.note_accept(true);\n", 1),
    # B1b: a productive interval does not clear the run
    "B1b": (R, "        if (v.productive) {\n            unproductive_run_ = 0;\n        } else {\n",
               "        if (v.productive) {\n        } else {\n", 1),
    # B1c: off by one (the tolerance read as exclusive)
    "B1c": (R, "        v.admissible = unproductive_run_ <= max_unproductive;\n",
               "        v.admissible = unproductive_run_ < max_unproductive;\n", 1),
    # B1d: the first entry tested
    "B1d": (R, "        if (!resumed_) {\n            return v;\n        }\n        v.tested = true;\n",
               "        v.tested = true;\n", 1),
    # B1e: the refusal spelled numerical_error / restoration_refused (and s = entry_rule)
    "B1e": (S, "        const bool no_progress = allow_restoration_ && below_entry_cap;\n",
               "        const bool no_progress = false;\n", 1),
    # B1f: the refusal spelled kInfeasible
    "B1f": (S, "        st.resto.status = no_progress ? SolveStatus::kStalled : SolveStatus::kNumericalError;\n",
               "        st.resto.status = no_progress ? SolveStatus::kInfeasible : SolveStatus::kNumericalError;\n", 1),
    # B1g: the refusal sets the certificate
    "B1g": (S, "            no_progress ? StopReason::kRestorationNoProgress : StopReason::kRestorationRefused;\n",
               "            no_progress ? StopReason::kRestorationNoProgress : StopReason::kRestorationRefused;\n"
               "        st.out.infeasibility_certified = st.out.infeasibility_certified || no_progress;\n", 1),
    # B1h: an accept from an optimal elastic rung not counted as progress
    "B1h": (S, "            st.resto.progress.note_accept(!mj.failure_step);\n",
               "            st.resto.progress.note_accept(!mj.failure_step && !mj.row.elastic_applied);\n", 1),
    # RB1: a rolled-back accept keeps its progress credit
    "RB1": (S, "        st.resto.progress.retract_last_accept();\n",
               "", 1),
    # RB2: a rolled-back accept still counts for the entry rule (the kept-accept count not decremented)
    "RB2": (R, "        --kept_accepts_;\n", "", 1),
    # F1: the R4 trigger stops reading zero_motion_exhausted
    "F1": (S, "                                      q.counters.zero_motion_cap_hits > 0 ||\n"
              "                                      q.counters.zero_motion_exhausted > 0;\n",
              "                                      q.counters.zero_motion_cap_hits > 0;\n", 1),
    # F2: the regularization-first order ignored
    "F2": (S, "    if (in.regularization_first && r4) {\n",
              "    if (false && r4) {\n", 1),
    # F3: the floor label without the floor
    "F3": (S, "                    const bool at_floor = shrink_hits_floor(st.delta);\n",
              "                    const bool at_floor = true;\n", 1),
    # B2a: a cumulative-budget cut spelled as the caller's kWorkLimit
    "B2a": (S, "        if (phase_stalled || phase_budget_cut) {\n",
               "        if (phase_stalled) {\n", 1),
    # B2b: the caller's ceiling spelled kRestorationWorkLimit
    "B2b": (S, "            budget_binds = budget_left < caller_left;\n",
               "            budget_binds = true;\n", 1),
}

ROOT = os.environ.get("MUT_TREE") or subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()


def main():
    arg = sys.argv[1]
    if arg == "list":
        print(" ".join(MUT))
        return 0
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
