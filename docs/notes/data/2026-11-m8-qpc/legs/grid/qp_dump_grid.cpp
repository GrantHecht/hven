// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// qp_dump_grid.cpp -- M8 QPC (docs/notes/data/2026-11-m8-qpc/DECLARATION-QPC.md, SENS-Q): one walk
// dump (tests/qp/fixtures/e2-d4, format "D4QP 1") re-solved at the walk seam on one working-set
// algebra, one CSV row appended per invocation. The SAME source is built against the baseline tree
// (R) and the round's trees, so it names only what both have: the QpCounters fields present at the
// baseline 99712ec32 (listed below, in declaration order) and the two QPC counters, printed as NA
// where the build does not have them (detected with a requires-expression, not a version macro).
//
// Columns: file, algebra, status, objective (hexfloat and %.17g), a 64-bit FNV-1a hash of the x
// hexfloats (x_fnv), every counter, then two INDEPENDENT readings of the returned point, neither
// taken from the engine:
//   d_rows, d_product, d_dual_scale, d_stationarity -- the certificate's data half recomputed from QpSolution
//     (rows relative to max(1,|b|,||a||_1||x||inf), bounds against the dump's EFFECTIVE box -- real
//     bounds intersected with the trust region about the clamped seed -- relative to
//     max(1,|bound|,||x||inf); product over equalities, rows flagged ineq_active and pinned bounds;
//     free-block stationarity Hx+g+Ae'le+Ai'li over the columns neither pinned nor TR-held, over
//     max(1,|Hx|,|g|,|Ae'le|,|Ai'li|); the dual scale = that scale joined by ||z||inf, which the
//     gate divides the product by). NaN-sticky.
//   k_eq, k_ineq, k_box, k_stationarity, k_stationarity_beyond_rounding, k_sign, k_compl, k_scale --
//     tests/qp/support/qp_dump.h's qp_dump_kkt (absolute primal terms, TR-aware);
//   feas_tol, opt_tol -- the dump's own tolerances (the verdict thresholds' units).
// Built against an hven INSTALL by legs/grid/CMakeLists.txt (legs/build_grid.sh), one binary per arm.
// usage: hven_qp_dump_grid --file DUMP --algebra refactorize|border --csv OUT.csv
// The per-cell kill is the caller's (legs/legN0.sh: timeout per invocation).

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <utility>

#include "qp/support/qp_dump.h" // from the hven tree's tests/ (identical at 99712ec32 and qpc)

namespace {

using hven::Index;
using hven::Vec;
using hven::detail::QpSolution;
using hven::test_support::QpDump;

#define HVEN_QPC_GRID_R_FIELDS(X) \
    X(factorizations) \
    X(schur_updates) \
    X(minor_iters) \
    X(eqp_refine_steps) \
    X(border_refine_steps) \
    X(eqp_refine_iters) \
    X(eqp_residual_refusals) \
    X(eqp_row_drift_refusals) \
    X(eqp_polish_steps) \
    X(verdict_refine_steps) \
    X(suspect_escalations) \
    X(dual_regularization_decreases) \
    X(k0_reused) \
    X(symbolic_analyses) \
    X(ws_adds) \
    X(ws_drops) \
    X(shift_adds) \
    X(degenerate_steps) \
    X(degenerate_run_max) \
    X(ws_adds_bound) \
    X(ws_drops_bound) \
    X(distinct_ineq_added) \
    X(distinct_bound_added) \
    X(drop_ties) \
    X(ratio_ties) \
    X(dep_test_solves) \
    X(dep_tests) \
    X(dep_dependent) \
    X(dep_undecidable) \
    X(entry_dependency_removals) \
    X(mandatory_dependent_entries) \
    X(dep_recheck_removals) \
    X(dep_ride_blockers) \
    X(ride_exchanges) \
    X(ride_declines_fallthrough_blocked) \
    X(ride_sign_vetoes) \
    X(ride_predicted_signs) \
    X(eq_drift_violations) \
    X(saddle_exits) \
    X(saddle_exit_misses) \
    X(saddle_direction_solves) \
    X(convex_certificates) \
    X(ascent_rides) \
    X(ascent_footprint_rides) \
    X(ascent_footprint_rungs) \
    X(ascent_footprint_rung_reverts) \
    X(ascent_footprint_ride_refusals) \
    X(ascent_footprint_declines) \
    X(dep_pin_skips) \
    X(ws_count_violations) \
    X(dep_untested_growth) \
    X(dep_screen_under_mandatory_dependence) \
    X(ws_exchanges) \
    X(ws_exchanges_bound) \
    X(ws_exchanges_shifted) \
    X(dep_infeasible_certificates) \
    X(dep_certificate_toleranced) \
    X(dep_certificate_rejections) \
    X(dep_passovers) \
    X(dep_blocker_terminations) \
    X(dep_deferred_adds) \
    X(dep_border_retreats) \
    X(standstill_retreats) \
    X(border_uncertified_fallbacks) \
    X(dep_structural_exact) \
    X(dep_known_dependent_refusals) \
    X(dep_uncertified_trims) \
    X(dep_uncertified_fallbacks) \
    X(dep_stall_admits) \
    X(dep_unrun_tests) \
    X(dep_repair_screen_pins) \
    X(dep_repair_budget_stops) \
    X(dep_repair_restarts) \
    X(dep_implied_passovers) \
    X(rank_repairs) \
    X(rank_repair_releases) \
    X(rank_repair_misses) \
    X(dep_refused_exchanges) \
    X(reentry_exemptions) \
    X(zero_motion_exhausted) \
    X(zero_motion_cap_hits) \
    X(dep_structural_first) \
    X(dep_unlocks) \
    X(dep_inertia_rereads) \
    X(dep_inertia_reread_flips) \
    X(dep_grey_rereads) \
    X(dep_grey_reread_dependent) \
    X(terminal_mu_refines) \
    X(terminal_mu_refine_rescues) \
    X(terminal_polish_solves) \
    X(terminal_polish_rejected) \
    X(terminal_polish_adopted) \
    X(certificate_sign_refusals) \
    X(saddle_shifted_searches) \
    X(saddle_shifted_directions) \
    X(terminal_polish_sign_reverts) \
    X(certificate_bound_refusals) \
    X(terminal_polish_bound_reverts) \
    X(terminal_farkas_certificates) \
    X(certificate_row_refusals) \
    X(stagnation_drops) \
    X(stagnation_stops) \
    X(border_latches_cap) \
    X(border_latches_conditioning) \
    X(border_latch_releases) \
    X(border_latch_holds) \
    X(inertia_suspect_verdicts) \
    X(inertia_wrong_verdicts) \
    X(dense_platform_retakes) \
    X(dense_flagged_factorizations) \
    X(dense_singular_refactors) \
    X(dense_shifted_solve_refusals) \
    X(dense_shifted_k0_fallbacks) \
    X(dense_shifted_terminations) \
    X(dense_solve_residual_refusals) \
    X(dense_factorizations) \
    X(optimal_row_gray_exits) \
    X(sparse_scaled_retakes) \
    X(sparse_matched_retakes) \
    X(sparse_retake_refusals) \
    X(symbolic_analysis_reuses) \
    /* end */

template <class C>
long long converged_refinement_arms_of(const C &c) {
    if constexpr (requires { c.converged_refinement_arms; }) {
        return static_cast<long long>(c.converged_refinement_arms);
    } else {
        return std::numeric_limits<long long>::min();
    }
}
template <class C>
long long certificate_gate_refusals_of(const C &c) {
    if constexpr (requires { c.certificate_gate_refusals; }) {
        return static_cast<long long>(c.certificate_gate_refusals);
    } else {
        return std::numeric_limits<long long>::min();
    }
}

std::string opt_count(long long v) {
    return v == std::numeric_limits<long long>::min() ? std::string("NA") : std::to_string(v);
}

std::string hexd(double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%a", v);
    return b;
}
std::string decd(double v) {
    char b[64];
    std::snprintf(b, sizeof b, "%.17g", v);
    return b;
}

std::uint64_t x_hash(const Vec &x) {
    std::uint64_t h = 1469598103934665603ULL;
    for (Index i = 0; i < x.size(); ++i) {
        const std::string s = hexd(x(i)) + ";";
        for (const char ch : s) {
            h ^= static_cast<unsigned char>(ch);
            h *= 1099511628211ULL;
        }
    }
    return h;
}

// max that keeps a NaN (std::max drops a NaN second argument).
double nmax(double a, double b) { return (std::isnan(a) || std::isnan(b)) ? std::numeric_limits<double>::quiet_NaN() : std::max(a, b); }

struct DataHalf {
    double rows = 0.0, product = 0.0, stationarity = 0.0, dual_scale = 1.0;
};

DataHalf data_half(const QpDump &d, const QpSolution &s) {
    const auto &qp = d.qp;
    const Index n = qp.n();
    DataHalf r;
    Vec lo = qp.lower, up = qp.upper;
    const double tr = std::isfinite(d.overrides.trust_region_radius) ? d.overrides.trust_region_radius
                                                                     : d.opts.trust_region_radius;
    if (std::isfinite(tr)) {
        Vec x0 = d.has_seed ? d.seed.x : Vec::Zero(n);
        for (Index i = 0; i < n; ++i) {
            x0(i) = std::clamp(x0(i), qp.lower(i), qp.upper(i));
            lo(i) = std::max(lo(i), x0(i) - tr);
            up(i) = std::min(up(i), x0(i) + tr);
        }
    }
    const double xmag = s.x.lpNorm<Eigen::Infinity>();
    const bool have_le = s.lambda_e.size() == qp.me(), have_li = s.lambda_i.size() == qp.mi();
    if (qp.me() > 0) {
        const Vec res = qp.Ae * s.x - qp.be;
        for (Index j = 0; j < qp.me(); ++j) {
            const double rs = std::max({1.0, std::abs(qp.be(j)), qp.Ae.row(j).cwiseAbs().sum() * xmag});
            const double rel = std::abs(res(j)) / rs;
            r.rows = nmax(r.rows, rel);
            r.product = nmax(r.product, rel * (have_le ? std::abs(s.lambda_e(j)) : 0.0));
        }
    }
    if (qp.mi() > 0) {
        const Vec res = qp.Ai * s.x - qp.bi;
        for (Index j = 0; j < qp.mi(); ++j) {
            const double rs = std::max({1.0, std::abs(qp.bi(j)), qp.Ai.row(j).cwiseAbs().sum() * xmag});
            r.rows = nmax(r.rows, std::max(0.0, res(j)) / rs + (std::isnan(res(j)) ? res(j) : 0.0));
            const bool active = static_cast<std::size_t>(j) < s.ineq_active.size() && s.ineq_active[static_cast<std::size_t>(j)];
            if (active) {
                r.product = nmax(r.product, std::abs(res(j)) / rs * (have_li ? std::abs(s.lambda_i(j)) : 0.0));
            }
        }
    }
    std::vector<bool> pinned(static_cast<std::size_t>(n), false);
    for (Index i = 0; i < n; ++i) {
        const double below = lo(i) - s.x(i), above = s.x(i) - up(i);
        const double bound = below >= above ? lo(i) : up(i);
        const double rs = std::max({1.0, std::abs(bound), xmag});
        r.rows = nmax(r.rows, std::max(below, above) / rs);
        const auto si = static_cast<std::size_t>(i);
        const bool tr_held = si < s.tr_active.size() && s.tr_active[si];
        const bool at_bound = si < s.bound_state.size() && s.bound_state[si] != hven::detail::BoundState::kFree;
        pinned[si] = tr_held || at_bound;
        if (at_bound && s.z.size() == n) {
            const double at = s.bound_state[si] == hven::detail::BoundState::kAtUpper ? up(i) : lo(i);
            r.product = nmax(r.product, std::abs(s.x(i) - at) / rs * std::abs(s.z(i)));
        }
    }
    const Vec hx = qp.H.selfadjointView<Eigen::Upper>() * s.x;
    Vec g = hx + qp.g;
    double scale = std::max({1.0, hx.lpNorm<Eigen::Infinity>(), qp.g.lpNorm<Eigen::Infinity>()});
    if (qp.me() > 0 && have_le) {
        const Vec t = qp.Ae.transpose() * s.lambda_e;
        g += t;
        scale = std::max(scale, t.lpNorm<Eigen::Infinity>());
    }
    if (qp.mi() > 0 && have_li) {
        const Vec t = qp.Ai.transpose() * s.lambda_i;
        g += t;
        scale = std::max(scale, t.lpNorm<Eigen::Infinity>());
    }
    double worst = 0.0;
    for (Index i = 0; i < n; ++i) {
        if (!pinned[static_cast<std::size_t>(i)]) {
            worst = nmax(worst, std::abs(g(i)));
        }
    }
    r.stationarity = worst / scale;
    r.dual_scale = std::max(scale, s.z.size() == n ? s.z.lpNorm<Eigen::Infinity>() : 0.0);
    return r;
}

int usage() {
    std::fprintf(stderr, "usage: hven_qp_dump_grid --file DUMP --algebra refactorize|border --csv OUT.csv\n");
    return 2;
}

} // namespace

int main(int argc, char **argv) {
    std::string file, algebra, csv;
    for (int i = 1; i + 1 < argc; i += 2) {
        const std::string k = argv[i];
        if (k == "--file") file = argv[i + 1];
        else if (k == "--algebra") algebra = argv[i + 1];
        else if (k == "--csv") csv = argv[i + 1];
        else return usage();
    }
    if (file.empty() || csv.empty() || (algebra != "refactorize" && algebra != "border")) {
        return usage();
    }
    const QpDump d = hven::test_support::load_qp_dump(file);
    hven::QpOptions o = d.opts;
    o.working_set_algebra = algebra == "border" ? hven::QpWorkingSetAlgebra::kSchurBorder
                                                : hven::QpWorkingSetAlgebra::kRefactorize;
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    const double f = hven::test_support::qp_dump_objective(d, s.x);
    const DataHalf dh = data_half(d, s);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);

    std::FILE *out = std::fopen(csv.c_str(), "a+");
    if (out == nullptr) {
        std::fprintf(stderr, "cannot open %s\n", csv.c_str());
        return 3;
    }
    std::fseek(out, 0, SEEK_END);
    if (std::ftell(out) == 0) {
        std::string h = "file,algebra,status,objective_hex,objective,x_fnv";
#define HVEN_QPC_HDR(name) h += "," #name;
        HVEN_QPC_GRID_R_FIELDS(HVEN_QPC_HDR)
#undef HVEN_QPC_HDR
        h += ",converged_refinement_arms,certificate_gate_refusals,d_rows,d_product,d_dual_scale,d_stationarity,"
             "k_eq,k_ineq,k_box,k_stationarity,k_stationarity_beyond_rounding,k_sign,k_compl,k_scale,"
             "feas_tol,opt_tol";
        std::fprintf(out, "%s\n", h.c_str());
    }
    const char *base = std::strrchr(file.c_str(), '/');
    std::string row = std::string(base ? base + 1 : file.c_str()) + "," + algebra + "," +
                      std::to_string(static_cast<int>(s.status)) + "," + hexd(f) + "," + decd(f) + ",";
    char hb[32];
    std::snprintf(hb, sizeof hb, "%016" PRIx64, x_hash(s.x));
    row += hb;
#define HVEN_QPC_VAL(name) row += "," + std::to_string(static_cast<long long>(s.counters.name));
    HVEN_QPC_GRID_R_FIELDS(HVEN_QPC_VAL)
#undef HVEN_QPC_VAL
    row += "," + opt_count(converged_refinement_arms_of(s.counters)) + "," +
           opt_count(certificate_gate_refusals_of(s.counters));
    for (const double v : {dh.rows, dh.product, dh.dual_scale, dh.stationarity, k.eq, k.ineq, k.box, k.stationarity,
                           k.stationarity_beyond_rounding, k.sign, k.complementarity, k.scale,
                           d.opts.feasibility_tol, d.opts.optimality_tol}) {
        row += "," + decd(v);
    }
    std::fprintf(out, "%s\n", row.c_str());
    std::fclose(out);
    return 0;
}
