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
    # B2c: the cumulative subtraction across entries dropped (each entry gets the whole budget)
    "B2c": (S, "            std::max<Index>(0, total_budget - st.out.counters.restoration_factorizations);\n",
               "            std::max<Index>(0, total_budget);\n", 1),
    # B2d: the derivation base read as the solve's total (restoration included), not the main loop's own
    "B2d": (R, "    const Index main_factorizations = factorizations - restoration_factorizations;\n",
               "    const Index main_factorizations = factorizations;\n", 1),
    # B2r: the reduction resume no longer reads a budget cut
    "B2r": (S, "         (rs.status == SolveStatus::kWorkLimit && (phase_stalled || phase_budget_cut)) ||\n",
               "         (rs.status == SolveStatus::kWorkLimit && phase_stalled) ||\n", 1),
    # B2z: a later entry with no budget left is handed no ceiling (zero read as none)
    "B2z": (S, "        phase_opts.common.max_factorizations = budget_left;\n",
               "        if (budget_left > 0) {\n            phase_opts.common.max_factorizations = budget_left;\n        }\n", 1),
    # B2s (C577 M6): a stall-window stop counted as a B2 cut (the label is unchanged; only restoration_budget_cuts moves)
    "B2s": (S, "            phase_result.status == SolveStatus::kWorkLimit && !phase_stalled && budget_binds;\n",
               "            phase_result.status == SolveStatus::kWorkLimit && budget_binds;\n", 1),
    # B2x (C577 should; ARGUED): the call site passes 0 for the restoration share, so the derived base is the solve's total
    "B2x": (S, "        st.out.counters.restoration_factorizations);\n    bool phase_budget_cut = false;\n",
               "        0);\n    bool phase_budget_cut = false;\n", 1),
    # FR1: a spent budget forbids the feasible resume (B2 gating a feasible entry; the declared behaviour is that it does not)
    "FR1": (S, "    const bool feasible_resume = kkt_r.feasibility <= opts_.common.feasibility_tol;\n",
               "    const bool feasible_resume = kkt_r.feasibility <= opts_.common.feasibility_tol &&\n"
               "                                 st.out.counters.restoration_factorizations < total_budget;\n", 1),
    # CAP1: the entry cap no longer outranks the no-progress label
    "CAP1": (S, "        const bool no_progress = allow_restoration_ && below_entry_cap;\n",
                "        const bool no_progress = allow_restoration_;\n", 1),
    # RR1: record_resume keeps the previous interval's solved flag
    "RR1": (R, "        resumed_ = true;\n        solved_since_resume_ = false;\n",
               "        resumed_ = true;\n", 1),
    # LAB1: kRestorationNoProgress dropped from the labelable exits (the optimal+acceptable count)
    "LAB1": (S, "                                               reason == StopReason::kRestorationWorkLimit ||\n"
                "                                               reason == StopReason::kRestorationNoProgress);\n",
                "                                               reason == StopReason::kRestorationWorkLimit);\n", 1),
    # CN1..CN6: each RST-B counter increment dropped; CN5b: the entry rule's counter also counts the cap's refusal
    "CN1": (S, "            ++st.out.counters.restoration_unproductive_entries;\n", "", 1),
    "CN2": (S, "            ++st.out.counters.restoration_no_progress_requests;\n", "", 1),
    "CN3": (S, "        st.out.counters.restoration_factorizations += phase.counters.factorizations;\n", "", 1),
    "CN4": (S, "            ++st.out.counters.restoration_budget_cuts;\n", "", 1),
    "CN5": (S, "            ++st.out.counters.restoration_no_accept_refusals;\n", "", 1),
    "CN5b": (S, "        if (no_progress && !accepted_since_resume) {\n",
                "        if (!accepted_since_resume) {\n", 1),
    "CN6": (S, "            st.out.counters.restoration_refusal_feasibility = mj.kkt.feasibility;\n", "", 1),
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
