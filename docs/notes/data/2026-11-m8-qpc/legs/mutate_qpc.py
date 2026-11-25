#!/usr/bin/env python3
"""QPC: apply (or restore) one declared mutation. usage: [MUT_TREE=<dir>] mutate_qpc.py <id> | restore | ids | check | argued.
Ids are declared in ../DECLARATION-QPC.md section 8. Each edit's anchor must occur exactly the stated number of times;
`check` applies every mutation in turn (restoring after each) and fails on the first anchor that does not apply.
`argued` prints the ids section 8 of the declaration marks ARGUED (the runner's parity check reads it).
(I19-2a mutate_i19.py, with the argued-set parser added.)"""
import os
import re
import subprocess
import sys

W = "src/qp/walk/walk_engine.cpp"
WH = "include/hven/detail/qp/walk/walk_engine.h"
E = "include/hven/detail/qp/walk/eqp_solve.h"
B = "include/hven/detail/qp/walk/bordered_eqp.h"
S = "src/sqp/sqp_solver.cpp"

GATE = ("                            if (exported_finite &&\n"
        "                                detail::certificate_data_half_holds(\n"
        "                                    data.rows, data.complementarity / data.dual_scale,\n"
        "                                    data.stationarity_beyond_rounding, eff_opts.feasibility_tol,\n"
        "                                    eff_opts.optimality_tol)) {")
HOLDS = ("    return rows <= kCertificateGateRowFactor * feas_tol &&\n"
         "           complementarity <= kCertificateGateComplFactor * feas_tol &&\n"
         "           stationarity <= kCertificateGateStationarityFactor * opt_tol;")
ARM = "            eqp_refinement_ == EqpRefinement::kModel && eff_opts.dual_regularization > 0.0) {\n            ++counters.converged_refinement_arms;\n            eqp_refinement_ = EqpRefinement::kConverged;\n        }"

# id: [(file, old, new, expected occurrences), ...]
MUT = {
    # ---- C2: the certificate's data half ---------------------------------------------------------
    "Q1": [(W, GATE, "                            if (true) {", 1)],
    "Q2": [(WH, HOLDS, "    return rows <= kCertificateGateRowFactor * feas_tol;", 1)],
    "Q3": [(WH, HOLDS, "    return complementarity <= kCertificateGateComplFactor * feas_tol;", 1)],
    "Q4": [(WH, "constexpr double kCertificateGateRowFactor = kInfeasibilityMarginFactor;",
                "constexpr double kCertificateGateRowFactor = 100.0;", 1)],
    "Q5": [(WH, "constexpr double kCertificateGateComplFactor = kTerminalPolishComplFactor;",
                "constexpr double kCertificateGateComplFactor = 1000.0;", 1)],
    "Q6": [(WH, HOLDS, "    return !(rows > kCertificateGateRowFactor * feas_tol) &&\n           !(complementarity > kCertificateGateComplFactor * feas_tol) &&\n           !(stationarity > kCertificateGateStationarityFactor * opt_tol);", 1)],
    "Q7": [(W, "                                ++counters.certificate_gate_refusals;\n                                status = QpStatus::kNumericalError;",
               "                                ++counters.certificate_gate_refusals;\n                                status = QpStatus::kInfeasible;", 1)],
    "Q8": [(W, "                                ++counters.certificate_gate_refusals;\n                                status = QpStatus::kNumericalError;",
               "                                status = QpStatus::kNumericalError;", 1)],
    "Q9": [(WH, "        out.rows = std::max(out.rows, std::max(below, above) / row_scale);\n        const BoundState st",
                "        const BoundState st", 1)],
    # ---- C1: the converged refinement at the release re-admission --------------------------------
    "Q10": [(W, ARM, "            false) {\n        }", 1)],
    "Q11": [(W, "            ++counters.converged_refinement_arms;\n            eqp_refinement_ = EqpRefinement::kConverged;\n",
                "            ++counters.converged_refinement_arms;\n            eqp_refinement_ = EqpRefinement::kConverged;\n            ++counters.dual_regularization_decreases;\n            eff_opts.dual_regularization *= detail::kDualRegDecreaseFactor;\n            discard_factor_state();\n", 1)],
    "Q12": [(E, "            if (refinement == EqpRefinement::kConverged && opts.dual_regularization > 0.0) {",
                "            if (false) {", 1)],
    "Q13": [(B, "    if (refinement == EqpRefinement::kConverged && opts.dual_regularization > 0.0 && !shifted_k0) {",
                "    if (false) {", 1)],
    "Q14": [(E, "        if (!(cres_inf < res_inf)) {\n            break; // stagnated, diverging, or NaN -- keep the better answer\n        }\n        y = cand;",
                "        y = cand;", 1)],
    "Q15": [(W, "    eqp_refinement_ = EqpRefinement::kModel;\n    struct RefinementReset {\n        EqpRefinement &mode;\n        ~RefinementReset() { mode = EqpRefinement::kModel; }\n    } const refinement_reset{eqp_refinement_};\n",
                "", 1)],
    "Q16": [(W, "    eqp_refinement_ = EqpRefinement::kModel;\n    struct RefinementReset {",
                "    eqp_refinement_ = EqpRefinement::kConverged;\n    struct RefinementReset {", 1)],
    "Q17": [(W, ARM, "            eff_opts.dual_regularization > 0.0) {\n            ++counters.converged_refinement_arms;\n            eqp_refinement_ = EqpRefinement::kConverged;\n        }", 1)],
    "Q18": [(S, "    st.out.counters.converged_refinement_arms += mj.qs.counters.converged_refinement_arms;\n    st.out.counters.certificate_gate_refusals += mj.qs.counters.certificate_gate_refusals;\n", "", 1)],
    "Q19": [(E, "                    detail::kMaxBorderRefineSteps - 1,\n                    [&](const Vec &r) { return detail::solve_vec(kkt, r); }, residual_of);",
                "                    1,\n                    [&](const Vec &r) { return detail::solve_vec(kkt, r); }, residual_of);", 1),
            (B, "            best, res, floor, detail::kMaxBorderRefineSteps - 1,", "            best, res, floor, 1,", 1)],
    # ---- C2, settler C553 Q3: stationarity re-read on the exported point; the finiteness guard ----
    "Q20": [(WH, HOLDS, "    return rows <= kCertificateGateRowFactor * feas_tol &&\n           complementarity <= kCertificateGateComplFactor * feas_tol;", 1)],
    "Q21": [(W, "                            if (exported_finite &&\n                                detail::certificate_data_half_holds(",
                "                            if (detail::certificate_data_half_holds(", 1)],
    # ---- C2, the Q5 defect fix: the product read against the dual scale --------------------------
    "Q22": [(W, "                                    data.rows, data.complementarity / data.dual_scale,\n",
                "                                    data.rows, data.complementarity,\n", 1)],
}

ROOT = os.environ.get("MUT_TREE") or subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True).strip()
FILES = sorted({e[0] for v in MUT.values() for e in v})
DECL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "DECLARATION-QPC.md")


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


def argued_from_declaration():
    """Ids whose section-8 table row's killer column starts with ARGUED."""
    txt = open(DECL).read()
    sec = txt.split("## 8.", 1)[1].split("\n## ", 1)[0]
    ids = []
    for line in sec.splitlines():
        m = re.match(r"^\|\s*(Q\d+)\s*\|[^|]*\|\s*(.*)\|\s*$", line)
        if m and m.group(2).strip().startswith("ARGUED"):
            ids.append(m.group(1))
    return ids


def main():
    arg = sys.argv[1]
    if arg == "restore":
        restore()
        print("RESTORED")
        return 0
    if arg == "ids":
        print(" ".join(MUT))
        return 0
    if arg == "argued":
        print(" ".join(argued_from_declaration()))
        return 0
    if arg == "check":
        missing = [m for m in re.findall(r"^\|\s*(Q\d+)\s*\|", open(DECL).read(), re.M) if m not in MUT]
        if missing:
            print(f"TABLE_ID_NOT_IN_SCRIPT {missing}")
            return 2
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
