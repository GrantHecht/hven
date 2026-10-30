// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

#include <hven/core/types.h>
#include <hven/detail/globalization/sqp/lbfgs_damped.h>
#include <hven/detail/globalization/sqp/mixed_lift.h>
#include <hven/detail/qp/ipqp_engine.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/ssn_engine.h>

namespace hven::solvers::detail {

/// Consecutive non-zero-step skips that reset the window (design m7-qn-design.md section 3). A
/// zero-step skip neither advances nor clears the streak: near a solution every step is zero,
/// and those skips say nothing about the pairs (design m7-qn-design.md section 3, the skip rule).
inline constexpr Index kQnSkipResetStreak = 5;

/// Offers (s, y) on the leading qn.dimension() entries; the kQnSkipResetStreak-th consecutive
/// non-zero-step skip resets the window (ResetReason::kConsecutiveSkips) and clears the streak.
UpdateOutcome offer_pair_with_skip_reset(QnModel &qn, Index &skip_streak, const Vec &s,
                                         const Vec &y, double x_inf);

// THE ENGINE-SIGNATURE DECORATOR of ruling C30 AMEND-3 (M7 QN stage 3). Under
// SqpHessianMode::kLbfgsDamped every QP engine call the SQP driver makes goes through one of
// these members: the subproblem (its H ignored) is lifted through the current B (mixed_lift.h),
// the warm seed is lifted with it, the engine runs on the lifted QP, and the answer is un-lifted
// -- x, z, bound_state, tr_active, lambda_e, the Farkas certificate, and the interior tier's
// box, bound duals, faces and infeasibility evidence -- back to the caller's sizes.
//
// ONE LIFT PER (SUBPROBLEM, B): lifts are cached (two slots) and keyed on the subproblem's
// CONTENT -- n, me, mi, bounds and both Jacobian blocks by bytes -- so a shrink-retry, the cap
// retry and every re-offer of the same subproblem reuse the lift bit for bit, and a QP that
// differs only in g/be/bi (an SOC re-solve, a later elastic rung) is served by
// MixedLift::lift_variant. invalidate() drops both slots; the driver calls it whenever B moves.
//
// Seeds whose blocks match neither the unlifted nor a lifted size are dropped field by field,
// exactly as the engines themselves ignore a wrongly sized seed field; a lifted-size interior
// carry (the tier's own warm_carry, from any earlier lift) is truncated to the unlifted blocks
// and re-lifted through the current B.
class QnLiftBridge {
  public:
    /// @param qn       the B every lift reads (borrowed; must outlive this object).
    /// @param n_struct the leading variables B acts on (1 <= n_struct <= qn.dimension()).
    QnLiftBridge(const QnModel &qn, Index n_struct);

    const QnModel &model() const { return *qn_; }
    Index n_struct() const { return n_struct_; }

    /// Drop the cached lifts: B has moved.
    void invalidate();

    /// The lifted problem the engines see for `qp` (cached as above).
    const QpProblem &lift_of(const QpProblem &qp);

    /// -(g^T p + 1/2 p^T B p), B acting on p's leading n_struct entries (trailing columns carry
    /// no curvature) -- `predicted_decrease`'s QN form.
    double predicted_decrease(const QpProblem &qp, const Vec &p) const;

    /// The walk: cold, warm, and warm with a hot handle offered.
    QpSolution solve(const QpEngine &engine, const QpProblem &qp, const SolveOverrides &o);
    QpSolution solve(const QpEngine &engine, const QpProblem &qp, const QpSolution &seed,
                     const SolveOverrides &o);
    QpSolution solve(const QpEngine &engine, const QpProblem &qp, const QpSolution &seed,
                     const SolveOverrides &o, const std::shared_ptr<const HotState> &hot);
    /// Tier 3 on an externally identified face (QpEngine::refine_on_face).
    bool refine_on_face(const QpEngine &engine, const QpProblem &qp, const QpSolution &face,
                        const SolveOverrides &o, QpSolution &out);
    /// The SSN tier. `out` is un-lifted in place.
    void solve(SsnEngine &engine, const QpProblem &qp, const SsnStart &start,
               const SsnOptions &sopts, const SolveOverrides &o, SsnResult *out);
    /// The interior tier.
    IpqpResult solve(IpqpEngine &engine, const QpProblem &qp, const IpqpSeed *seed,
                     const IpqpOptions &iopts, const SolveOverrides &o);

    /// Whether `seed` can seed `qp` through the lift: its blocks are the unlifted sizes, or
    /// a lifted carry's -- accepted only when `qp` has exactly the (n, me) of the last lift an
    /// interior-tier solve ran on and the carry has exactly that lift's sizes (a change in the
    /// equality-basis reduction between the two would otherwise misalign the lambda_e seed);
    /// lambda_i exactly mi either way.
    bool ipqp_seed_usable(const IpqpSeed &seed, const QpProblem &qp) const;

    /// What the lifted solves reported since the last take_tally(): the widest lift built
    /// (r+ + r-), the solves whose engine reported a wrong-inertia reading, and the lifts built
    /// while the model did not read positive definite (QnModel::positive_definite(): rounding
    /// can push the eigen form's smallest eigenvalue to or below zero) -- both bug flags; and the
    /// number of variables the model carries (QnModel::carried_dimension(), set by take_tally()).
    struct Tally {
        Index lift_rank_peak = 0;
        Index kwrong_seen = 0;
        Index nonpd_seen = 0;
        Index dimension = 0;
    };
    Tally take_tally();

  private:
    struct Slot {
        std::optional<MixedLift> lift;
        QpProblem source;  // the lift's input, H cleared
        QpProblem variant; // the last variant served from this slot (lifted)
        QpProblem variant_source;
        bool has_variant = false;
        std::uint64_t used = 0;
    };

    /// The lifted problem of `qp` and its lift, served from the cache or built.
    const QpProblem &lifted(const QpProblem &qp, const MixedLift **lift);

    const QnModel *qn_;
    Index n_struct_;
    // The last interior-tier lift: the unlifted (n, me) it was built for and its lifted
    // (n, me). -1 until the first one.
    Index ipqp_n_ = -1, ipqp_me_ = -1, ipqp_n_lifted_ = -1, ipqp_me_lifted_ = -1;
    std::array<Slot, 2> slots_;
    std::uint64_t clock_ = 0;
    Tally tally_;
};

} // namespace hven::solvers::detail
