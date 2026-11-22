#!/usr/bin/env python3
"""RF-1 gate A+B2 (-r4: mutate_ab2_r3.py with M21 = F-2 added; every r3 entry unchanged): apply (or restore) one declared mutation. usage: [MUT_TREE=<dir>] mutate_ab2.py <id> | restore.
Ids are declared in ../DECLARATION-RF-1-AB2-r4.md section 4 (r3 entries: ../DECLARATION-RF-1-AB2-r3.md section 4). Each anchor must occur exactly the stated number of times.
(gate C's mutate.py, its table replaced.)"""
import os
import subprocess
import sys

S = "src/sqp/sqp_solver.cpp"
R = "include/hven/detail/sqp/globalization/restoration.h"

# id: (file, old, new, expected occurrences)
MUT = {
    # 3.1 rung skip rules inverted
    "M1": (S, "if (unused(RetryRung::kShrink) && (in.window_active || in.step_inf > in.shrunk_radius)) {",
              "if (unused(RetryRung::kShrink) && !(in.window_active || in.step_inf > in.shrunk_radius)) {", 1),
    "M2": (S, "if (unused(RetryRung::kCold) && in.warm_nonempty) {",
              "if (unused(RetryRung::kCold) && !in.warm_nonempty) {", 1),
    "M3": (S, "if (unused(RetryRung::kRegularization) && in.dependent_or_zero_motion &&",
              "if (unused(RetryRung::kRegularization) && !in.dependent_or_zero_motion &&", 1),
    # 3.1 replay guard disabled
    "M4": (S, "const bool replay = st.retry_fp_valid && fp == st.retry_fp;", "const bool replay = false;", 1),
    # 3.2 projection on every termination kind / never
    "M5": (S, "    if (mj.walk_owns_this_qp && mj.qs.counters.stagnation_stops > 0 &&\n",
              "    if (mj.walk_owns_this_qp &&\n", 1),
    "M6": (S, "    if (mj.walk_owns_this_qp && mj.qs.counters.stagnation_stops > 0 &&\n",
              "    if (false && mj.walk_owns_this_qp && mj.qs.counters.stagnation_stops > 0 &&\n", 1),
    # 3.3 row refusal routing reverted to the bound rule
    "M7": (S, "        (any_h_row_refusal ||\n", "        (false ||\n", 1),
    # 3.4 floor exit conjuncts dropped
    "M8": (S, "    return feasibility <= feasibility_tol && pred <= kNoiseFloorFactor *",
              "    return pred <= kNoiseFloorFactor *", 1),
    "M9": (S, "    return feasibility <= feasibility_tol && pred <= kNoiseFloorFactor *",
              "    return feasibility <= feasibility_tol && pred <= 1e300 *", 1),
    # 3.1 the after-rungs elastic route reverted
    "M10": (S, "    const bool rungs_exhausted_to_elastic =\n        mj.walk_owns_this_qp &&",
               "    const bool rungs_exhausted_to_elastic =\n        false && mj.walk_owns_this_qp &&", 1),
    # 4.1 phase budget unset; phase budget labelled the caller's kWorkLimit
    "M11": (R, "        return samples_.front().factorizations <= horizon &&\n               !(best_ <= kRestorationStallFraction * samples_.front().best_h);",
               "        return false;", 1),
    "M12": (S, "        if (phase_stalled) {\n            st.resto.status = SolveStatus::kStalled;",
               "        if (false && phase_stalled) {\n            st.resto.status = SolveStatus::kStalled;", 1),
    # 4.2 the sub-driver's return_best reverted
    "M13": (S, "    ropts.common.return_best = ReturnBest::kOnAnyNonOptimal;",
               "    ropts.common.return_best = ReturnBest::kNever;", 1),
    # ruling B2-1: the comparator reverted to the shared order; its phase-feasibility conjunct dropped;
    # its start incumbent dropped (the start becomes capturable: the entry-point fallback)
    "M14": (S, "        if (restoration_wrapper_ != nullptr) {", "        if (false) {", 1),
    "M15": (R, "    return phase_violation <= feasibility_tol && parent_h < best_parent_h;",
               "    return parent_h < best_parent_h;", 1),
    "M16": (S, "            st.resto_best_parent_h = parent_h;\n        } else if (detail::restoration_best_improves(",
               "            st.resto_best_parent_h = std::numeric_limits<double>::infinity();\n        }\n        if (detail::restoration_best_improves(", 1),
    # B2-1: a captured-nothing phase at a limit takes the budget-best exit anyway
    "M17": (S, "if (!mj.converged && !probe_exhausted && st.mb.snap_valid &&",
               "if (!mj.converged && !probe_exhausted &&", 1),
    # 4.3 the elastic-exhaustion route inside the phase reverted to the immediate refusal
    "M18": (S, "            if (is_restoration_sub_driver_ && !stop_phase_on_qp_failure_ && mj.walk_owns_this_qp &&",
               "            if (false && is_restoration_sub_driver_ && !stop_phase_on_qp_failure_ && mj.walk_owns_this_qp &&", 1),
    # 4.4 the reduction-resume extension reverted
    "M19": (S, "         (rs.status == SolveStatus::kWorkLimit && phase_stalled) ||",
               "         (false && rs.status == SolveStatus::kWorkLimit && phase_stalled) ||", 1),
    # F-1 (22d6cccb6): the row-refusal route's exclusion of the phase's curvature-switch run dropped
    "M20": (S, "mj.qs.counters.certificate_row_refusals > 0 && !stop_phase_on_qp_failure_;",
               "mj.qs.counters.certificate_row_refusals > 0;", 1),
    # F-2 (994619746): the certificate's claim measure reverted to every row's (the dropped rows read too)
    "M21": (S, "    if (resto_rows) {\n        NlpEval ev_priced = ev_r;",
               "    if (false && resto_rows) {\n        NlpEval ev_priced = ev_r;", 1),
    # REPORTED, not must-kill: the comparator's strictness (a tie replaces the incumbent)
    "R1": (R, "    return phase_violation <= feasibility_tol && parent_h < best_parent_h;",
              "    return phase_violation <= feasibility_tol && parent_h <= best_parent_h;", 1),
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
