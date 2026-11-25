// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// qp_dump.h -- the loader for the M7 W2 E2-D4 QP dump corpus (format "D4QP 1", the contract in
// tests/qp/fixtures/e2-d4/FORMAT.txt) and for the library's own dumps (docs/qp-dump-format.md,
// whose optional `infinite_bound` line it reads; absent, the library default holds). The full
// 32-dump corpus is under docs/notes/data/2026-09-m7-w2-e2-d4-feasible-qps/dumps/. A dump is one
// walk solve as a cell made it: the QP, the engine's QpOptions, the solve's QpSolveOverrides, and
// the warm seed. The in-cell outcome line is kept verbatim (`result`). Replaying a dump on a fresh
// QpEngine reproduced the in-cell status and minors on every dump (E2-D4 REPORT).
//
// Also here: the KKT residuals of a returned point, recomputed from the QP (not from the
// engine's counters), against the EFFECTIVE box -- the real bounds intersected with the
// l_inf trust region |x - x0| <= tr about x0 = clamp(seed_x or 0, lower, upper) -- in the
// engine's sign convention (z = Hx + g + Ae'le + Ai'li at pinned columns, z > 0 at a lower
// bound, z < 0 at an upper one; li >= 0). At an index QpSolution::tr_active flags, z is exported
// as 0 and the stationarity residual there is the internal TR dual: it is read as that index's
// bound multiplier (its complementarity checked against the effective box) instead.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/walk/walk_engine.h>

namespace hven::test_support {

struct QpDump {
    hven::detail::QpProblem qp;
    hven::QpOptions opts;
    int threads = 0;
    hven::QpSolveOverrides overrides;
    bool has_seed = false;
    hven::detail::QpSolution seed;
    std::string result; // the in-cell outcome line, verbatim
    Vec x_final;        // the in-cell solve's final iterate
};

inline QpDump load_qp_dump(const std::string &path) {
    using SpRM = Eigen::SparseMatrix<double, Eigen::RowMajor>;
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("load_qp_dump: cannot open " + path);
    }
    QpDump d;
    Index n = 0, me = 0, mi = 0;
    std::map<std::string, Vec> vec;
    std::map<std::string, SpRM> mat;
    std::map<std::string, std::vector<long>> ivec;
    const auto num = [](const std::string &s) { return std::strtod(s.c_str(), nullptr); };
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream is(line);
        std::string key;
        is >> key;
        if (key.empty() || key == "D4QP") {
            continue;
        }
        if (key == "dims") {
            is >> n >> me >> mi;
            continue;
        }
        if (key == "infinite_bound") {
            std::string v;
            is >> v;
            d.qp.infinite_bound = num(v);
            continue;
        }
        if (key == "opts") {
            std::vector<std::string> t;
            std::string w;
            while (is >> w) {
                t.push_back(w);
            }
            if (t.size() != 11) {
                throw std::runtime_error("load_qp_dump: bad opts line in " + path);
            }
            d.opts.primal_regularization = num(t[0]);
            d.opts.dual_regularization = num(t[1]);
            d.opts.feasibility_tol = num(t[2]);
            d.opts.optimality_tol = num(t[3]);
            // The dump records the derived cap as 0 (or less): that is "empty" now.
            if (const long cap = std::stol(t[4]); cap > 0) {
                d.opts.max_iterations = cap;
            }
            d.opts.max_schur_border_dimension = std::stol(t[5]);
            d.opts.schur_condition_max = num(t[6]);
            d.opts.working_set_algebra = std::stoi(t[7]) == 0
                                             ? hven::QpWorkingSetAlgebra::kRefactorize
                                             : hven::QpWorkingSetAlgebra::kSchurBorder;
            d.opts.trust_region_radius = num(t[8]);
            d.opts.independent_working_set = std::stoi(t[9]) != 0;
            d.threads = std::stoi(t[10]);
            continue;
        }
        if (key == "overrides") {
            std::string a, b, c;
            is >> a >> b >> c;
            d.overrides.trust_region_radius = num(a);
            d.overrides.primal_regularization = num(b);
            d.overrides.dual_regularization = num(c);
            continue;
        }
        if (key == "warm") {
            int warm = 0, seed = 0;
            std::string tag;
            is >> warm >> tag >> seed;
            d.has_seed = seed != 0;
            continue;
        }
        if (key == "result") {
            d.result = line;
            // hkc = QpOptions::hessian_known_convex as the solve ran (absent: false).
            std::string tok;
            while (is >> tok) {
                if (tok.rfind("hkc=", 0) == 0) {
                    d.opts.hessian_known_convex = tok == "hkc=1";
                }
            }
            continue;
        }
        std::vector<long> nums;
        long v = 0;
        while (is >> v) {
            nums.push_back(v);
        }
        const bool is_int = key.rfind("seed_bound", 0) == 0 || key.rfind("seed_ineq", 0) == 0 ||
                            key.rfind("final_", 0) == 0;
        if (nums.size() == 1 && is_int) {
            std::vector<long> iv(static_cast<std::size_t>(nums[0]));
            for (long &x : iv) {
                std::getline(f, line);
                x = std::stol(line);
            }
            ivec[key] = std::move(iv);
        } else if (nums.size() == 1) {
            Vec x(nums[0]);
            for (Index i = 0; i < nums[0]; ++i) {
                std::getline(f, line);
                x(i) = num(line);
            }
            vec[key] = std::move(x);
        } else if (nums.size() == 3) {
            std::vector<Eigen::Triplet<double>> t;
            t.reserve(static_cast<std::size_t>(nums[2]));
            for (long k = 0; k < nums[2]; ++k) {
                std::getline(f, line);
                std::istringstream ls(line);
                long r = 0, c = 0;
                std::string sv;
                ls >> r >> c >> sv;
                t.emplace_back(static_cast<int>(r), static_cast<int>(c), num(sv));
            }
            SpRM A(nums[0], nums[1]);
            A.setFromTriplets(t.begin(), t.end());
            mat[key] = std::move(A);
        } else {
            throw std::runtime_error("load_qp_dump: bad line '" + line + "' in " + path);
        }
    }
    d.qp.H = mat["H"];
    d.qp.g = vec["g"];
    d.qp.Ae = mat["Ae"];
    d.qp.be = vec["be"];
    d.qp.Ai = mat["Ai"];
    d.qp.bi = vec["bi"];
    d.qp.lower = vec["lower"];
    d.qp.upper = vec["upper"];
    d.x_final = vec["x_final"];
    if (d.qp.Ae.rows() != me) {
        d.qp.Ae.resize(me, n);
    }
    if (d.qp.Ai.rows() != mi) {
        d.qp.Ai.resize(mi, n);
    }
    if (d.qp.n() != n || d.qp.g.size() != n || d.qp.be.size() != me || d.qp.bi.size() != mi) {
        throw std::runtime_error("load_qp_dump: inconsistent dimensions in " + path);
    }
    if (d.has_seed) {
        d.seed.x = vec["seed_x"];
        for (const long b : ivec["seed_bound_state"]) {
            d.seed.bound_state.push_back(static_cast<hven::BoundState>(b));
        }
        for (const long a : ivec["seed_ineq_active"]) {
            d.seed.ineq_active.push_back(a != 0);
        }
    }
    return d;
}

/// Re-solve the dump on a fresh engine, with the dumped options except those the caller changed.
inline hven::detail::QpSolution solve_qp_dump(const QpDump &d, const hven::QpOptions &opts) {
    const hven::detail::QpEngine engine(opts, d.threads, hven::linear::DensePolicy::kNever);
    return d.has_seed ? engine.solve(d.qp, d.seed, d.overrides) : engine.solve(d.qp, d.overrides);
}

inline double qp_dump_objective(const QpDump &d, const Vec &x) {
    return d.qp.g.dot(x) + 0.5 * x.dot(d.qp.H.selfadjointView<Eigen::Upper>() * x);
}

/// The KKT residuals of `s` on the dump's QP, recomputed. Primal terms are absolute (the
/// largest violation); `stationarity` is ||Hx + g + Ae'le + Ai'li - z||_inf over the largest
/// term in it (scale >= 1); `sign` is the largest wrong-signed multiplier and `complementarity`
/// the largest |multiplier * slack| on a constraint the point is not on, over the same scale.
///
/// `stationarity_beyond_rounding` is the same residual with each component's ROUNDING FLOOR taken
/// out first: component i is a sum of c_i terms (g_i, z_i and the nonzeros of column i of H, Ae and
/// Ai) whose magnitudes add to m_i, so evaluating it in double -- and rounding the multipliers it
/// is formed from -- can leave c_i * u * m_i even at an exact KKT point (Higham, the gamma_n dot
/// product bound, unit roundoff u = eps / 2). It reads max_i max(0, |r_i| - c_i * u * m_i) /
/// scale. Where the terms do not cancel it equals `stationarity` to within c_i * u; where huge
/// terms cancel to a small sum the normwise figure can sit below what double precision can resolve
/// (C11 SAWPATH: terms of 2.4e13 cancelling to 3.6e2), and only this one is a check a correct
/// answer passes on every build.
struct QpDumpKkt {
    double eq = 0.0;
    double ineq = 0.0;
    double box = 0.0;
    double stationarity = 0.0;
    double stationarity_beyond_rounding = 0.0;
    double sign = 0.0;
    double complementarity = 0.0;
    double scale = 1.0;
};

inline QpDumpKkt qp_dump_kkt(const QpDump &d, const hven::detail::QpSolution &s) {
    const hven::detail::QpProblem &qp = d.qp;
    const Index n = qp.n();
    QpDumpKkt k;
    Vec lo = qp.lower;
    Vec up = qp.upper;
    const double tr = std::isfinite(d.overrides.trust_region_radius)
                          ? d.overrides.trust_region_radius
                          : d.opts.trust_region_radius;
    if (std::isfinite(tr)) {
        Vec x0 = d.has_seed ? d.seed.x : Vec::Zero(n);
        for (Index i = 0; i < n; ++i) {
            x0(i) = std::clamp(x0(i), qp.lower(i), qp.upper(i));
            lo(i) = std::max(lo(i), x0(i) - tr);
            up(i) = std::min(up(i), x0(i) + tr);
        }
    }
    if (qp.me() > 0) {
        k.eq = (qp.Ae * s.x - qp.be).lpNorm<Eigen::Infinity>();
    }
    Vec slack = Vec::Zero(qp.mi());
    if (qp.mi() > 0) {
        slack = qp.bi - qp.Ai * s.x;
        k.ineq = std::max(0.0, -slack.minCoeff());
    }
    for (Index i = 0; i < n; ++i) {
        k.box = std::max({k.box, lo(i) - s.x(i), s.x(i) - up(i)});
    }
    const Vec hx = qp.H.selfadjointView<Eigen::Upper>() * s.x;
    Vec r = hx + qp.g - s.z;
    k.scale = std::max({1.0, hx.lpNorm<Eigen::Infinity>(), qp.g.lpNorm<Eigen::Infinity>(),
                        s.z.lpNorm<Eigen::Infinity>()});
    if (qp.me() > 0 && s.lambda_e.size() == qp.me()) {
        const Vec t = qp.Ae.transpose() * s.lambda_e;
        r += t;
        k.scale = std::max(k.scale, t.lpNorm<Eigen::Infinity>());
    }
    if (qp.mi() > 0 && s.lambda_i.size() == qp.mi()) {
        const Vec t = qp.Ai.transpose() * s.lambda_i;
        r += t;
        k.scale = std::max(k.scale, t.lpNorm<Eigen::Infinity>());
        for (Index j = 0; j < qp.mi(); ++j) {
            k.sign = std::max(k.sign, -s.lambda_i(j));
            k.complementarity = std::max(k.complementarity, std::abs(s.lambda_i(j) * slack(j)));
        }
    }
    // At a TR-held index z(i) is exported as 0 (QpSolution::tr_active): the residual there IS
    // the internal TR dual, so it is read as the bound multiplier and taken out of r.
    const auto tr_held = [&](Index i) {
        return static_cast<std::size_t>(i) < s.tr_active.size() &&
               s.tr_active[static_cast<std::size_t>(i)];
    };
    for (Index i = 0; i < n; ++i) {
        if (tr_held(i)) {
            const double zi = r(i);
            r(i) = 0.0;
            k.complementarity =
                std::max(k.complementarity,
                         zi > 0.0 ? zi * std::abs(s.x(i) - lo(i)) : -zi * std::abs(up(i) - s.x(i)));
        }
    }
    k.stationarity = r.lpNorm<Eigen::Infinity>() / k.scale;
    {
        // The rounding floor of each component of r (see QpDumpKkt): term magnitudes m and term
        // counts c, column by column.
        const SpMatRM h_abs = qp.H.cwiseAbs();
        const SpMatRM h_pat = h_abs.cwiseSign();
        const Vec ones_n = Vec::Ones(n);
        Vec m = h_abs.selfadjointView<Eigen::Upper>() * s.x.cwiseAbs() + qp.g.cwiseAbs() +
                s.z.cwiseAbs();
        Vec c = h_pat.selfadjointView<Eigen::Upper>() * ones_n + Vec::Constant(n, 2.0);
        if (qp.me() > 0 && s.lambda_e.size() == qp.me()) {
            const SpMatRM a_abs = qp.Ae.cwiseAbs();
            m += a_abs.transpose() * s.lambda_e.cwiseAbs();
            c += SpMatRM(a_abs.cwiseSign()).transpose() * Vec::Ones(qp.me());
        }
        if (qp.mi() > 0 && s.lambda_i.size() == qp.mi()) {
            const SpMatRM a_abs = qp.Ai.cwiseAbs();
            m += a_abs.transpose() * s.lambda_i.cwiseAbs();
            c += SpMatRM(a_abs.cwiseSign()).transpose() * Vec::Ones(qp.mi());
        }
        constexpr double kUnitRoundoff = 0.5 * std::numeric_limits<double>::epsilon();
        double beyond = 0.0;
        for (Index i = 0; i < n; ++i) {
            const double excess = std::abs(r(i)) - c(i) * kUnitRoundoff * m(i);
            // NaN-sticky: a NaN residual is never "within rounding".
            if (std::isnan(excess)) {
                beyond = excess;
                break;
            }
            beyond = std::max(beyond, excess);
        }
        k.stationarity_beyond_rounding = beyond / k.scale;
    }
    for (Index i = 0; i < n; ++i) {
        const double zi = s.z(i);
        // z > 0 belongs at the lower bound, z < 0 at the upper one.
        if (zi > 0.0) {
            k.complementarity = std::max(k.complementarity, zi * std::abs(s.x(i) - lo(i)));
        } else if (zi < 0.0) {
            k.complementarity = std::max(k.complementarity, -zi * std::abs(up(i) - s.x(i)));
        }
    }
    k.sign /= k.scale;
    k.complementarity /= k.scale;
    return k;
}

} // namespace hven::test_support
