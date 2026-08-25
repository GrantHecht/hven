// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The executable form of the SQP driver's three step-recovery tiers: the
// second-order correction (detail/globalization/sqp/soc.h), the elastic l1
// exact-penalty tier (elastic.h), and the restoration phase's
// feasibility-model wrapper (restoration.h). Every contract, derivation and
// "why" stays in those headers.

// One include, deliberately not the three obvious ones: soc.h and restoration.h
// are NOT self-contained -- their declarations name `NlpEval`, which only
// drivers/sqp_driver.h defines -- and that header includes all three at exactly
// the point after NlpEval where they belong. Including them here directly does
// not work and must not be "restored" by a tidying pass or an
// include-what-you-use run: clang-format sorts `detail/globalization/...`
// ahead of `drivers/...`, which would put the three headers BEFORE the
// definition they depend on and fail to compile.
#include <hven/drivers/sqp_driver.h>

namespace hven::solvers {

QpProblem build_soc_subproblem(const QpProblem &qp, const NlpEval &ev, const NlpEval &ev_trial,
                               const Vec &p, const std::vector<bool> &ineq_active) {
    QpProblem soc_qp = qp;
    if (qp.me() > 0) {
        const Vec residual_e = ev_trial.ce - ev.ce - qp.Ae * p;
        soc_qp.be = -ev.ce - residual_e;
    }
    if (qp.mi() > 0) {
        if (static_cast<Index>(ineq_active.size()) != qp.mi()) {
            throw std::invalid_argument(fmt::format(
                "build_soc_subproblem: ineq_active has size {}, expected {} (= qp.mi())",
                ineq_active.size(), qp.mi()));
        }
        const Vec Ai_p = qp.Ai * p;
        Vec bi_soc = qp.bi; // inactive rows keep their rhs; only active rows get the correction
        for (Index j = 0; j < qp.mi(); ++j) {
            if (ineq_active[j]) {
                const double residual_i = ev_trial.ci(j) - ev.ci(j) - Ai_p(j);
                bi_soc(j) = -ev.ci(j) - residual_i;
            }
        }
        soc_qp.bi = std::move(bi_soc);
    }
    return soc_qp;
}

Vec ElasticQp::slacks(const Vec &x_aug) const {
    return x_aug.size() == qp.n() ? Vec(x_aug.tail(ns)) : Vec::Zero(ns);
}

Vec ElasticQp::slack_violations(const Vec &x_aug) const {
    return slacks(x_aug).cwiseProduct(slack_scale);
}

ElasticQp build_elastic_subproblem(const QpProblem &qp, double tr_radius, double rho, double tol) {
    const Index n = qp.n();
    const Index me = qp.me();
    const Index mi = qp.mi();

    ElasticQp e;
    e.n_orig = n;
    e.p_ref = Vec::Zero(n);

    Vec lo(n), up(n);
    const bool tr = std::isfinite(tr_radius);
    for (Index i = 0; i < n; ++i) {
        e.p_ref(i) = std::min(std::max(0.0, qp.lower(i)), qp.upper(i));
        lo(i) = tr ? std::max(qp.lower(i), e.p_ref(i) - tr_radius) : qp.lower(i);
        up(i) = tr ? std::min(qp.upper(i), e.p_ref(i) + tr_radius) : qp.upper(i);
    }

    // Which rows are violated at p_ref, and by how much.
    const Vec eq_res = me > 0 ? Vec(qp.be - qp.Ae * e.p_ref) : Vec(0);
    const Vec in_res = mi > 0 ? Vec(qp.Ai * e.p_ref - qp.bi) : Vec(0);
    e.eq_slack.assign(static_cast<std::size_t>(me), kNoSlack);
    e.ineq_slack.assign(static_cast<std::size_t>(mi), kNoSlack);
    std::vector<double> scales;
    Index ns = 0;
    for (Index k = 0; k < me; ++k) {
        if (std::abs(eq_res(k)) > tol) {
            e.eq_slack[static_cast<std::size_t>(k)] = n + ns++;
            e.violation_l1 += std::abs(eq_res(k));
            scales.push_back(std::max(1.0, std::abs(eq_res(k))));
        }
    }
    for (Index j = 0; j < mi; ++j) {
        if (in_res(j) > tol) {
            e.ineq_slack[static_cast<std::size_t>(j)] = n + ns++;
            e.violation_l1 += in_res(j);
            scales.push_back(std::max(1.0, in_res(j)));
        }
    }
    e.ns = ns;
    e.slack_scale = Eigen::Map<const Vec>(scales.data(), ns);
    const Index n2 = n + ns;

    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<std::size_t>(qp.H.nonZeros()));
    for (Index i = 0; i < n; ++i) {
        for (SpMatRM::InnerIterator it(qp.H, i); it; ++it) {
            t.emplace_back(it.row(), it.col(), it.value());
        }
    }
    e.qp.H = SpMatRM(n2, n2);
    e.qp.H.setFromTriplets(t.begin(), t.end());
    e.qp.H.makeCompressed();

    e.qp.g = Vec::Zero(n2);
    e.qp.g.head(n) = qp.g;
    e.qp.g.tail(ns) = rho * e.slack_scale; // rho * (the VIOLATION), see set_elastic_penalty

    t.clear();
    t.reserve(static_cast<std::size_t>(qp.Ae.nonZeros()) + static_cast<std::size_t>(me));
    for (Index k = 0; k < me; ++k) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(qp.Ae, k); it; ++it) {
            t.emplace_back(it.row(), it.col(), it.value());
        }
        const Index col = e.eq_slack[static_cast<std::size_t>(k)];
        if (col != kNoSlack) {
            const double sigma = e.slack_scale(col - n);
            t.emplace_back(k, col, eq_res(k) > 0.0 ? sigma : -sigma);
        }
    }
    e.qp.Ae = Eigen::SparseMatrix<double, Eigen::RowMajor>(me, n2);
    e.qp.Ae.setFromTriplets(t.begin(), t.end());
    e.qp.Ae.makeCompressed();
    e.qp.be = qp.be;

    t.clear();
    t.reserve(static_cast<std::size_t>(qp.Ai.nonZeros()) + static_cast<std::size_t>(mi));
    for (Index j = 0; j < mi; ++j) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(qp.Ai, j); it; ++it) {
            t.emplace_back(it.row(), it.col(), it.value());
        }
        const Index col = e.ineq_slack[static_cast<std::size_t>(j)];
        if (col != kNoSlack) {
            t.emplace_back(j, col, -e.slack_scale(col - n));
        }
    }
    e.qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(mi, n2);
    e.qp.Ai.setFromTriplets(t.begin(), t.end());
    e.qp.Ai.makeCompressed();
    e.qp.bi = qp.bi;

    e.qp.lower = Vec::Zero(n2);
    e.qp.upper = Vec::Zero(n2);
    e.qp.lower.head(n) = lo;
    e.qp.upper.head(n) = up;
    // THE SLACK CEILING: violation_l1 IN ACTUAL UNITS, i.e. violation_l1 /
    // sigma_j in the scaled variable, rather than +inf. The relaxation may not
    // leave the linearization MORE violated in total than it already is at
    // p_ref, and it cannot cut off the witness point, whose actual violation
    // |r_j| is at most sum_k |r_k| = violation_l1 by construction. A slack
    // that saturates it reports kAtUpper, which qp_engine.h's is_runaway skips
    // outright ("pinned at a bound: it did not run away").
    for (Index k = 0; k < ns; ++k) {
        e.qp.upper(n + k) = e.violation_l1 / e.slack_scale(k);
    }
    return e;
}

QpSolution elastic_seed(const ElasticQp &e, const QpSolution &failed) {
    const Index n2 = e.qp.n();
    QpSolution s;
    s.status = QpStatus::kOptimal;
    s.x = Vec::Zero(n2);
    s.bound_state.assign(static_cast<std::size_t>(n2), BoundState::kFree);
    if (static_cast<Index>(failed.bound_state.size()) == e.n_orig) {
        for (Index i = 0; i < e.n_orig; ++i) {
            s.bound_state[static_cast<std::size_t>(i)] =
                failed.bound_state[static_cast<std::size_t>(i)];
        }
    }
    s.ineq_active = static_cast<Index>(failed.ineq_active.size()) == e.qp.mi()
                        ? failed.ineq_active
                        : std::vector<bool>(static_cast<std::size_t>(e.qp.mi()), false);
    return s;
}

QpSolution elastic_project(const ElasticQp &e, const QpProblem &qp, const QpSolution &aug,
                           bool carry_multipliers) {
    const Index n = e.n_orig;
    QpSolution out;
    out.status = aug.status;
    out.counters = aug.counters;
    out.x = aug.x.size() >= n ? Vec(aug.x.head(n)) : Vec::Zero(n);
    // z IS QUARANTINED WITH THE MULTIPLIERS, not carried independently: a
    // bound price at index i is the stationarity residual there, so it is
    // contaminated by exactly the same rho the row multipliers are (see the
    // carry_multipliers handling of lambda_e/lambda_i below). SqpDriver never
    // reads QpSolution::z -- it reports the MODEL-implied bound multiplier
    // instead -- so this changes nothing today; it removes a trap for the
    // next reader.
    out.z = carry_multipliers && aug.z.size() >= n ? Vec(aug.z.head(n)) : Vec::Zero(n);
    out.lambda_e =
        carry_multipliers && aug.lambda_e.size() == qp.me() ? aug.lambda_e : Vec::Zero(qp.me());
    out.lambda_i =
        carry_multipliers && aug.lambda_i.size() == qp.mi() ? aug.lambda_i : Vec::Zero(qp.mi());
    out.ineq_active = static_cast<Index>(aug.ineq_active.size()) == qp.mi()
                          ? aug.ineq_active
                          : std::vector<bool>(static_cast<std::size_t>(qp.mi()), false);
    out.bound_state.assign(static_cast<std::size_t>(n), BoundState::kFree);
    out.tr_active.assign(static_cast<std::size_t>(n), false);
    if (static_cast<Index>(aug.bound_state.size()) < n) {
        return out;
    }
    for (Index i = 0; i < n; ++i) {
        const auto si = static_cast<std::size_t>(i);
        const BoundState st = aug.bound_state[si];
        const bool tr = (st == BoundState::kAtLower && e.qp.lower(i) > qp.lower(i)) ||
                        (st == BoundState::kAtUpper && e.qp.upper(i) < qp.upper(i));
        out.tr_active[si] = tr;
        out.bound_state[si] = tr ? BoundState::kFree : st;
        if (tr) {
            out.z(i) = 0.0;
        }
    }
    return out;
}

RestorationModel::RestorationModel(const NlpModel &model, const Vec &x_entry, const NlpEval &ev)
    : model_(model), nx_(model.n()), me_(model.me()), mi_(model.mi()) {
    if (x_entry.size() != nx_) {
        throw std::invalid_argument(
            fmt::format("RestorationModel: x_entry has size {}, expected {} (= model.n())",
                        x_entry.size(), nx_));
    }
    n_ = nx_ + 2 * me_ + mi_;

    sigma_e_ = Vec::Ones(me_);
    for (Index k = 0; k < me_; ++k) {
        double row_max = 0.0;
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(ev.Je, k); it; ++it) {
            row_max = std::max(row_max, std::abs(it.value()));
        }
        sigma_e_(k) = std::max(1.0, row_max);
    }
    sigma_i_ = Vec::Ones(mi_);
    for (Index j = 0; j < mi_; ++j) {
        double row_max = 0.0;
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(ev.Ji, j); it; ++it) {
            row_max = std::max(row_max, std::abs(it.value()));
        }
        sigma_i_(j) = std::max(1.0, row_max);
    }

    lower_ = Vec::Zero(n_);
    upper_ = Vec::Constant(n_, kRestorationSlackBound);
    lower_.head(nx_) = model.lower();
    upper_.head(nx_) = model.upper();

    // THE START POINT IS FEASIBLE FOR THIS WRAPPER BY CONSTRUCTION, with
    // objective exactly h(x_entry): put each row's whole violation into
    // the slack that absorbs it, in that slack's own units.
    start_ = Vec::Zero(n_);
    start_.head(nx_) = x_entry;
    for (Index k = 0; k < me_; ++k) {
        const double c = ev.ce(k);
        start_(nx_ + k) = std::max(0.0, -c) / sigma_e_(k);      // sp
        start_(nx_ + me_ + k) = std::max(0.0, c) / sigma_e_(k); // sm
    }
    for (Index j = 0; j < mi_; ++j) {
        start_(nx_ + 2 * me_ + j) = std::max(0.0, ev.ci(j)) / sigma_i_(j); // si
    }
}

Index RestorationModel::n() const { return n_; }

Index RestorationModel::me() const { return me_; }

Index RestorationModel::mi() const { return mi_; }

Vec RestorationModel::original_x(const Vec &y) const {
    return y.size() >= nx_ ? Vec(y.head(nx_)) : Vec::Zero(nx_);
}

const Vec &RestorationModel::slack_scale_e() const { return sigma_e_; }

const Vec &RestorationModel::slack_scale_i() const { return sigma_i_; }

double RestorationModel::eval_f(const Vec &y) const {
    double s = 0.0;
    for (Index k = 0; k < me_; ++k) {
        s += sigma_e_(k) * (y(nx_ + k) + y(nx_ + me_ + k));
    }
    for (Index j = 0; j < mi_; ++j) {
        s += sigma_i_(j) * y(nx_ + 2 * me_ + j);
    }
    return s;
}

Vec RestorationModel::eval_grad(const Vec &) const {
    Vec g = Vec::Zero(n_);
    g.segment(nx_, me_) = sigma_e_;
    g.segment(nx_ + me_, me_) = sigma_e_;
    g.segment(nx_ + 2 * me_, mi_) = sigma_i_;
    return g;
}

Vec RestorationModel::eval_ce(const Vec &y) const {
    if (me_ == 0) {
        return Vec(0);
    }
    Vec c = model_.eval_ce(original_x(y));
    for (Index k = 0; k < me_; ++k) {
        c(k) += sigma_e_(k) * (y(nx_ + k) - y(nx_ + me_ + k));
    }
    return c;
}

Vec RestorationModel::eval_ci(const Vec &y) const {
    if (mi_ == 0) {
        return Vec(0);
    }
    Vec c = model_.eval_ci(original_x(y));
    for (Index j = 0; j < mi_; ++j) {
        c(j) -= sigma_i_(j) * y(nx_ + 2 * me_ + j);
    }
    return c;
}

SpMatRM RestorationModel::eval_hess(const Vec &y, double, const Vec &lambda_e,
                                    const Vec &lambda_i) const {
    const SpMatRM inner = model_.eval_hess(original_x(y), 0.0, lambda_e, lambda_i);
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<std::size_t>(inner.nonZeros()));
    for (Index i = 0; i < nx_; ++i) {
        for (SpMatRM::InnerIterator it(inner, i); it; ++it) {
            t.emplace_back(it.row(), it.col(), it.value());
        }
    }
    SpMatRM H(n_, n_);
    H.setFromTriplets(t.begin(), t.end());
    H.makeCompressed();
    return H;
}

Eigen::SparseMatrix<double, Eigen::RowMajor> RestorationModel::eval_jac_e(const Vec &y) const {
    Eigen::SparseMatrix<double, Eigen::RowMajor> J(me_, n_);
    if (me_ == 0) {
        return J;
    }
    const Eigen::SparseMatrix<double, Eigen::RowMajor> Je = model_.eval_jac_e(original_x(y));
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<std::size_t>(Je.nonZeros() + 2 * me_));
    for (Index k = 0; k < me_; ++k) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(Je, k); it; ++it) {
            t.emplace_back(it.row(), it.col(), it.value());
        }
        t.emplace_back(k, nx_ + k, sigma_e_(k));
        t.emplace_back(k, nx_ + me_ + k, -sigma_e_(k));
    }
    J.setFromTriplets(t.begin(), t.end());
    J.makeCompressed();
    return J;
}

Eigen::SparseMatrix<double, Eigen::RowMajor> RestorationModel::eval_jac_i(const Vec &y) const {
    Eigen::SparseMatrix<double, Eigen::RowMajor> J(mi_, n_);
    if (mi_ == 0) {
        return J;
    }
    const Eigen::SparseMatrix<double, Eigen::RowMajor> Ji = model_.eval_jac_i(original_x(y));
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<std::size_t>(Ji.nonZeros() + mi_));
    for (Index j = 0; j < mi_; ++j) {
        for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(Ji, j); it; ++it) {
            t.emplace_back(it.row(), it.col(), it.value());
        }
        t.emplace_back(j, nx_ + 2 * me_ + j, -sigma_i_(j));
    }
    J.setFromTriplets(t.begin(), t.end());
    J.makeCompressed();
    return J;
}

const Vec &RestorationModel::lower() const { return lower_; }

const Vec &RestorationModel::upper() const { return upper_; }

Vec RestorationModel::start_point() const { return start_; }

} // namespace hven::solvers
