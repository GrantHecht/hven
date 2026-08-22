// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

#pragma once

// aggregate_eval_seam.h — the SQP driver's consumer-side binding onto the
// provider contract (model/nlp_aggregate.h), through the claim-stream
// interface (model/claim_stream_source.h).
//
// WHAT IT IS. One object that reproduces the driver's evaluation moments —
// today's free functions in drivers/sqp_driver.h — against an aggregate
// instead of an NlpModel, and reproduces them BIT-IDENTICALLY: the same
// NlpEval fields, the same QpProblem blocks, the same sparse structures. The
// driver's own arithmetic is untouched by construction, because the objects it
// reads are the same objects.
//
// THE BINDING IN ONE PARAGRAPH. At lay time the seam reads the provider's claim
// stream — one (assembled row, assembled column) pair per stored element the
// provider will scatter — and turns it into (a) a sorted row-major pattern per
// matrix, identical to the pattern the model's own return carries, (b) ONE
// owned value arena laid [H | Ae | Ai] in exactly those patterns' CSR order,
// and (c) the claim-slot -> arena-offset permutation, published as a
// KktLocationTable. Each later evaluation is then `values[location(slot)] +=
// v` on the provider's side and a contiguous segment copy on this one.
//
// What the path costs. Each evaluation moment runs model scratch -> arena
// scatter -> segment publish, where the free functions it replaces assigned
// into the driver's objects directly: the provider materializes its own
// results, scatters them into this seam's arena, and the seam copies the three
// contiguous segments out. Those transfers are per major, not per minor -- the
// QP engine's working-set iterations gain no copy, no branch and no
// indirection from any of it, which is the ground the R2.3 rider's scope
// stands on (docs/notes/2026-08-21-m4-task5-design.md §7).
//
// WHY THE SEED IS NEGATIVE ZERO. The contract's assemble ACCUMULATES (see
// model/nlp_aggregate.h) and the consumer owns the initial state, so every
// destination is seeded before the call. Seeding with -0.0 rather than +0.0 is
// what makes the accumulation the exact IDENTITY on every double: IEEE 754
// gives (-0.0) + x == x for every x including both zeros, whereas
// (+0.0) + (-0.0) == +0.0 would silently turn a model's negative zero — an
// ordinary Jacobian entry at, say, `-2.0 * x(1)` with x(1) == 0.0 — into a
// positive one. Nothing downstream reads a sign bit off a zero, but the
// preservation bar here is bit-identity, and this costs nothing to keep.
//
// NOT SELF-CONTAINED BY DESIGN, on the same footing as
// detail/globalization/sqp/soc.h: `NlpEval` is defined in
// drivers/sqp_driver.h and nowhere else, and this header never includes that
// one (the driver includes this one). Any includer must have NlpEval complete
// first.

#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/model/claim_space.h>
#include <hven/model/claim_stream_source.h>
#include <hven/model/structure_identity.h>

namespace hven::solvers {

/// @brief The driver's evaluation moments, served from an NlpAggregate.
///
/// Constructed from the claim-stream interface (model/claim_stream_source.h)
/// rather than from a concrete provider: this seam binds an interface, and any
/// provider that publishes a claim stream is a subject of it. The NlpAggregate
/// base publishes a declaration, an epoch and the evaluation entries -- what a
/// consumer needs in order to ASK for a fill -- and the claim stream is what a
/// consumer additionally needs in order to lay a destination it can be
/// scattered into. This seam lays its own destination, so it binds the half
/// that carries both.
///
/// CONCURRENCY: none of its own. The contract's posture applies unchanged —
/// one operation at a time on the aggregate, structural mutation included —
/// and this class adds no thread safety on top.
class AggregateEvalSeam {
  public:
    /// @brief Binds to a provider and lays the destinations its claim stream
    ///        describes.
    ///
    /// @param aggregate the provider, which must outlive this object.
    /// @throws std::invalid_argument if the claim stream names a coordinate
    ///         outside the assembled space the declaration describes, if a
    ///         Hessian claim is below the diagonal, or if the sorted claim
    ///         order does not reproduce the pattern it was built from.
    explicit AggregateEvalSeam(ClaimStreamSource &aggregate);

    /// Neither copied nor moved: the two location tables are non-owning views
    /// over THIS object's own index vectors, so a copy would publish tables
    /// addressing the original's storage. Deleting the copy also suppresses the
    /// implicit move, which is the intended posture -- a seam is constructed
    /// where it is used, beside the aggregate it binds to.
    AggregateEvalSeam(const AggregateEvalSeam &) = delete;
    AggregateEvalSeam &operator=(const AggregateEvalSeam &) = delete;

    // ---- what was laid ----------------------------------------------------

    /// @brief Primal variable count, as laid.
    Index n() const noexcept { return primal_vars_; }
    /// @brief Equality row count, as laid.
    Index me() const noexcept { return equality_rows_; }
    /// @brief Inequality row count, as laid.
    Index mi() const noexcept { return inequality_rows_; }

    /// @brief The materialized lower bounds, one row per declared variable.
    const Vec &lower() const noexcept { return lower_; }
    /// @brief The materialized upper bounds, one row per declared variable.
    const Vec &upper() const noexcept { return upper_; }

    /// @brief The structure epoch this seam last read, i.e. the one its
    ///        current destinations were laid against.
    StructureEpoch epoch() const noexcept { return epoch_at_lay_; }

    /// @brief The provider this seam is bound to.
    ClaimStreamSource &aggregate() const noexcept { return *aggregate_; }

    // ---- the evaluation moments -------------------------------------------

    /// @brief Full first-order evaluation at @p x: f, grad f, cE, cI, Je, Ji.
    ///
    /// Reproduces eval_nlp (drivers/sqp_driver.h) field for field, through the
    /// candidate values entry plus one assemble of the gradient-and-Jacobians
    /// shape. `all_finite` covers f, grad, cE and cI, exactly as there.
    ///
    /// THE MULTIPLIER BLOCKS ARE NOT READ, and are parameters only so that
    /// this moment and the subproblem moment take the same point. The shape
    /// this evaluation names consumes no multipliers, so the CandidatePoint it
    /// builds carries EMPTY multiplier blocks — which is what the contract
    /// means by empty: "no multipliers", never "zeros of the declared length".
    ///
    /// @param x         the point, in declaration space.
    /// @param lambda_e  unread; see above.
    /// @param lambda_i  unread; see above.
    /// @return the bundle.
    /// @throws std::invalid_argument through the aggregate's own entries if a
    ///         block does not match the declaration, or if the model returns a
    ///         block contradicting what it declares.
    NlpEval eval_nlp(const Vec &x, const Vec &lambda_e, const Vec &lambda_i);

    /// @brief Values only at @p x: f, cE, cI, with grad/Je/Ji carrying no
    ///        derivative information.
    ///
    /// Reproduces eval_nlp_values (drivers/sqp_driver.h), including its
    /// n / (me x n) / (mi x n)-sized empty linearization — an honestly empty
    /// one, not this seam's claim patterns.
    ///
    /// @param x the point, in declaration space.
    /// @return the bundle.
    NlpEval eval_nlp_values(const Vec &x);

    /// @brief Upgrades a values-only bundle at @p x to a full one in place:
    ///        grad, Je, Ji, with the GRADIENT's finiteness folded into
    ///        `all_finite`.
    ///
    /// The accepted-trial moment. Reproduces upgrade_to_full
    /// (drivers/sqp_driver.h), values deliberately not re-evaluated.
    ///
    /// Jacobian finiteness is NOT screened here. `all_finite` carries the
    /// objective, the gradient and the two residual blocks and nothing else,
    /// which is legacy parity with the free function this replaces.
    ///
    /// @param ev the bundle to upgrade, taken at THIS x.
    /// @param x  the point, in declaration space.
    void refresh_derivatives(NlpEval &ev, const Vec &x);

    /// @brief Fills @p ev's Je/Ji at @p x and nothing else.
    ///
    /// The zero-major ingest probe's moment: its values arrived on the
    /// values-only path and nothing downstream reads a gradient. A block the
    /// declaration gives no rows is left alone, exactly as the probe does.
    ///
    /// @param ev the bundle to fill, taken at THIS x.
    /// @param x  the point, in declaration space.
    void jacobians_only(NlpEval &ev, const Vec &x);

    /// @brief The QP subproblem at @p x, from a bundle taken at THIS x.
    ///
    /// Reproduces build_subproblem (drivers/sqp_driver.h): the Lagrangian
    /// Hessian is the one evaluation this call makes, and every other block
    /// comes from @p ev or from the materialized bounds. No trust region is
    /// baked in.
    ///
    /// @param ev        the bundle at @p x. Row counts are frozen across a
    ///                  solve, so a bundle whose blocks do not match the
    ///                  structures now laid predates a re-lay that changed one
    ///                  and is refused rather than paired with them.
    /// @param x         the point, in declaration space.
    /// @param lambda_e  the equality multipliers, full length.
    /// @param lambda_i  the inequality multipliers, full length.
    /// @param obj_scale scales the objective's gradient and Hessian halves.
    /// @return the subproblem.
    QpProblem build_subproblem(const NlpEval &ev, const Vec &x, const Vec &lambda_e,
                               const Vec &lambda_i, double obj_scale = 1.0);

  private:
    // The permutation-corruption hook the falsification pin needs. DECLARED
    // here and DEFINED only in the test that uses it, so there is no mutation
    // surface on the shipped class -- a friend declaration names a type, it
    // does not add a method.
    friend struct AggregateEvalSeamTestAccess;

    /// Reads the epoch and rebuilds every destination from the claim stream.
    void lay();

    /// Re-lays when the aggregate's epoch has moved since the last lay. Called
    /// first by every evaluation moment; a re-lay invalidates the cached
    /// patterns, so outputs produced after one carry the NEW structure.
    void relay_if_stale();

    /// Refuses a bundle whose block sizes are not the ones the structures now
    /// declare.
    ///
    /// Row counts are FROZEN ACROSS A SOLVE. A caller holds an NlpEval across
    /// several moments, and each of those moments may re-lay first, so a re-lay
    /// that changed a row count leaves the held bundle describing a problem
    /// these structures are no longer for. Fixed work, once per moment.
    ///
    /// @param ev     the bundle the caller handed in.
    /// @param moment the entry point's name, for the refusal message.
    /// @throws std::invalid_argument naming the block, the size held and the
    ///         size now declared.
    void require_bundle_matches_layout(const NlpEval &ev, const char *moment) const;

    /// Seeds one KKT arena segment with negative zero -- see the header note
    /// on why the seed is not +0.0.
    void seed_kkt_segment(const ClaimBlock &block);

    /// The KKT destination over the whole arena.
    KktScatterView kkt_view();

    /// The objective-gradient destination.
    RhsArenaView gradient_view();

    /// Copies one arena segment into a fresh copy of that domain's pattern.
    void publish_matrix(const ClaimBlock &block, const SpMatRM &pattern, SpMatRM &out) const;

    /// The Lagrangian-Hessian shape: one eval_hess and nothing else.
    void assemble_hessian(const Vec &x, const Vec &lambda_e, const Vec &lambda_i, double obj_scale);

    /// The gradient-and-Jacobians shape: the accepted-trial derivative bill.
    void assemble_gradient_and_jacobians(const Vec &x);

    /// The Jacobians-alone shape: the probe's derivative bill.
    void assemble_jacobians(const Vec &x);

    ClaimStreamSource *aggregate_ = nullptr;

    Index primal_vars_ = 0;
    Index equality_rows_ = 0;
    Index inequality_rows_ = 0;

    Vec lower_;
    Vec upper_;

    // The KKT arena, laid [H | Ae | Ai], and the claim-slot -> offset
    // permutation addressing it. The table is a non-owning view over
    // kkt_locations_, so both are rebuilt together at every lay.
    Vec arena_;
    std::vector<int> kkt_locations_;
    KktLocationTable kkt_table_;

    ClaimBlock hessian_;
    ClaimBlock equality_jacobian_;
    ClaimBlock inequality_jacobian_;

    // The objective-gradient arena and the rows the provider published for it.
    Vec gradient_arena_;
    std::vector<int> gradient_rows_;
    RhsLocationTable gradient_table_;

    // The three patterns, values unread: each output is a copy of one of these
    // with its value array overwritten from the arena segment.
    SpMatRM hessian_pattern_;
    SpMatRM equality_pattern_;
    SpMatRM inequality_pattern_;

    StructureEpoch epoch_at_lay_{};
};

} // namespace hven::solvers
