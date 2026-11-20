#!/usr/bin/env python3
"""TU-0: apply (or restore) one declared mutation. usage: mutate_tu0.py <id> | restore | check | ids.
`check` verifies every anchor occurs exactly once in the tree without writing. Ids and dispositions: ../DECLARATION-TU-0.md section 5.
Two kinds: literal (file, old, new) and V* = the whole condition of the `if (` that starts at the anchor replaced by `false` (validate rules),
or `if (false)` put before an enum_in_range call."""
import subprocess
import sys

S = "src/sqp/sqp_solver.cpp"
O = "src/sqp/sqp_options.cpp"
T = "include/hven/detail/sqp/globalization/trust_region.h"
N = "src/qp/ssn/ssn_engine.cpp"
P = "src/sqp/problem_scaling.cpp"
FILES = [S, O, T, N, P]

LIT = {
    "G1": (T, "        return ratio || feasibility_step;", "        return ratio;"),
    "G2": (T, "    case SqpTrustRegionGrowth::kAnyAccepted:\n        return true;", "    case SqpTrustRegionGrowth::kAnyAccepted:\n        return ratio;"),
    "G3": (T, "    case SqpTrustRegionGrowth::kRatio:\n        return ratio;", "    case SqpTrustRegionGrowth::kRatio:\n        return true;"),
    "G4": (T, "    if (failure_step || !tr_binding || !delta_finite) {", "    if (!tr_binding || !delta_finite) {"),
    "G5": (S, "trust_region_grows(opts_.advanced.trust_region_growth,", "trust_region_grows(SqpTrustRegionGrowth::kRatio,"),
    "F1": (S, "    sopts.farkas_residual_tol = opts_.advanced.ssn_farkas_residual_tol;\n", ""),
    "F2": (S, "    sopts.uncertain_tol = opts_.advanced.ssn_uncertain_tol;\n", ""),
    "F3": (S, "    sopts.lm_sigma_c = opts_.advanced.ssn_lm_sigma_c;\n", ""),
    "F4": (S, "    sopts.lm_sigma_exponent = opts_.advanced.ssn_lm_sigma_exponent;\n", ""),
    "F5": (N, "rel_resid <= resid_tol &&", "rel_resid <= detail::kSsnFarkasResidualTol &&"),
    "F6": (N, "sopts.lm_sigma_c * std::min(r, rp)", "detail::kSsnLmSigmaC * std::min(r, rp)"),
    "F7": (N, "sopts.lm_sigma_exponent == 2.0 ? r * r : std::pow(r, sopts.lm_sigma_exponent)", "r * r"),
    "E1": (S, "if (have_p && opts_.advanced.restoration_elastic_seed) {", "if (have_p) {"),
    "H1": (S, "for (Index k = 0; k < headroom_rungs; ++k) {", "for (Index k = 0; k < 1; ++k) {"),
    "H2": (S, "opts.advanced.elastic_rho0_headroom_rungs, rho0_ceiling_hit)", "1, rho0_ceiling_hit)"),
    "M1": (S, "                ? opts_.advanced.adaptive_dual_regularization_max\n", "                ? kAdaptiveMuMax\n"),
    "M2": (S, "kAdaptiveMuMin,\n                             opts_.advanced.adaptive_dual_regularization_max);", "kAdaptiveMuMin,\n                             kAdaptiveMuMax);"),
    "R1": (P, "        if (rule.power_of_two_rows) {\n            s.eq_rows(i)", "        if (false) {\n            s.eq_rows(i)"),
    "R2": (P, "        if (rule.power_of_two_rows) {\n            s.ineq_rows(i)", "        if (false) {\n            s.ineq_rows(i)"),
    "R3": (S, "rule.power_of_two_rows = opts_.row_scaling_power_of_two;", "rule.power_of_two_rows = false;"),
}
# validate rules: the anchor starts at `if (` (condition replaced by false) or at `enum_in_range(` (guarded by if (false))
VIF = {
    "V1": "if (!(opts.qp.dual_regularization >= 0.0)",
    "V2": "if (!(opts.qp.primal_regularization >= 0.0)",
    "V3": "if (!(opts.qp.feasibility_tol > 0.0)",
    "V4": "if (!(opts.qp.optimality_tol > 0.0)",
    "V5": "if (!(opts.qp.schur_condition_max >= 1.0)",
    "V6": "if (!(opts.qp.max_schur_border_dimension >= 0)",
    "V8": "if (!(opts.qp.trust_region_radius >= 0.0)",
    "V10": "if (!(opts.advanced.ssn_farkas_residual_tol > 0.0)",
    "V11": "if (!(opts.advanced.ssn_uncertain_tol >= 0.0)",
    "V12": "if (!(opts.advanced.ssn_lm_sigma_c > 0.0)",
    "V13": "if (!(opts.advanced.ssn_lm_sigma_exponent >= 1.0)",
    "V14": "if (!(opts.advanced.elastic_rho0_headroom_rungs >= 1)",
    "V15": "if (!(opts.advanced.adaptive_dual_regularization_max >= 1e-12)",
}
VENUM = {
    "V7": "enum_in_range(opts.qp.working_set_algebra,",
    "V9": "enum_in_range(opts.advanced.trust_region_growth,",
}
ORDER = list(LIT) + list(VIF) + list(VENUM)


def read(p):
    return open(p).read()


def count(p, a):
    return read(p).count(a)


def match_paren(s, i):
    d = 0
    for j in range(i, len(s)):
        if s[j] == "(":
            d += 1
        elif s[j] == ")":
            d -= 1
            if d == 0:
                return j
    raise SystemExit("UNBALANCED")


def apply(i):
    if i in LIT:
        p, old, new = LIT[i]
        s = read(p)
        assert s.count(old) == 1, (i, s.count(old))
        open(p, "w").write(s.replace(old, new))
    elif i in VIF:
        s = read(O)
        a = VIF[i]
        assert s.count(a) == 1, (i, s.count(a))
        k = s.index(a)
        po = k + len("if ")
        pc = match_paren(s, po)
        open(O, "w").write(s[:po] + "(false)" + s[pc + 1:])
    elif i in VENUM:
        s = read(O)
        a = VENUM[i]
        assert s.count(a) == 1, (i, s.count(a))
        open(O, "w").write(s.replace(a, "if (false) " + a))
    else:
        raise SystemExit("unknown id " + i)


def main():
    c = sys.argv[1]
    if c == "restore":
        subprocess.check_call(["git", "checkout", "--"] + FILES)
    elif c == "ids":
        print(" ".join(ORDER))
    elif c == "check":
        bad = 0
        for i in ORDER:
            a = LIT[i][:2] if i in LIT else (O, VIF.get(i) or VENUM[i])
            n = count(a[0], a[1])
            if n != 1:
                print("ANCHOR", i, n)
                bad = 1
        print("CHECK_OK" if not bad else "CHECK_FAIL")
        sys.exit(bad)
    else:
        apply(c)
        print("MUTATED", c)


main()
