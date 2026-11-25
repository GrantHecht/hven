#!/usr/bin/env python3
"""I19-2a: apply (or restore) one declared mutation. usage: [MUT_TREE=<dir>] mutate_i19.py <id> | restore | ids | check.
Ids are declared in ../DECLARATION-I19-2a.md section 8. Each edit's anchor must occur exactly the stated number of times;
`check` applies every mutation in turn (restoring after each) and fails on the first anchor that does not apply.
(RF-1c mutate_rf1c_r3.py, with multi-edit entries.)"""
import os
import subprocess
import sys

S = "src/sqp/sqp_solver.cpp"
T = "include/hven/detail/sqp/globalization/trust_region.h"
O = "include/hven/sqp/sqp_options.h"

# id: [(file, old, new, expected occurrences), ...]
MUT = {
    # ---- G1: kRatioByStepType --------------------------------------------------------------------
    "M1": [(T, "        candidate = in.h_lin.has_value() && pred_dh > noise &&\n                    in.h_old - in.h_new >= kTrGrowThreshold * pred_dh;",
               "        candidate = f_ratio_ok;", 1)],          # the h branch reads the objective ratio
    "M2": [(T, "        candidate = in.h_lin.has_value() && pred_dh > noise &&",
               "        candidate = (!in.h_lin.has_value() || pred_dh > noise) &&", 1)],   # a null linearization grows
    "M3": [(T, "        candidate = in.h_lin.has_value() && pred_dh > noise &&",
               "        candidate = in.h_lin.has_value() &&", 1)],                         # pred_dh <= 0 grows
    "M3b": [(T, "        candidate = in.h_lin.has_value() && pred_dh > noise &&",
                "        candidate = in.h_lin.has_value() && pred_dh > 0.0 &&", 1)],      # the noise floor dropped
    "M19": [(S, "        gin.h_new = mj.ctx.h_new;", "        gin.h_new = mj.trial_h_new;", 1)],  # actual_dh from the original trial, not the SOC ctx
    "M20": [(S, "        mj.h_lin_model = model_linearized_violation_l1(st.ev, mj.qs.x);",
                "        mj.h_lin_model = linearized_violation_l1(st.qp, mj.qs.x, seam.me(), seam.mi(), st.solve_scaling);", 1)],  # h_lin in caller units
    # ---- G2 ----------------------------------------------------------------------------------
    "M4": [(T, "    if (!a.trust_region_growth_after_soc && in.soc_applied) {", "    if (false) {", 1)],
    # ---- G3: memory ----------------------------------------------------------------------------
    "M5": [(S, "            st.tr_memory.record_rejection(st.delta, mj.radius_from_growth);", "            (void)0;", 1)],   # never recorded
    "M5b": [(S, "            st.tr_memory.record_rejection(st.delta, mj.radius_from_growth);",
                "            st.tr_memory.record_rejection(st.delta * kTrShrinkFactor, mj.radius_from_growth);", 1)],   # the cap recorded after the shrink
    "M5c": [(S, "            st.tr_memory.record_rejection(st.delta, mj.radius_from_growth);",
                "            st.tr_memory.record_rejection(st.delta, true);", 1)],   # every rejection recorded, grown radius or not
    "M6": [(T, "        if (++clean_ >= window) {", "        if (++clean_ >= window && false) {", 1)],   # never clears
    "M7": [(T, "        cap_ = std::min(cap_, radius);\n        clean_ = 0;", "        cap_ = std::min(cap_, radius);", 1)],   # a rejection does not restart the count
    "M7b": [(S, "        gin.rejection_cap = st.tr_memory.cap();",
                "        if (opts_.advanced.trust_region_rejection_memory > 0) {\n            st.tr_memory.after_acceptance(opts_.advanced.trust_region_rejection_memory);\n        }\n        gin.rejection_cap = st.tr_memory.cap();", 1),
            (S, "            if (opts_.advanced.trust_region_rejection_memory > 0) {\n                st.tr_memory.after_acceptance(opts_.advanced.trust_region_rejection_memory);\n            }\n",
                "", 1)],   # the clean count incremented before the growth decision
    "M28": [(T, "in.grown_radius >= in.rejection_cap", "in.grown_radius > in.rejection_cap", 1)],   # >= becomes >
    "M17": [(S, "        st.tr_memory.clear(); // I19-2a: a fresh phase, a fresh memory\n", "", 1)],   # not cleared at resume
    "M18": [(S, "            st.tr_memory.clear(); // I19-2a: the rebase restarts the radius's history\n", "", 1)],   # not cleared at a watchdog rebase
    "M29": [(S, "        if (opts_.advanced.trust_region_rejection_memory > 0) {\n            st.tr_memory.record_rejection(st.rollback.delta, st.rollback.radius_from_growth);\n        }\n", "", 1)],   # a rollback is not a rejection for the memory
    # ---- G4: ratio ceiling ---------------------------------------------------------------------
    "M8": [(T, "        if (granted_by && *granted_by > a.trust_region_growth_ratio_max) {", "        if (false) {", 1)],
    "M9": [(T, "        case SqpTrustRegionGrowth::kAnyAccepted:\n            break;\n        case SqpTrustRegionGrowth::kRatioByStepType:",
               "        case SqpTrustRegionGrowth::kAnyAccepted:\n            granted_by = in.actual_df / in.pred_df;\n            break;\n        case SqpTrustRegionGrowth::kRatioByStepType:", 1)],   # the ceiling reads a ratio-free grant
    "M9b": [(T, "            if (!in.feasibility_step) {\n                granted_by = in.actual_df / in.pred_df;\n            }",
                "            granted_by = in.actual_df / in.pred_df;", 1)],   # the ceiling reads kRatioOrFeasibilityStep's feasibility grant
    "M9c": [(T, "            granted_by = h_branch ? h_ratio : in.actual_df / in.pred_df;",
                "            granted_by = in.actual_df / in.pred_df;", 1)],   # the ceiling reads the objective ratio on an h-type step
    # ---- G5: shrink to step --------------------------------------------------------------------
    "M10": [(T, "    if (shrink_to_step && !radius_binding && !failure_step) {", "    if (shrink_to_step && !failure_step) {", 1)],   # ignores radius_binding
    "M10b": [(T, "    if (shrink_to_step && !radius_binding && !failure_step) {", "    if (shrink_to_step && !radius_binding) {", 1)],   # failure steps not excluded
    "M10c": [(T, "            std::min(kTrShrinkFactor * std::min(delta, window), kTrShrinkFactor * step_inf);",
                 "            kTrShrinkFactor * step_inf;", 1)],   # the min with 0.5 delta_eff dropped
    "M10d": [(T, "            std::min(kTrShrinkFactor * std::min(delta, window), kTrShrinkFactor * step_inf);",
                 "            std::min(kTrShrinkFactor * delta, kTrShrinkFactor * step_inf);", 1)],   # delta, not the effective window
    "M11": [(T, "        return {r, true, r < floor};", "        return {r, true, delta * kTrShrinkFactor < floor};", 1)],   # the floor test reads 0.5 delta
    "M21": [(S, "        case RetryRung::kShrink:\n            ++st.out.counters.qp_retry_rung_shrink;\n            st.delta = shrunk_radius(st.delta);",
                "        case RetryRung::kShrink:\n            ++st.out.counters.qp_retry_rung_shrink;\n            st.delta = opts_.advanced.trust_region_shrink_to_step\n                           ? std::min(shrunk_radius(st.delta), kTrShrinkFactor * mj.row.step_norm)\n                           : shrunk_radius(st.delta);", 1)],   # leaks to the retry rung
    "M22": [(S, "        st.delta = shrunk_radius(st.rollback.delta);",
                "        st.delta = opts_.advanced.trust_region_shrink_to_step\n                       ? kTrShrinkFactor * mj.row.step_norm\n                       : shrunk_radius(st.rollback.delta);", 1)],   # leaks to the rollback
    "M23": [(S, "            st.delta, opts_.qp.trust_region_radius, mj.row.step_norm, opts_.trust_region_min,",
                "            st.delta, opts_.qp.trust_region_radius,\n            mj.qs_soc.x.size() > 0 ? mj.qs_soc.x.lpNorm<Eigen::Infinity>() : mj.row.step_norm,\n            opts_.trust_region_min,", 1)],   # reads the SOC step's norm
    # ---- G6: cap pins --------------------------------------------------------------------------
    "M12": [(T, "    if (!a.trust_region_growth_on_step_limit && !in.radius_binding) {",
                "    if (!a.trust_region_growth_on_step_limit && !in.region_limited) {", 1)],   # reads the merged flag
    "M12b": [(S, "        gin.radius_binding = mj.row.radius_binding;",
                 "        gin.radius_binding = mj.row.radius_binding && !mj.row.cap_binding;", 1)],   # a mixed step refused
    # ---- G7: funnel headroom -------------------------------------------------------------------
    "M25": [(T, "    if (a.trust_region_growth_funnel_guard && std::isfinite(in.width)) {", "    if (false) {", 1)],
    "M26": [(S, "        gin.h_trial = mj.trial_h_new;", "        gin.h_trial = mj.ctx.h_new;", 1)],   # reads the SOC point
    "M27": [(S, "                gin.width = funnel->width();", "                (void)funnel;", 1)],   # the width never reaches it
    # ---- attribution and counters --------------------------------------------------------------
    "M31": [(T, "    if (!base && !candidate) {\n        return {};\n    }", "    if (false) {\n        return {};\n    }", 1)],   # levers consulted before the base rule
    "M15": [(S, "    mj.radius_from_growth = st.radius_from_growth;\n    st.radius_from_growth = false;",
                "    mj.radius_from_growth = st.radius_from_growth;", 1)],   # the grow-then-reject flag never cleared
    "M16": [(S, "            if (mj.growth.grows) {\n                if (grown > st.delta) {\n                    ++st.out.counters.trust_region_growths;",
                "            if (grown > st.delta) {\n                ++st.out.counters.trust_region_growths;\n            }\n            if (mj.growth.grows) {\n                if (grown > st.delta) {", 1)],   # trust_region_growths counted before the refusals
    "M30": [(S, "        if (st.rollback.grew) {\n            --st.out.counters.trust_region_growths;\n        }\n", "", 1)],   # a rolled-back growth stays counted
    # ---- pre-freeze review additions (F5) --------------------------------------------------------
    "M32": [(T, "        const double lin = in.h_lin.value_or(0.0);",
                "        if (!in.h_lin.has_value()) {\n            refusal |= SqpTrustRegionGrowthRefusal::kFunnelHeadroom;\n        }\n        const double lin = in.h_lin.value_or(0.0);", 1)],   # G7: a missing h_lin refuses (declared: reads 0)
    "M33": [(T, "lin + 4.0 * std::max(0.0, in.h_trial - lin) >", "lin + 4.0 * (in.h_trial - lin) >", 1)],   # G7: the max(0, .) dropped
    "M34": [(T, "        cap_ = std::min(cap_, radius);", "        cap_ = radius;", 1)],   # the memory keeps the LAST rejected radius, not the smallest
    "M35": [(S, "        st.tr_memory = st.rollback.tr_memory;\n", "", 1)],   # a rollback keeps the memory the undone commit advanced
    "M36": [(S, "        if (shrink.to_step) {\n            ++st.out.counters.trust_region_shrinks_to_step; // counted only when the shrink is taken\n        }\n", "", 1),
            (S, "        if (shrink.hits_floor) {", "        if (shrink.to_step) {\n            ++st.out.counters.trust_region_shrinks_to_step;\n        }\n        if (shrink.hits_floor) {", 1)],   # the shrink counted before the floor test
    "M37": [(S, "                if (mj.growth.refusal != SqpTrustRegionGrowthRefusal::kNone) {\n                    ++st.out.counters.trust_region_growth_refusals;\n                }\n                publish_stop_at(", "                publish_stop_at(", 1)],   # the stop-poll exit does not count the pushed row's refusal
    "M38": [(S, "        if (st.rollback.radius_from_growth) {\n            ++st.out.counters.trust_region_grown_step_rejections;\n        }\n", "", 1)],   # a rolled-back grown step is not a grown-then-rejected one
    "M24": [(S, "        st.out.counters.soc_qp_iteration_limits += phase.counters.soc_qp_iteration_limits;\n",
                "        st.out.counters.soc_qp_iteration_limits += phase.counters.soc_qp_iteration_limits;\n        st.out.counters.trust_region_growths += phase.counters.trust_region_growths;\n        st.out.counters.trust_region_grown_step_rejections += phase.counters.trust_region_grown_step_rejections;\n", 1)],   # restoration's counters folded
    # ---- default flips -------------------------------------------------------------------------
    "D1": [(O, "    SqpTrustRegionGrowth trust_region_growth = SqpTrustRegionGrowth::kRatio;",
               "    SqpTrustRegionGrowth trust_region_growth = SqpTrustRegionGrowth::kRatioByStepType;", 1)],
    "D2": [(O, "    bool trust_region_growth_after_soc = true;", "    bool trust_region_growth_after_soc = false;", 1)],
    "D3": [(O, "    Index trust_region_rejection_memory = 0;", "    Index trust_region_rejection_memory = 3;", 1)],
    "D4": [(O, "    double trust_region_growth_ratio_max = std::numeric_limits<double>::infinity();",
               "    double trust_region_growth_ratio_max = 3.0;", 1)],
    "D5": [(O, "    bool trust_region_shrink_to_step = false;", "    bool trust_region_shrink_to_step = true;", 1)],
    "D6": [(O, "    bool trust_region_growth_on_step_limit = true;", "    bool trust_region_growth_on_step_limit = false;", 1)],
    "D7": [(O, "    bool trust_region_growth_funnel_guard = false;", "    bool trust_region_growth_funnel_guard = true;", 1)],
}

ROOT = os.environ.get("MUT_TREE") or subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()
FILES = sorted({e[0] for v in MUT.values() for e in v})


def restore():
    subprocess.check_call(["git", "-C", ROOT, "checkout", "--"] + FILES)


def apply(mid):
    texts = {}
    for f, old, new, want in MUT[mid]:
        s = texts.get(f) or open(f"{ROOT}/{f}").read()
        n = s.count(old)
        if n != want:
            print(f"ANCHOR {mid} {f}: {n} occurrences, want {want}")
            return 1
        texts[f] = s.replace(old, new)
    for f, s in texts.items():
        open(f"{ROOT}/{f}", "w").write(s)
    print(f"APPLIED {mid}")
    return 0


def main():
    arg = sys.argv[1]
    if arg == "restore":
        restore()
        print("RESTORED")
        return 0
    if arg == "ids":
        print(" ".join(MUT))
        return 0
    if arg == "check":
        for mid in MUT:
            rc = apply(mid)
            restore()
            if rc:
                print(f"PREFLIGHT_DOES_NOT_APPLY {mid}")
                return 2
        print("PREFLIGHT every mutation applies and restores")
        return 0
    return apply(arg)


if __name__ == "__main__":
    sys.exit(main())
