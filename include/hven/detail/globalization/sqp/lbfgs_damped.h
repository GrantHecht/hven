// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <Eigen/Dense>

namespace hven::solvers::detail {

// Damped compact L-BFGS approximation B of the Lagrangian Hessian, in the SCALED space.
// Math only: no driver hook, no QP-engine coupling (the lift adapter and the driver
// integration are later stages). References: Powell, "Algorithms for nonlinear constraints
// that use Lagrangian functions" (Math. Programming 14, 1978) for the damping; Byrd, Nocedal
// & Schnabel (Math. Programming 63, 1994) for the compact form; Burdakov-Gong-Zikrin-Yuan
// (2017) for the eigen form.
//
// B = sigma*I - W N^-1 W^T, W = [sigma*S, R] (n x 2p), N = [[sigma*S^T S, L], [L^T, -D]],
// L_ij = s_i^T r_j (i > j), D = diag(s_i^T r_i); the pairs (s_i, r_i) are stored oldest first
// and replayed from B0 = sigma_k*I on EVERY rebuild (sigma is the newest pair's scale, so each
// update re-bases the whole window). W is column-normalised before its thin QR; the 2p x 2p
// symmetric eigendecomposition gives B = sigma*I + U*Theta*U^T with U (n x r) orthonormal.
// Eigen-columns with |theta| <= truncation_tol*sigma are dropped; the truncated form is the
// OPERATIONAL B (apply(), the Powell damping test and the lift data all read it).
//
// MIXED-LIFT DATA (ruling C30 BLOCKING-2): Theta is split by sign,
//   B = sigma*I + Up*Theta_p*Up^T - V*V^T,   V = Um*diag(sqrt(-theta_i)).
// SCALES (ruling C30 BLOCKING-1): kappa_i = 2*||Up_i||_1 for each Up column and
// kappa_i = 2*||V_i||_1 for each V column. Under a box ||p||_inf <= Delta the lifted
// auxiliary u_i = Up_i^T p (resp. w_i = V_i^T p) is bounded by ||col_i||_1*Delta, so
// auxiliaries divided by kappa_i stay within Delta/2 of the window.

/// Why an update() call did not enter the window.
enum class SkipReason : int {
    kNonFinite = 0, ///< s, y or the iterate norm is not finite
    kZeroStep,      ///< ||s||_inf <= zero_step_scale * max(1, ||x||_inf)
    kCurvature,     ///< s^T B s <= eps_mach * ||s||^2 * sigma
    kConditioning,  ///< after damping: s^T r < 1e-8 ||s||^2 or ||r||^2 / s^T r > 1e8
};
inline constexpr std::size_t kSkipReasonCount = 4;

/// Why the window was cleared. The SQP driver raises kRestorationResume, kConsecutiveSkips and
/// kRadiusFloor only; kRestorationEntry and kExplicit are never raised by it (a restoration
/// sub-solve starts a fresh instance instead) and exist for direct callers.
enum class ResetReason : int {
    kRestorationEntry = 0,
    kRestorationResume,
    kConsecutiveSkips,
    kRadiusFloor,
    kExplicit,
};
inline constexpr std::size_t kResetReasonCount = 5;

enum class UpdateOutcome : int {
    kUpdated = 0,
    kSkippedNonFinite,
    kSkippedZeroStep,
    kSkippedCurvature,
    kSkippedConditioning,
};

struct LbfgsDampedOptions {
    int memory = 10;                // window length m (>= 1)
    double zero_step_scale = 1e-12; // the driver passes kZeroStepScale
    double sigma_min = 1e-8;        // sigma clamp
    double sigma_max = 1e8;
    double truncation_tol = 1e-6; // tau': drop eigen-columns with |theta| <= tau'*sigma
    double rank_tol = 1e-10;      // |R_ii| of the column-normalised W below this = rank loss
    double powell_fraction = 0.2; // damping threshold
    double powell_target = 0.8;
    double curvature_floor = 1e-8; // s^T r >= floor*||s||^2
    double conditioning_cap = 1e8; // ||r||^2 / s^T r <= cap
};

/// Aggregate (bare snake_case members).
struct LbfgsDampedCounters {
    long updates = 0; // pairs accepted into the window
    long damped = 0;  // accepted pairs with theta < 1
    std::array<long, kSkipReasonCount> skipped{};
    std::array<long, kResetReasonCount> resets{};
    long rank_dropped = 0; // pairs evicted early by the rank test
    long pairs_in_memory = 0;
};

/// The mixed-lift data: B = sigma I + u_plus diag(theta_plus) u_plus^T - v v^T.
struct MixedLiftData {
    double sigma = 1.0;
    Eigen::MatrixXd u_plus;     // n x p, orthonormal columns, theta > 0
    Eigen::VectorXd theta_plus; // p, ascending
    Eigen::MatrixXd v;          // n x q, columns U_i*sqrt(-theta_i), theta < 0
    Eigen::VectorXd kappa_plus; // p, 2*||u_plus_i||_1
    Eigen::VectorXd kappa_v;    // q, 2*||v_i||_1
};

class LbfgsDamped {
  public:
    /// n >= 1, sigma0 finite > 0 (the scale used until the first pair is accepted and after a
    /// reset). Throws std::invalid_argument on a bad field.
    LbfgsDamped(int n, double sigma0, LbfgsDampedOptions options = {});

    int dimension() const { return n_; }
    int pairs() const { return static_cast<int>(s_.size()); }
    double sigma() const { return sigma_; }
    const LbfgsDampedCounters &counters() const { return counters_; }

    /// Offer the step s = x+ - x and the gradient difference y; x_inf is ||x||_inf of the
    /// iterate the step started from. Size mismatch throws; non-finite data is a counted skip.
    UpdateOutcome update(const Eigen::VectorXd &s, const Eigen::VectorXd &y, double x_inf);

    /// B*v (the truncated operational form). Size mismatch / non-finite v throws.
    Eigen::VectorXd apply(const Eigen::VectorXd &v) const;

    /// Clear the window, sigma <- sigma0 (counted by reason).
    void reset(ResetReason reason);
    /// Replace sigma0 (finite > 0); takes effect at once if the window is empty.
    void set_sigma0(double sigma0);

    /// Eigen form B = sigma I + U diag(theta) U^T (post truncation); theta ascending.
    const Eigen::MatrixXd &eigenvectors() const { return u_; }
    const Eigen::VectorXd &eigenvalues() const { return theta_; }
    /// Smallest / largest eigenvalue of B, read from Theta (sigma on the complement of U).
    double min_eigenvalue() const;
    double max_eigenvalue() const;
    const MixedLiftData &mixed_lift() const { return lift_; }

    /// Stored pairs, oldest first (the damped r, not y).
    const std::vector<Eigen::VectorXd> &stored_s() const { return s_; }
    const std::vector<Eigen::VectorXd> &stored_r() const { return r_; }

  private:
    void rebuild();   // rank trim + eigen form + lift data, from the stored pairs and sigma_
    bool try_build(); // false on rank loss / non-finite; fills u_, theta_ on success

    int n_;
    double sigma0_;
    double sigma_;
    LbfgsDampedOptions opt_;
    LbfgsDampedCounters counters_;
    std::vector<Eigen::VectorXd> s_, r_;
    Eigen::MatrixXd u_;
    Eigen::VectorXd theta_;
    MixedLiftData lift_;
};

} // namespace hven::solvers::detail
