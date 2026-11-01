// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MixedLift: the quasi-Newton mixed-lift QP adapter. The formulation, the scalings and the
// reasons for them live in mixed_lift.h.

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include <fmt/format.h>

#include <hven/detail/globalization/sqp/mixed_lift.h>

namespace hven::solvers::detail {

namespace {

using Triplet = Eigen::Triplet<double>;

[[noreturn]] void bad(const std::string &why) {
    throw std::invalid_argument(fmt::format("MixedLift: {}", why));
}

void check_scales(const char *what, const Eigen::MatrixXd &cols, const Vec &kappa, Index n_struct) {
    if (cols.cols() > 0 && cols.rows() != n_struct) {
        bad(fmt::format("{} has {} rows, expected n_struct = {}", what, cols.rows(), n_struct));
    }
    if (kappa.size() != cols.cols()) {
        bad(fmt::format("kappa for {} has size {}, expected {}", what, kappa.size(), cols.cols()));
    }
    for (Index k = 0; k < cols.cols(); ++k) {
        const double need = 2.0 * cols.col(k).lpNorm<1>();
        if (!cols.col(k).allFinite() || !std::isfinite(kappa(k)) || !(kappa(k) > 0.0) ||
            kappa(k) < need * (1.0 - 1e-12)) {
            bad(fmt::format("kappa for {} column {} is {}, must be finite and >= 2*||column||_1 = "
                            "{} (BLOCKING-1: the trust window must never bind an auxiliary)",
                            what, k, kappa(k), need));
        }
    }
}

} // namespace

MixedLift::MixedLift(const QpProblem &qp, Index n_struct, const MixedLiftData &d) {
    qp.validate();
    n_orig_ = qp.n();
    me_orig_ = qp.me();
    n_struct_ = n_struct;
    if (n_struct < 0 || n_struct > n_orig_) {
        bad(fmt::format("n_struct = {} outside [0, {}]", n_struct, n_orig_));
    }
    if (!std::isfinite(d.sigma) || !(d.sigma > 0.0)) {
        bad(fmt::format("sigma = {} must be finite and > 0", d.sigma));
    }
    if (d.theta_plus.size() != d.u_plus.cols()) {
        bad(fmt::format("theta_plus has size {}, u_plus has {} columns", d.theta_plus.size(),
                        d.u_plus.cols()));
    }
    for (Index k = 0; k < d.theta_plus.size(); ++k) {
        if (!std::isfinite(d.theta_plus(k)) || !(d.theta_plus(k) > 0.0)) {
            bad(fmt::format("theta_plus({}) = {} must be finite and > 0", k, d.theta_plus(k)));
        }
    }
    check_scales("u_plus", d.u_plus, d.kappa_plus, n_struct);
    check_scales("v", d.v, d.kappa_v, n_struct);
    if (n_struct == 0 && (d.u_plus.cols() > 0 || d.v.cols() > 0)) {
        bad("auxiliary columns with n_struct = 0");
    }
    // THE SCALAR PART'S REPLACEMENTS (lbfgs_damped.h MixedLiftData).
    const bool has_diag = d.diag.size() > 0;
    const bool has_base = d.base.rows() > 0 || d.base.cols() > 0;
    if (has_diag && (d.diag.size() != n_struct || !d.diag.allFinite() || d.diag.minCoeff() < 0.0)) {
        bad(fmt::format("diag has size {} (expected n_struct = {}) or a negative / non-finite "
                        "entry",
                        d.diag.size(), n_struct));
    }
    if (has_base) {
        if (!has_diag) {
            bad("base without diag");
        }
        if (d.base.rows() != n_struct || d.base.cols() != n_struct) {
            bad(fmt::format("base is {}x{}, expected {}x{}", d.base.rows(), d.base.cols(), n_struct,
                            n_struct));
        }
        for (Index r = 0; r < d.base.outerSize(); ++r) {
            for (SpMatRM::InnerIterator e(d.base, r); e; ++e) {
                if (e.col() < e.row() || !std::isfinite(e.value())) {
                    bad(fmt::format("base entry ({}, {}) is below the diagonal or non-finite",
                                    e.row(), e.col()));
                }
            }
        }
    }
    std::vector<Index> rows;
    if (d.support.empty()) {
        rows.resize(static_cast<std::size_t>(n_struct));
        for (Index i = 0; i < n_struct; ++i) {
            rows[static_cast<std::size_t>(i)] = i;
        }
    } else {
        std::vector<char> in(static_cast<std::size_t>(n_struct), 0);
        for (std::size_t k = 0; k < d.support.size(); ++k) {
            const Index i = d.support[k];
            if (i < 0 || i >= n_struct || (k > 0 && i <= d.support[k - 1])) {
                bad(fmt::format("support entry {} = {} is out of [0, {}) or not ascending", k, i,
                                n_struct));
            }
            in[static_cast<std::size_t>(i)] = 1;
        }
        // Rows outside the support must be zero in u_plus and v: they are never emitted.
        for (Index i = 0; i < n_struct; ++i) {
            if (in[static_cast<std::size_t>(i)] == 0 &&
                ((d.u_plus.cols() > 0 && d.u_plus.row(i).cwiseAbs().maxCoeff() != 0.0) ||
                 (d.v.cols() > 0 && d.v.row(i).cwiseAbs().maxCoeff() != 0.0))) {
                bad(fmt::format("row {} is outside the support but nonzero in u_plus or v", i));
            }
        }
        rows = d.support;
    }

    u_plus_ = d.u_plus;
    v_ = d.v;
    kappa_plus_ = d.kappa_plus;
    kappa_v_ = d.kappa_v;
    r_plus_ = d.u_plus.cols();
    r_minus_ = d.v.cols();
    const Index n = n_orig_;
    const Index nl = n + r_plus_ + r_minus_;
    const Index me = me_orig_;
    const Index mi = qp.mi();

    p_ref_ = qp.lower.cwiseMax(Vec::Zero(n)).cwiseMin(qp.upper);
    // A crossed box (lower > upper) is a legitimate infeasible QP the engine answers itself;
    // the cwiseMin above then returns upper. Only the reference is affected, and it stays finite.
    const Vec p_ref_s = p_ref_.head(n_struct);
    const Vec c = (r_plus_ > 0) ? Vec(u_plus_.transpose() * p_ref_s) : Vec(0);
    const Vec a = (r_minus_ > 0) ? Vec(v_.transpose() * p_ref_s) : Vec(0);

    std::vector<Triplet> t;
    const auto n_rows = static_cast<Index>(rows.size());
    t.reserve(static_cast<std::size_t>(n_struct + d.base.nonZeros() + r_plus_ + r_minus_ +
                                       n_rows * r_minus_));
    for (Index i = 0; i < n_struct; ++i) {
        const double s_i = has_diag ? d.diag(i) : d.sigma;
        if (s_i != 0.0) {
            t.emplace_back(i, i, s_i);
        }
    }
    if (has_base) {
        for (Index r = 0; r < d.base.outerSize(); ++r) {
            for (SpMatRM::InnerIterator e(d.base, r); e; ++e) {
                t.emplace_back(e.row(), e.col(), e.value());
            }
        }
    }
    for (Index k = 0; k < r_plus_; ++k) {
        t.emplace_back(n + k, n + k, d.theta_plus(k) * kappa_plus_(k) * kappa_plus_(k));
    }
    for (Index j = 0; j < r_minus_; ++j) {
        const Index col = n + r_plus_ + j;
        t.emplace_back(col, col, kappa_v_(j) * kappa_v_(j));
        for (const Index i : rows) { // structurally dense on the support (AMEND-8)
            t.emplace_back(i, col, -kappa_v_(j) * v_(i, j));
        }
    }
    lifted_.H = SpMatRM(nl, nl);
    lifted_.H.setFromTriplets(t.begin(), t.end());
    lifted_.H.makeCompressed();

    lifted_.g = Vec::Zero(nl);
    lifted_.g.head(n) = qp.g;
    if (r_minus_ > 0) {
        va_ = v_ * a;
        lifted_.g.head(n_struct) -= va_;
        lifted_.g.tail(r_minus_) = kappa_v_.cwiseProduct(a);
    }
    for (Index k = 0; k < r_plus_; ++k) {
        lifted_.g(n + k) = d.theta_plus(k) * kappa_plus_(k) * c(k);
    }

    t.clear();
    for (Index k = 0; k < me; ++k) {
        for (SpMatRM::InnerIterator it(qp.Ae, k); it; ++it) {
            t.emplace_back(k, it.col(), it.value());
        }
    }
    for (Index k = 0; k < r_plus_; ++k) {
        for (const Index i : rows) { // structurally dense on the support
            t.emplace_back(me + k, i, -u_plus_(i, k));
        }
        t.emplace_back(me + k, n + k, kappa_plus_(k));
    }
    lifted_.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(me + r_plus_, nl);
    lifted_.Ae.setFromTriplets(t.begin(), t.end());
    lifted_.Ae.makeCompressed();
    lifted_.be = Vec::Zero(me + r_plus_);
    lifted_.be.head(me) = qp.be;
    lifted_.be.tail(r_plus_) = -c;

    t.clear();
    for (Index j = 0; j < mi; ++j) {
        for (SpMatRM::InnerIterator it(qp.Ai, j); it; ++it) {
            t.emplace_back(j, it.col(), it.value());
        }
    }
    lifted_.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(mi, nl);
    lifted_.Ai.setFromTriplets(t.begin(), t.end());
    lifted_.Ai.makeCompressed();
    lifted_.bi = qp.bi;

    constexpr double kInf = std::numeric_limits<double>::infinity();
    lifted_.lower = Vec::Constant(nl, -kInf);
    lifted_.upper = Vec::Constant(nl, kInf);
    lifted_.lower.head(n) = qp.lower;
    lifted_.upper.head(n) = qp.upper;
    lifted_.validate();
}

Vec MixedLift::lift_primal(const Vec &p) const {
    if (p.size() != n_orig_) {
        bad(fmt::format("lift_primal: p has size {}, expected {}", p.size(), n_orig_));
    }
    const Vec pc = p.cwiseMax(lifted_.lower.head(n_orig_)).cwiseMin(lifted_.upper.head(n_orig_));
    Vec x(n_lifted());
    x.head(n_orig_) = pc;
    const Vec dp = (pc - p_ref_).head(n_struct_);
    if (r_plus_ > 0) {
        x.segment(n_orig_, r_plus_) = (u_plus_.transpose() * dp).cwiseQuotient(kappa_plus_);
    }
    if (r_minus_ > 0) {
        x.tail(r_minus_) = (v_.transpose() * dp).cwiseQuotient(kappa_v_);
    }
    return x;
}

QpSolution MixedLift::lift_seed(const QpSolution &seed) const {
    const Index nl = n_lifted();
    const auto sized = [&](Index have, Index want, const char *what) {
        if (have != 0 && have != want) {
            bad(fmt::format("lift_seed: {} has size {}, expected {} or empty", what, have, want));
        }
        return have != 0;
    };
    QpSolution out = seed;
    out.farkas = QpFarkasCertificate{};
    if (sized(seed.x.size(), n_orig_, "x")) {
        out.x = lift_primal(seed.x);
    }
    if (sized(seed.z.size(), n_orig_, "z")) {
        out.z = Vec::Zero(nl);
        out.z.head(n_orig_) = seed.z;
    }
    if (sized(seed.lambda_e.size(), me_orig_, "lambda_e")) {
        out.lambda_e = Vec::Zero(me_orig_ + r_plus_);
        out.lambda_e.head(me_orig_) = seed.lambda_e;
    }
    if (sized(static_cast<Index>(seed.bound_state.size()), n_orig_, "bound_state")) {
        out.bound_state.resize(static_cast<std::size_t>(nl), BoundState::kFree);
    }
    if (sized(static_cast<Index>(seed.tr_active.size()), n_orig_, "tr_active")) {
        out.tr_active.resize(static_cast<std::size_t>(nl), false);
    }
    return out;
}

QpSolution MixedLift::unlift(const QpSolution &l) const {
    const Index nl = n_lifted();
    const auto need = [&](Index have, Index want, const char *what) {
        if (have != want) {
            bad(fmt::format("unlift: {} has size {}, expected {}", what, have, want));
        }
    };
    need(l.x.size(), nl, "x");
    need(l.z.size(), nl, "z");
    need(l.lambda_e.size(), me_orig_ + r_plus_, "lambda_e");
    need(static_cast<Index>(l.bound_state.size()), nl, "bound_state");
    need(static_cast<Index>(l.tr_active.size()), nl, "tr_active");

    QpSolution out;
    out.status = l.status;
    out.x = l.x.head(n_orig_);
    out.z = l.z.head(n_orig_);
    out.lambda_e = l.lambda_e.head(me_orig_);
    out.lambda_i = l.lambda_i;
    out.ineq_active = l.ineq_active;
    out.counters = l.counters;
    out.bound_state.assign(l.bound_state.begin(), l.bound_state.begin() + n_orig_);
    out.tr_active.assign(l.tr_active.begin(), l.tr_active.begin() + n_orig_);
    out.farkas = l.farkas;
    if (l.farkas.present) {
        if (!l.farkas.blocker_is_ineq && l.farkas.blocker >= n_orig_) {
            out.farkas = QpFarkasCertificate{}; // an auxiliary blocker: not expressible
        } else {
            if (l.farkas.y_e.size() == me_orig_ + r_plus_) {
                out.farkas.y_e = l.farkas.y_e.head(me_orig_);
            }
            if (l.farkas.box_part.size() == nl) {
                out.farkas.box_part = l.farkas.box_part.head(n_orig_);
            }
        }
    }
    return out;
}

Vec MixedLift::unlift_primal(const Vec &x) const {
    if (x.size() != n_lifted()) {
        bad(fmt::format("unlift_primal: size {}, expected {}", x.size(), n_lifted()));
    }
    return x.head(n_orig_);
}

QpProblem MixedLift::lift_variant(const QpProblem &variant) const {
    if (variant.n() != n_orig_ || variant.me() != me_orig_ || variant.mi() != lifted_.mi() ||
        variant.g.size() != n_orig_ || variant.be.size() != me_orig_ ||
        variant.bi.size() != lifted_.mi()) {
        bad(fmt::format("lift_variant: variant is {}x({}, {}), the lift's input is {}x({}, {})",
                        variant.n(), variant.me(), variant.mi(), n_orig_, me_orig_, lifted_.mi()));
    }
    QpProblem out = lifted_;
    out.g.head(n_orig_) = variant.g;
    if (r_minus_ > 0) {
        out.g.head(n_struct_) -= va_;
    }
    out.be.head(me_orig_) = variant.be;
    out.bi = variant.bi;
    return out;
}

Vec MixedLift::unlift_equality_multipliers(const Vec &lambda_e) const {
    if (lambda_e.size() != me_orig_ + r_plus_) {
        bad(fmt::format("unlift_equality_multipliers: size {}, expected {}", lambda_e.size(),
                        me_orig_ + r_plus_));
    }
    return lambda_e.head(me_orig_);
}

} // namespace hven::solvers::detail
