// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

#pragma once

// nlp_model_aggregate.h — the bridge that carries a native NlpModel onto the
// Level 2 aggregate contract.
//
// NlpModel (model/nlp_model.h) is one problem's callbacks: f, cE, cI, their
// derivatives, and the exact Lagrangian Hessian, each returned whole by value.
// NlpAggregate (model/nlp_aggregate.h) is a collection of pieces plus the
// arenas they claim out of. This bridge is how a single-model problem reaches
// the second surface without the first gaining a method, a base class or an
// obligation.
//
// One serial piece: the bridge evaluates the whole model itself, on the calling
// thread, at partition count 1. No partitioning analysis, no fan-out, no locks.
// Its work is decomposition -- splitting the model's Hessian, Jacobian, gradient
// and residual returns into the contract's claims and arenas -- and declaring
// honestly what it does.
//
// The claims are stated in a square assembled space of n + me + mi, laid
// [primal | equality rows | inequality rows]. A Hessian claim names (i, j) with
// i <= j, the upper triangle the model itself returns; an equality Jacobian
// claim names (n + r, c); an inequality Jacobian claim names (n + me + r, c).
// No slack block and no solver coefficients: those are the consumer's own
// storage and the consumer's own step, per the mapping table in
// model/candidate_point.h.
//
// The bridge relies on a precondition the model already carries rather than a
// new one: eval_hess, eval_jac_e and eval_jac_i emit an invariant sparsity
// pattern -- independent of x, and for the Hessian of obj_scale and of the
// multiplier values as well (nlp_model.h's structural-pattern-invariance note).
// The claim pass walks those three patterns once and every later evaluation
// scatters through the slots that walk assigned.
//
// What the bridge requires of a return, stated at the door: every evaluation
// must present the same stored coordinates, in the same order, as the claim
// pass recorded at those slots. The scatter pairs the nth stored element with
// the nth claim slot, so this is the condition that makes the pairing mean
// anything -- and the bridge validates it per entry rather than trusting it. A
// return presenting a coordinate its slot was not claimed at is refused by
// name, never summed into the location laid for another coordinate.

#include <memory>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include "hven/core/types.h"
#include "hven/model/nlp_aggregate.h"
#include "hven/model/nlp_model.h"

namespace hven::solvers {

/// @brief One contiguous run of claim slots, as [start, start + count).
struct ClaimBlock {
    int start_ = 0;
    int count_ = 0;

    friend bool operator==(const ClaimBlock &, const ClaimBlock &) = default;
};

/// @brief An NlpAggregate over one NlpModel: a single serial piece at partition
///        count 1.
///
/// The bridge owns the decomposition. The model returns whole objects -- a
/// Hessian, two Jacobians, a gradient, two residual blocks -- and this class
/// splits them into the claims and arenas the contract addresses, evaluating
/// exactly the model methods each request needs and no others.
///
/// Capabilities: kValuesFastPath and nothing else. The values path runs
/// NlpModel::eval_values, whose own contract is values at the cost of the three
/// value calls, so a consumer pricing a probe at values cost is pricing it
/// correctly. kDirectScatter is not declared: the model's evaluators return by
/// value, so a provider-owned intermediate exists on every fill path.
///
/// Destination binding: none. The claims are offsets into whatever location
/// table a caller presents, so bound_kkt_destination() keeps the base's nullptr
/// and the assemble entry's identity check is vacuous here.
///
/// Concurrency: the contract's posture applies unchanged -- one operation at a
/// time, structural mutation included. Nothing in this class is thread-safe
/// beyond the epoch counter the base owns.
class NlpModelAggregate final : public NlpAggregate {
  public:
    /// @brief Builds the bridge and lays its structures for the first time.
    ///
    /// The lay validates the declaration at the contract's own boundary, then
    /// walks the model's three derivative patterns once at the model's own start
    /// point. It is a structural event like any other: the epoch is 1 on return.
    ///
    /// @param model the model to bridge; must be non-null.
    /// @throws std::invalid_argument if the model is null, reports dimensions
    ///         that cannot describe a problem, declares bounds that do not
    ///         intersect, returns a sparse block whose dimensions are not the
    ///         ones it declares, or returns a Hessian entry below the diagonal at
    ///         claim time. The triangle is checked where the pattern is walked and
    ///         nowhere else, so a later evaluation's triangle rests on the same
    ///         invariance precondition the per-call nonzero count rests on.
    /// The handle is `std::shared_ptr<const NlpModel>`: widened from a
    /// non-const handle by changing this one parameter, not by adding an
    /// overload, because a constness-only overload pair is ambiguous for a
    /// caller holding `shared_ptr<Derived>` (both conversions rank equally).
    /// The widening is source-compatible -- every `shared_ptr<NlpModel>`
    /// caller still converts implicitly. It also lets a consumer that does
    /// not own its model bridge a `const NlpModel &` through shared_ptr's
    /// aliasing constructor with a null owner, which is how
    /// drivers/sqp_driver.h's model-taking solve() overloads reach this
    /// contract. History: commit 9579e08; docs/notes/2026-08-m4-ledger.md:790.
    explicit NlpModelAggregate(std::shared_ptr<const NlpModel> model);

    /// @brief The declaration these structures were laid from.
    ///
    /// Stored state, refreshed only by a lay. Its piece lists are empty: those
    /// hold the partitioned engine's own type-erased handles, and this bridge is
    /// one serial piece of its own rather than a collection of them. The
    /// dimensions, the partition count and the bound records are what a consumer
    /// of this declaration reads, and they are all as laid.
    const AggregateDeclaration &declaration() const override { return laid_.declaration_; }

    /// @brief Adopts a partition count and returns what was adopted, which is
    ///        always 1.
    ///
    /// One serial piece is one partition's worth of work, so every positive
    /// request caps to 1 and the cap is reported through the return value. A
    /// non-positive request is refused rather than corrected. The call re-lays
    /// and therefore bumps the structure epoch, including when it adopts the
    /// count already in force.
    ///
    /// @param requested the partition count the caller wants.
    /// @return the adopted count, always 1.
    /// @throws std::invalid_argument if @p requested is below 1.
    int negotiate_partition_count(int requested) override;

    /// @brief The evaluation thread budget, which is 1.
    ///
    /// The bridge evaluates on the calling thread and has no pool to spend a
    /// budget on, so 1 is the count evaluation will actually use, whatever was
    /// requested.
    int evaluation_threads() const override { return 1; }

    /// @brief Accepts a thread request and changes nothing.
    ///
    /// A serial provider has one thread whatever is asked of it, so nothing is
    /// stored and evaluation_threads() keeps reporting the count evaluation will
    /// actually use.
    ///
    /// @param n the requested thread count.
    /// @throws std::invalid_argument if @p n is below 1.
    void set_evaluation_threads(int n) override;

    /// @brief The three conjuncts as captured at the last lay.
    ///
    /// Each through its one public builder: claim_stream_digest over the KKT
    /// claim stream, materialized_bound_digest over the declaration's bounds,
    /// and the adopted partition count.
    ModelStructureKey model_structure_key() const override { return laid_.key_; }

    /// @brief kValuesFastPath, and only that.
    AggregateCapability capabilities() const override {
        return AggregateCapability::kValuesFastPath;
    }

    /// @brief The structure epoch paired with a digest of the candidate values
    ///        at @p x.
    ///
    /// A values evaluation plus a hash, routed through the public values entry
    /// so it inherits that entry's validation.
    ///
    /// @param x the point to probe, in declaration space.
    /// @return the probe.
    IdentityProbe probe_identity(ConstVecRef x) override;

    /// @brief The model behind this bridge.
    const NlpModel &model() const { return *model_; }

    /// @brief Edge dimension of the assembled KKT space the claims are stated
    ///        in: n + me + mi.
    int kkt_dimension() const { return laid_.kkt_dimension_; }

    /// @brief Claim slot to assembled KKT row, in claim order.
    Eigen::Ref<const Eigen::VectorXi> kkt_claim_rows() const { return laid_.kkt_claim_rows_; }

    /// @brief Claim slot to assembled KKT column, in claim order.
    Eigen::Ref<const Eigen::VectorXi> kkt_claim_cols() const { return laid_.kkt_claim_cols_; }

    /// @brief The KKT claim slots the Lagrangian Hessian scatters through.
    ClaimBlock hessian_claims() const { return laid_.hessian_; }

    /// @brief The KKT claim slots the equality Jacobian scatters through.
    ClaimBlock equality_jacobian_claims() const { return laid_.equality_jacobian_; }

    /// @brief The KKT claim slots the inequality Jacobian scatters through.
    ClaimBlock inequality_jacobian_claims() const { return laid_.inequality_jacobian_; }

    /// @brief Claim slot to row of the objective-gradient arena.
    Eigen::Ref<const Eigen::VectorXi> objective_gradient_claim_rows() const {
        return laid_.objective_gradient_rows_;
    }

    /// @brief Claim slot to row of the constraint-adjoint-gradient arena.
    Eigen::Ref<const Eigen::VectorXi> constraint_adjoint_gradient_claim_rows() const {
        return laid_.adjoint_gradient_rows_;
    }

    /// @brief Claim slot to row of the equality-residual arena.
    Eigen::Ref<const Eigen::VectorXi> equality_residual_claim_rows() const {
        return laid_.equality_residual_rows_;
    }

    /// @brief Claim slot to row of the inequality-residual arena.
    Eigen::Ref<const Eigen::VectorXi> inequality_residual_claim_rows() const {
        return laid_.inequality_residual_rows_;
    }

  protected:
    /// @brief Evaluates one request against the model and scatters what it
    ///        names.
    ///
    /// The model evaluators each request set runs, and no others:
    ///
    ///   1. objective value only              eval_f
    ///   2. objective + constraint values     eval_values
    ///   3. + objective gradient              eval_values, eval_grad
    ///   4. + constraint adjoint gradient     eval_values, eval_grad,
    ///                                        eval_jac_e, eval_jac_i
    ///   5. constraint values + Jacobian      eval_ce, eval_ci, eval_jac_e,
    ///                                        eval_jac_i
    ///   6. first-order KKT                   eval_values, eval_grad,
    ///                                        eval_jac_e, eval_jac_i
    ///   7. constraint-only KKT               eval_ce, eval_ci, eval_jac_e,
    ///                                        eval_jac_i, eval_hess at
    ///                                        obj_scale 0
    ///   8. the full KKT system               eval_values, eval_grad,
    ///                                        eval_jac_e, eval_jac_i, eval_hess
    ///
    /// Shapes 1, 5 and 7 name no combination of objective value and constraint
    /// values, so they call the individual value evaluators rather than
    /// eval_values, which would compute a block the request did not name. Shape
    /// 7 reaches the constraint half of the Lagrangian Hessian by passing
    /// obj_scale 0, which nlp_model.h defines as the objective block dropped to
    /// a structural zero. eval_ce and eval_ci are skipped where the model
    /// declares no rows of that kind, matching eval_values' own skip.
    ///
    /// Whether a constraint block's Jacobian evaluator runs is decided by the
    /// block's declared row count, never by how many slots it claimed. A
    /// constant constraint has rows and an all-structural-zeros Jacobian: it
    /// claims nothing, and the request that names a Jacobian still calls its
    /// evaluator, which then scatters nothing. Only a block with no rows at all
    /// is skipped.
    ///
    /// @param point   the point, in declaration space.
    /// @param request the evaluation shape, already validated by the entry.
    /// @param kkt     the KKT destination, empty unless the request is
    ///                KKT-bearing.
    /// @param rhs     the right-hand-side destinations.
    /// @throws std::invalid_argument if a named arena's view does not match the
    ///         width this provider laid it over, if a location table does not
    ///         describe this provider's claim stream, or if the model returns a
    ///         block whose size, dimensions or nonzero count contradicts what it
    ///         declares or what the claim pass recorded.
    void assemble_impl(const CandidatePoint &point, EvalRequest request, KktScatterView kkt,
                       RhsScatterView rhs) override;

    /// @brief The model's values at a point, through NlpModel::eval_values.
    ///
    /// The one call, and no derivative evaluator: this is what kValuesFastPath
    /// declares.
    ///
    /// @param point the point, in declaration space.
    /// @param out   caller-owned storage, assigned.
    /// @throws std::invalid_argument if the model returns a residual block sized
    ///         differently from what it declares.
    void evaluate_candidate_values_impl(const CandidatePoint &point, CandidateValues out) override;

    /// @brief The model's values and first derivatives at a point.
    ///
    /// Runs eval_values, eval_grad, eval_jac_e and eval_jac_i. The constraint
    /// adjoint gradient is Je^T lambda_e + Ji^T lambda_i, the sign convention
    /// nlp_model.h's stationarity condition states.
    ///
    /// @param point the point, in declaration space, with full multiplier
    ///              blocks.
    /// @param out   caller-owned storage, assigned.
    /// @throws std::invalid_argument if the model returns a block whose size,
    ///         dimensions or nonzero count contradicts what it declares or what
    ///         the claim pass recorded.
    void evaluate_candidate_first_order_impl(const CandidatePoint &point,
                                             CandidateFirstOrder out) override;

  private:
    /// Everything one lay produces. Built into a local and committed whole, so a
    /// lay that throws part-way leaves the structures on hand untouched -- there
    /// is then nothing to restore and no structural event to report.
    struct LaidStructures {
        AggregateDeclaration declaration_;

        Eigen::VectorXi kkt_claim_rows_;
        Eigen::VectorXi kkt_claim_cols_;
        ClaimBlock hessian_;
        ClaimBlock equality_jacobian_;
        ClaimBlock inequality_jacobian_;
        int kkt_dimension_ = 0;

        Eigen::VectorXi objective_gradient_rows_;
        Eigen::VectorXi adjoint_gradient_rows_;
        Eigen::VectorXi equality_residual_rows_;
        Eigen::VectorXi inequality_residual_rows_;

        ModelStructureKey key_;
    };

    /// Builds a complete layout at @p partition_count without touching this
    /// object's own.
    LaidStructures lay(int partition_count) const;

    /// Commits a fresh layout and records the structural event. The bump is the
    /// last thing it does.
    void relay(int partition_count);

    /// Copies a point's blocks into the concrete vectors the model's signatures
    /// take, and returns the primal one.
    const Vec &stage_point(const CandidatePoint &point, bool with_multipliers);

    /// Runs eval_values and checks the two residual blocks against the declared
    /// row counts.
    void evaluate_values(const Vec &x, double &objective);

    /// Runs eval_ce/eval_ci alone, skipping a block the model declares no rows
    /// for.
    void evaluate_constraint_values(const Vec &x);

    /// Runs both Jacobian evaluators and checks each against its claim count.
    void evaluate_jacobians(const Vec &x);

    /// Writes Je^T lambda_e + Ji^T lambda_i into adjoint_scratch_.
    void compose_adjoint_gradient();

    // HELD CONST. The bridge reads the model and never mutates it, so the
    // storage says so and the compiler enforces it -- which is also what lets a
    // consumer bind a bridge over a `const NlpModel &` it does not own, through
    // shared_ptr's aliasing constructor (see drivers/sqp_driver.h's own
    // model-taking solve() overloads).
    std::shared_ptr<const NlpModel> model_;
    int primal_vars_ = 0;
    int equality_rows_ = 0;
    int inequality_rows_ = 0;

    LaidStructures laid_;

    // Evaluation scratch, sized at the first call and reused. The model's
    // signatures take concrete vectors and return whole objects, so a bridge
    // evaluation holds an intermediate by construction -- which is why
    // kDirectScatter is not declared.
    Vec x_scratch_;
    Vec equality_multiplier_scratch_;
    Vec inequality_multiplier_scratch_;
    Vec gradient_scratch_;
    Vec adjoint_scratch_;
    Vec equality_residual_scratch_;
    Vec inequality_residual_scratch_;
    SpMatRM hessian_scratch_;
    SpMatRM equality_jacobian_scratch_;
    SpMatRM inequality_jacobian_scratch_;

    // Storage the probe's CandidateValues view names; it must outlive the view.
    Vec probe_equality_scratch_;
    Vec probe_inequality_scratch_;
};

} // namespace hven::solvers
