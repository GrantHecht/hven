// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// DenseDampedBfgs and MaskedQnModel: the class contracts and the references live in
// qn_models.h.

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include <fmt/format.h>

#include <hven/detail/globalization/sqp/qn_models.h>

namespace hven::solvers::detail {

namespace {

[[noreturn]] void bad(const char *cls, const char *where, const std::string &why) {
    throw std::invalid_argument(fmt::format("{}::{}: {}", cls, where, why));
}

} // namespace

// ---- DenseDampedBfgs ---------------------------------------------------------------------

DenseDampedBfgs::DenseDampedBfgs(int d, double sigma0, LbfgsDampedOptions options)
    : d_(d), sigma0_(sigma0), sigma_(sigma0), opt_(options) {
    if (d < 1) {
        bad("DenseDampedBfgs", "DenseDampedBfgs", fmt::format("d ({}) must be >= 1", d));
    }
    if (!std::isfinite(sigma0) || !(sigma0 > 0.0)) {
        bad("DenseDampedBfgs", "DenseDampedBfgs",
            fmt::format("sigma0 ({}) must be finite and > 0", sigma0));
    }
    if (!(opt_.sigma_min > 0.0) || !(opt_.sigma_max >= opt_.sigma_min) ||
        !std::isfinite(opt_.sigma_max)) {
        bad("DenseDampedBfgs", "DenseDampedBfgs",
            fmt::format("options.sigma_min/sigma_max ({}, {}) must satisfy 0 < min <= max < inf",
                        opt_.sigma_min, opt_.sigma_max));
    }
    b_ = sigma0_ * Eigen::MatrixXd::Identity(d_, d_);
    publish();
}

void DenseDampedBfgs::reset(ResetReason reason) {
    ++counters_.resets[static_cast<std::size_t>(reason)];
    sigma_ = sigma0_;
    b_ = sigma0_ * Eigen::MatrixXd::Identity(d_, d_);
    pairs_ = 0;
    pd_ = true;
    counters_.pairs_in_memory = 0;
    publish();
}

Eigen::VectorXd DenseDampedBfgs::apply(const Eigen::VectorXd &v) const {
    if (v.size() != d_) {
        bad("DenseDampedBfgs", "apply", fmt::format("v has size {}, expected {}", v.size(), d_));
    }
    if (!v.allFinite()) {
        bad("DenseDampedBfgs", "apply", "v contains a non-finite entry");
    }
    return b_ * v;
}

UpdateOutcome DenseDampedBfgs::update(const Eigen::VectorXd &s, const Eigen::VectorXd &y,
                                      double x_inf) {
    if (s.size() != d_ || y.size() != d_) {
        bad("DenseDampedBfgs", "update",
            fmt::format("s/y have sizes {}/{}, expected {}", s.size(), y.size(), d_));
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
    const double sy = s.dot(y);
    // THE INITIAL SCALING (Nocedal & Wright eq. 6.20, in B form): the first accepted pair
    // re-bases B0 before it is applied. Kept only if the pair is.
    Eigen::MatrixXd b = b_;
    double sigma = sigma_;
    if (pairs_ == 0 && sy > 0.0) {
        sigma = std::clamp(y.squaredNorm() / sy, opt_.sigma_min, opt_.sigma_max);
        b = sigma * Eigen::MatrixXd::Identity(d_, d_);
    } else if (opt_.self_scaling && pairs_ > 0 && sy > 0.0) {
        const double sBs0 = s.dot(b * s);
        const double tau = sBs0 > 0.0 ? sy / sBs0 : 1.0;
        if (tau > 0.0 && tau < 1.0) {
            b *= tau;
        }
    }
    const Eigen::VectorXd Bs = b * s;
    const double sBs = s.dot(Bs);
    if (!(sBs > std::numeric_limits<double>::epsilon() * ss * sigma)) {
        return skip(SkipReason::kCurvature, UpdateOutcome::kSkippedCurvature);
    }
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
    b.noalias() -= (Bs / sBs) * Bs.transpose();
    b.noalias() += (r / sr) * r.transpose();
    b = 0.5 * (b + b.transpose()).eval();
    if (!b.allFinite() || Eigen::LLT<Eigen::MatrixXd>(b).info() != Eigen::Success) {
        return skip(SkipReason::kConditioning, UpdateOutcome::kSkippedConditioning);
    }
    b_ = std::move(b);
    sigma_ = sigma;
    pd_ = true;
    ++pairs_;
    ++counters_.updates;
    if (theta < 1.0) {
        ++counters_.damped;
    }
    counters_.pairs_in_memory = pairs_;
    publish();
    return UpdateOutcome::kUpdated;
}

void DenseDampedBfgs::publish() {
    lift_ = MixedLiftData{};
    lift_.sigma = sigma_;
    lift_.diag = Eigen::VectorXd::Zero(d_);
    std::vector<Eigen::Triplet<double>> t;
    t.reserve(static_cast<std::size_t>(d_) * static_cast<std::size_t>(d_ + 1) / 2);
    for (int i = 0; i < d_; ++i) {
        for (int j = i; j < d_; ++j) {
            t.emplace_back(i, j, b_(i, j));
        }
    }
    lift_.base.resize(d_, d_);
    lift_.base.setFromTriplets(t.begin(), t.end());
    lift_.base.makeCompressed();
}

// ---- MaskedQnModel -----------------------------------------------------------------------

MaskedQnModel::MaskedQnModel(int n, std::vector<Eigen::Index> vars, std::unique_ptr<QnModel> inner)
    : n_(n), vars_(std::move(vars)), inner_(std::move(inner)) {
    if (n < 1) {
        bad("MaskedQnModel", "MaskedQnModel", fmt::format("n ({}) must be >= 1", n));
    }
    if (!inner_) {
        bad("MaskedQnModel", "MaskedQnModel", "inner model is null");
    }
    if (vars_.empty() || static_cast<int>(vars_.size()) != inner_->dimension()) {
        bad("MaskedQnModel", "MaskedQnModel",
            fmt::format("vars has {} entries, the inner model acts on {}", vars_.size(),
                        inner_->dimension()));
    }
    for (std::size_t k = 0; k < vars_.size(); ++k) {
        if (vars_[k] < 0 || vars_[k] >= n || (k > 0 && vars_[k] <= vars_[k - 1])) {
            bad("MaskedQnModel", "MaskedQnModel",
                fmt::format("vars[{}] = {} is out of [0, {}) or not ascending", k, vars_[k], n));
        }
    }
    publish();
}

Eigen::VectorXd MaskedQnModel::gather(const Eigen::VectorXd &v) const {
    Eigen::VectorXd out(static_cast<Eigen::Index>(vars_.size()));
    for (std::size_t k = 0; k < vars_.size(); ++k) {
        out(static_cast<Eigen::Index>(k)) = v(vars_[k]);
    }
    return out;
}

UpdateOutcome MaskedQnModel::update(const Eigen::VectorXd &s, const Eigen::VectorXd &y,
                                    double x_inf) {
    if (s.size() != n_ || y.size() != n_) {
        bad("MaskedQnModel", "update",
            fmt::format("s/y have sizes {}/{}, expected {}", s.size(), y.size(), n_));
    }
    const UpdateOutcome o = inner_->update(gather(s), gather(y), x_inf);
    if (o == UpdateOutcome::kUpdated) {
        publish();
    }
    return o;
}

Eigen::VectorXd MaskedQnModel::apply(const Eigen::VectorXd &v) const {
    if (v.size() != n_) {
        bad("MaskedQnModel", "apply", fmt::format("v has size {}, expected {}", v.size(), n_));
    }
    const Eigen::VectorXd w = inner_->apply(gather(v));
    Eigen::VectorXd out = Eigen::VectorXd::Zero(n_);
    for (std::size_t k = 0; k < vars_.size(); ++k) {
        out(vars_[k]) = w(static_cast<Eigen::Index>(k));
    }
    return out;
}

void MaskedQnModel::reset(ResetReason reason) {
    inner_->reset(reason);
    publish();
}

void MaskedQnModel::publish() {
    const MixedLiftData &in = inner_->mixed_lift();
    const auto d = static_cast<Eigen::Index>(vars_.size());
    lift_ = MixedLiftData{};
    lift_.sigma = in.sigma;
    lift_.diag = Eigen::VectorXd::Zero(n_);
    for (Eigen::Index k = 0; k < d; ++k) {
        lift_.diag(vars_[static_cast<std::size_t>(k)]) = in.diag.size() > 0 ? in.diag(k) : in.sigma;
    }
    if (in.base.rows() > 0) {
        std::vector<Eigen::Triplet<double>> t;
        t.reserve(static_cast<std::size_t>(in.base.nonZeros()));
        for (Eigen::Index r = 0; r < in.base.outerSize(); ++r) {
            for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator e(in.base, r); e;
                 ++e) {
                t.emplace_back(vars_[static_cast<std::size_t>(e.row())],
                               vars_[static_cast<std::size_t>(e.col())], e.value());
            }
        }
        lift_.base.resize(n_, n_);
        lift_.base.setFromTriplets(t.begin(), t.end());
        lift_.base.makeCompressed();
    }
    auto scatter_rows = [&](const Eigen::MatrixXd &src) {
        Eigen::MatrixXd out = Eigen::MatrixXd::Zero(n_, src.cols());
        for (Eigen::Index k = 0; k < d && src.cols() > 0; ++k) {
            out.row(vars_[static_cast<std::size_t>(k)]) = src.row(k);
        }
        return out;
    };
    lift_.u_plus = scatter_rows(in.u_plus);
    lift_.theta_plus = in.theta_plus;
    lift_.v = scatter_rows(in.v);
    lift_.kappa_plus = in.kappa_plus;
    lift_.kappa_v = in.kappa_v;
    lift_.support = vars_;
}

} // namespace hven::solvers::detail
