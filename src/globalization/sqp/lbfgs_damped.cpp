// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// LbfgsDamped: damped compact L-BFGS, eigen form and mixed-lift data. The class contract and
// the formulas live in lbfgs_damped.h.

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <fmt/format.h>

#include <hven/detail/globalization/sqp/lbfgs_damped.h>

namespace hven::solvers::detail {

namespace {

[[noreturn]] void bad(const char *where, const char *field, const std::string &why) {
    throw std::invalid_argument(fmt::format("LbfgsDamped::{}: {} {}", where, field, why));
}

} // namespace

LbfgsDamped::LbfgsDamped(int n, double sigma0, LbfgsDampedOptions options)
    : n_(n), sigma0_(sigma0), sigma_(sigma0), opt_(options) {
    if (n < 1) {
        bad("LbfgsDamped", "n", fmt::format("({}) must be >= 1", n));
    }
    if (!std::isfinite(sigma0) || !(sigma0 > 0.0)) {
        bad("LbfgsDamped", "sigma0", fmt::format("({}) must be finite and > 0", sigma0));
    }
    if (opt_.memory < 1) {
        bad("LbfgsDamped", "options.memory", fmt::format("({}) must be >= 1", opt_.memory));
    }
    if (!std::isfinite(opt_.zero_step_scale) || opt_.zero_step_scale < 0.0) {
        bad("LbfgsDamped", "options.zero_step_scale",
            fmt::format("({}) must be finite and >= 0", opt_.zero_step_scale));
    }
    if (!(opt_.sigma_min > 0.0) || !(opt_.sigma_max >= opt_.sigma_min) ||
        !std::isfinite(opt_.sigma_max)) {
        bad("LbfgsDamped", "options.sigma_min/sigma_max",
            fmt::format("({}, {}) must satisfy 0 < min <= max < inf", opt_.sigma_min,
                        opt_.sigma_max));
    }
    if (!(opt_.truncation_tol >= 0.0) || !std::isfinite(opt_.truncation_tol)) {
        bad("LbfgsDamped", "options.truncation_tol",
            fmt::format("({}) must be finite and >= 0", opt_.truncation_tol));
    }
    rebuild();
}

void LbfgsDamped::set_sigma0(double sigma0) {
    if (!std::isfinite(sigma0) || !(sigma0 > 0.0)) {
        bad("set_sigma0", "sigma0", fmt::format("({}) must be finite and > 0", sigma0));
    }
    sigma0_ = sigma0;
    if (s_.empty()) {
        sigma_ = sigma0;
        rebuild();
    }
}

void LbfgsDamped::reset(ResetReason reason) {
    ++counters_.resets[static_cast<std::size_t>(reason)];
    s_.clear();
    r_.clear();
    sigma_ = sigma0_;
    rebuild();
}

Eigen::VectorXd LbfgsDamped::apply(const Eigen::VectorXd &v) const {
    if (v.size() != n_) {
        bad("apply", "v", fmt::format("has size {}, expected {}", v.size(), n_));
    }
    if (!v.allFinite()) {
        bad("apply", "v", "contains a non-finite entry");
    }
    Eigen::VectorXd out = sigma_ * v;
    if (u_.cols() > 0) {
        out.noalias() += u_ * theta_.cwiseProduct(u_.transpose() * v);
    }
    return out;
}

UpdateOutcome LbfgsDamped::update(const Eigen::VectorXd &s, const Eigen::VectorXd &y,
                                  double x_inf) {
    if (s.size() != n_) {
        bad("update", "s", fmt::format("has size {}, expected {}", s.size(), n_));
    }
    if (y.size() != n_) {
        bad("update", "y", fmt::format("has size {}, expected {}", y.size(), n_));
    }
    auto skip = [&](SkipReason why, UpdateOutcome outcome) {
        ++counters_.skipped[static_cast<std::size_t>(why)];
        return outcome;
    };
    if (!s.allFinite() || !y.allFinite() || !std::isfinite(x_inf)) {
        return skip(SkipReason::kNonFinite, UpdateOutcome::kSkippedNonFinite);
    }
    if (s.cwiseAbs().maxCoeff() <= opt_.zero_step_scale * std::max(1.0, std::abs(x_inf))) {
        return skip(SkipReason::kZeroStep, UpdateOutcome::kSkippedZeroStep);
    }
    const double ss = s.squaredNorm();
    const Eigen::VectorXd Bs = apply(s);
    const double sBs = s.dot(Bs);
    if (!(sBs > std::numeric_limits<double>::epsilon() * ss * sigma_)) {
        return skip(SkipReason::kCurvature, UpdateOutcome::kSkippedCurvature);
    }
    const double sy = s.dot(y);
    double theta = 1.0;
    if (sy < opt_.powell_fraction * sBs) {
        theta = opt_.powell_target * sBs / (sBs - sy);
    }
    Eigen::VectorXd r = y;
    if (theta != 1.0) {
        r = theta * y + (1.0 - theta) * Bs;
    }
    const double sr = s.dot(r);
    const double rr = r.squaredNorm();
    if (!std::isfinite(sr) || !std::isfinite(rr) || sr < opt_.curvature_floor * ss ||
        rr / sr > opt_.conditioning_cap) {
        return skip(SkipReason::kConditioning, UpdateOutcome::kSkippedConditioning);
    }
    s_.push_back(s);
    r_.push_back(std::move(r));
    if (static_cast<int>(s_.size()) > opt_.memory) {
        s_.erase(s_.begin());
        r_.erase(r_.begin());
    }
    sigma_ = std::clamp(rr / sr, opt_.sigma_min, opt_.sigma_max);
    ++counters_.updates;
    if (theta < 1.0) {
        ++counters_.damped;
    }
    rebuild();
    return UpdateOutcome::kUpdated;
}

bool LbfgsDamped::try_build() {
    const int p = static_cast<int>(s_.size());
    if (p == 0) {
        u_.resize(n_, 0);
        theta_.resize(0);
        return true;
    }
    const int k = 2 * p;
    if (k > n_) {
        return false; // W cannot have full column rank
    }
    Eigen::MatrixXd W(n_, k);
    for (int i = 0; i < p; ++i) {
        W.col(i) = sigma_ * s_[static_cast<std::size_t>(i)];
        W.col(p + i) = r_[static_cast<std::size_t>(i)];
    }
    Eigen::MatrixXd N = Eigen::MatrixXd::Zero(k, k);
    for (int i = 0; i < p; ++i) {
        for (int j = 0; j < p; ++j) {
            const double sr = s_[static_cast<std::size_t>(i)].dot(r_[static_cast<std::size_t>(j)]);
            if (i == j) {
                N(p + i, p + i) = -sr;
            } else if (i > j) {
                N(i, p + j) = sr;
                N(p + j, i) = sr;
            }
            N(i, j) = sigma_ * s_[static_cast<std::size_t>(i)].dot(s_[static_cast<std::size_t>(j)]);
        }
    }
    // Column normalisation: W Ninv W^T = Wt Nt^-1 Wt^T with Wt = W Dc^-1, Nt = Dc^-1 N Dc^-1.
    Eigen::VectorXd c = W.colwise().norm().transpose();
    if (!c.allFinite() || c.minCoeff() <= 0.0) {
        return false;
    }
    const Eigen::VectorXd cinv = c.cwiseInverse();
    W = W * cinv.asDiagonal();
    N = cinv.asDiagonal() * N * cinv.asDiagonal();

    Eigen::HouseholderQR<Eigen::MatrixXd> qr(W);
    const Eigen::MatrixXd R = qr.matrixQR().topRows(k).triangularView<Eigen::Upper>();
    for (int i = 0; i < k; ++i) {
        if (!(std::abs(R(i, i)) >= opt_.rank_tol)) {
            return false;
        }
    }
    Eigen::PartialPivLU<Eigen::MatrixXd> lu(N);
    const Eigen::MatrixXd Ninv_Rt = lu.solve(R.transpose());
    Eigen::MatrixXd G = -(R * Ninv_Rt);
    if (!G.allFinite()) {
        return false;
    }
    G = 0.5 * (G + G.transpose());
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(G);
    if (eig.info() != Eigen::Success || !eig.eigenvalues().allFinite()) {
        return false;
    }
    const Eigen::MatrixXd Q = qr.householderQ() * Eigen::MatrixXd::Identity(n_, k);
    const Eigen::MatrixXd U = Q * eig.eigenvectors();
    const double cut = opt_.truncation_tol * sigma_;
    std::vector<int> keep;
    for (int i = 0; i < k; ++i) { // ascending
        if (std::abs(eig.eigenvalues()(i)) > cut) {
            keep.push_back(i);
        }
    }
    u_.resize(n_, static_cast<Eigen::Index>(keep.size()));
    theta_.resize(static_cast<Eigen::Index>(keep.size()));
    for (std::size_t j = 0; j < keep.size(); ++j) {
        u_.col(static_cast<Eigen::Index>(j)) = U.col(keep[j]);
        theta_(static_cast<Eigen::Index>(j)) = eig.eigenvalues()(keep[j]);
    }
    return true;
}

void LbfgsDamped::rebuild() {
    while (!try_build()) {
        s_.erase(s_.begin());
        r_.erase(r_.begin());
        ++counters_.rank_dropped;
    }
    counters_.pairs_in_memory = static_cast<long>(s_.size());

    int np = 0;
    int nm = 0;
    for (Eigen::Index i = 0; i < theta_.size(); ++i) {
        (theta_(i) > 0.0 ? np : nm) += 1;
    }
    lift_.sigma = sigma_;
    lift_.u_plus.resize(n_, np);
    lift_.theta_plus.resize(np);
    lift_.v.resize(n_, nm);
    lift_.kappa_plus.resize(np);
    lift_.kappa_v.resize(nm);
    int ip = 0;
    int im = 0;
    for (Eigen::Index i = 0; i < theta_.size(); ++i) {
        if (theta_(i) > 0.0) {
            lift_.u_plus.col(ip) = u_.col(i);
            lift_.theta_plus(ip) = theta_(i);
            lift_.kappa_plus(ip) = 2.0 * u_.col(i).lpNorm<1>();
            ++ip;
        } else {
            lift_.v.col(im) = u_.col(i) * std::sqrt(-theta_(i));
            lift_.kappa_v(im) = 2.0 * lift_.v.col(im).lpNorm<1>();
            ++im;
        }
    }
}

double LbfgsDamped::min_eigenvalue() const {
    if (theta_.size() == 0) {
        return sigma_;
    }
    const double lo = sigma_ + theta_.minCoeff();
    return u_.cols() < n_ ? std::min(lo, sigma_) : lo;
}

double LbfgsDamped::max_eigenvalue() const {
    if (theta_.size() == 0) {
        return sigma_;
    }
    const double hi = sigma_ + theta_.maxCoeff();
    return u_.cols() < n_ ? std::max(hi, sigma_) : hi;
}

} // namespace hven::solvers::detail
