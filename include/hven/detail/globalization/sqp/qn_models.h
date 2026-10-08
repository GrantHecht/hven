// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <memory>
#include <vector>

#include <Eigen/Dense>

#include <hven/detail/globalization/sqp/lbfgs_damped.h>

namespace hven::solvers::detail {

// The two quasi-Newton models beside LbfgsDamped (M7 QN quality round). References: Powell,
// Math. Programming 14 (1978) for the damping; Nocedal & Wright, Numerical Optimization
// (2006) sections 6.1 and 18.3 for the BFGS update and the initial scaling; Gill, Murray &
// Saunders, SIAM Review 47 (2005) section 2 for the two published ideas carried here -- a
// full-memory BFGS approximation when the nonlinear variables are few, and a Hessian
// approximation that acts on the nonlinear variables only.

/// FULL-MEMORY damped BFGS on a dense d x d B (d small). B0 = sigma0 I until the first
/// accepted pair re-bases it to (y^T y / s^T y) I (clamped to [sigma_min, sigma_max]); every
/// accepted pair then applies the damped BFGS update
///     B+ = B - (B s)(B s)^T / s^T B s + r r^T / s^T r,     r = Powell's damped y,
/// with LbfgsDamped's skip rules (non-finite, zero step, curvature, conditioning) and a
/// Cholesky test of B+ (a failed factorization is a conditioning skip; B is kept). There is
/// no window: every pair stays until a reset. options.memory, truncation_tol and rank_tol
/// (the window's) are unused. With options.self_scaling (Oren & Luenberger, Management
/// Science 20 (1974), restricted to scaling down), every pair after the first first scales B
/// by tau = s^T y / s^T B s when 0 < tau < 1 -- the correction for a B0 that overestimates
/// the curvature, which full memory otherwise unlearns one direction at a time.
///
/// MIXED-LIFT DATA: no low-rank part; diag = 0 and base = the upper triangle of B (all
/// d(d+1)/2 entries, so the pattern depends on d only). The lift is then the plain QP with H
/// replaced by B -- no auxiliaries and no lift rows.
class DenseDampedBfgs final : public QnModel {
  public:
    /// d >= 1, sigma0 finite > 0. Throws std::invalid_argument on a bad field.
    DenseDampedBfgs(int d, double sigma0, LbfgsDampedOptions options = {});

    int dimension() const override { return d_; }
    int pairs() const override { return pairs_; }
    double sigma() const override { return sigma_; }
    const LbfgsDampedCounters &counters() const override { return counters_; }
    UpdateOutcome update(const Eigen::VectorXd &s, const Eigen::VectorXd &y, double x_inf) override;
    Eigen::VectorXd apply(const Eigen::VectorXd &v) const override;
    void reset(ResetReason reason) override;
    bool positive_definite() const override { return pd_; }
    const MixedLiftData &mixed_lift() const override { return lift_; }

    const Eigen::MatrixXd &matrix() const { return b_; }

  private:
    void publish();

    int d_;
    double sigma0_;
    double sigma_;
    int pairs_ = 0;
    bool pd_ = true;
    LbfgsDampedOptions opt_;
    LbfgsDampedCounters counters_;
    Eigen::MatrixXd b_;
    MixedLiftData lift_;
};

/// THE NONLINEAR-VARIABLES-ONLY RESTRICTION: an inner model of dimension d acts on the
/// variables `vars` (ascending, a subset of [0, n)); every other variable carries ZERO
/// curvature, as it does in every Lagrangian Hessian when the problem is linear in it.
/// update() offers the inner model the restricted (s, y); apply() scatters B_inner v_vars;
/// the lift data is the inner model's, scattered to n rows, with diag = the inner scalar part
/// on `vars` and 0 elsewhere, and support = vars (the lift emits no entry off it).
/// positive_definite() is the inner model's (B is PSD on the full space by construction).
class MaskedQnModel final : public QnModel {
  public:
    /// n >= 1; vars ascending, non-empty, within [0, n); inner->dimension() == vars.size().
    MaskedQnModel(int n, std::vector<Eigen::Index> vars, std::unique_ptr<QnModel> inner);

    int dimension() const override { return n_; }
    int pairs() const override { return inner_->pairs(); }
    double sigma() const override { return inner_->sigma(); }
    const LbfgsDampedCounters &counters() const override { return inner_->counters(); }
    UpdateOutcome update(const Eigen::VectorXd &s, const Eigen::VectorXd &y, double x_inf) override;
    Eigen::VectorXd apply(const Eigen::VectorXd &v) const override;
    void reset(ResetReason reason) override;
    bool positive_definite() const override { return inner_->positive_definite(); }
    const MixedLiftData &mixed_lift() const override { return lift_; }

    const std::vector<Eigen::Index> &vars() const { return vars_; }
    const QnModel &inner() const { return *inner_; }

  private:
    Eigen::VectorXd gather(const Eigen::VectorXd &v) const;
    void publish();

    int n_;
    std::vector<Eigen::Index> vars_;
    std::unique_ptr<QnModel> inner_;
    MixedLiftData lift_;
};

} // namespace hven::solvers::detail
