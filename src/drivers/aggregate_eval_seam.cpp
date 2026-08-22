// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

// The consumer-side binding's claim pass and its evaluation moments. The
// contract text it implements against is model/nlp_aggregate.h; the moments it
// reproduces are the free functions in drivers/sqp_driver.h, which this TU
// includes for NlpEval (see the seam header's NOT SELF-CONTAINED note).

#include <hven/drivers/sqp_driver.h>

#include "hven/detail/drivers/aggregate_eval_seam.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <stdexcept>

#include <fmt/format.h>

namespace hven::solvers {

namespace {

/// The arena's initial state. NOT +0.0: the provider ACCUMULATES, and
/// (-0.0) + x == x for every double x while (+0.0) + (-0.0) == +0.0 -- so this
/// seed, and only this seed, makes a one-contribution accumulation the exact
/// identity on a model's negative zeros too. The seam header carries the
/// argument in full.
inline constexpr double kArenaSeed = -0.0;

/// One domain's claims, in the walk order the provider handed them out.
struct DomainClaims {
    std::vector<int> rows_; ///< matrix-local row per claim, in walk order
    std::vector<int> cols_; ///< matrix-local column per claim, in walk order
};

/// Refuses a published location table whose largest location is past the end of
/// the destination it addresses.
///
/// Run wherever a table is BOUND -- which for this seam is the one lay() that
/// rebuilds every table and its destination together -- and never on an
/// evaluation. A table's own constructor validates the array's sentinels, but
/// it is handed no destination length and so cannot check the upper bound; this
/// is the only place both are in scope. It matters most for the rows a provider
/// publishes and this seam copies verbatim: unlike the KKT offsets, which this
/// seam computes itself, those arrive from outside, and one past the end is a
/// heap write during the provider's own scatter in a build with the asserts
/// compiled out.
///
/// @param locations          the published table's array.
/// @param destination_length the length of the array those locations index.
/// @param table              the table's name, for the refusal message.
/// @throws std::invalid_argument naming the table, the offending slot, its
///         location and the destination length.
void require_locations_within(const std::vector<int> &locations, Eigen::Index destination_length,
                              const char *table) {
    for (std::size_t slot = 0; slot < locations.size(); ++slot) {
        if (locations[slot] >= destination_length) {
            throw std::invalid_argument(
                fmt::format("AggregateEvalSeam: the {0}'s slot {1} names location {2}, but the "
                            "destination it is bound to is {3} long",
                            table, slot, locations[slot], destination_length));
        }
    }
}

/// Reads one claim block out of the assembled claim stream and translates it
/// into the matrix-local coordinates that block's own matrix carries.
///
/// The assembled space is square, n + me + mi, laid [primal | equality rows |
/// inequality rows] (model/claim_stream_source.h): a Hessian claim names
/// (i, j) with i <= j, an equality Jacobian claim (n + r, c), an inequality
/// Jacobian claim (n + me + r, c). The row offset undoes the second and third.
///
/// Every KKT claim coordinate is range-checked, once per claim at lay time and
/// never on an evaluation. This is the seam's own boundary rather than a
/// duplicate of the aggregate's: nothing else compares a KKT claim against the
/// destination the consumer is about to lay, and an out-of-range one would be a
/// write past the end of a pattern the consumer built.
///
/// The claim scope is exactly that -- KKT claims. The right-hand-side rows the
/// seam also copies in (objective_gradient_claim_rows) are not a permutation
/// and get no coordinate check here; what they get instead is the bound check
/// every published table gets, against the destination it addresses, where the
/// table is bound (require_locations_within below).
DomainClaims read_claims(Eigen::Ref<const Eigen::VectorXi> stream_rows,
                         Eigen::Ref<const Eigen::VectorXi> stream_cols, const ClaimBlock &block,
                         int row_offset, int matrix_rows, int matrix_cols, bool upper_triangle,
                         const char *domain) {
    DomainClaims claims;
    claims.rows_.reserve(static_cast<std::size_t>(block.count_));
    claims.cols_.reserve(static_cast<std::size_t>(block.count_));
    for (int index = 0; index < block.count_; ++index) {
        const int slot = block.start_ + index;
        const int row = stream_rows[slot] - row_offset;
        const int col = stream_cols[slot];
        if (row < 0 || row >= matrix_rows || col < 0 || col >= matrix_cols) {
            throw std::invalid_argument(
                fmt::format("AggregateEvalSeam: claim slot {0} names ({1}, {2}) in the {3} block, "
                            "which this aggregate declares as {4} x {5}",
                            slot, row, col, domain, matrix_rows, matrix_cols));
        }
        if (upper_triangle && row > col) {
            throw std::invalid_argument(
                fmt::format("AggregateEvalSeam: claim slot {0} names ({1}, {2}) in the {3} block, "
                            "below the diagonal; the assembled Hessian is the upper triangle only",
                            slot, row, col, domain));
        }
        claims.rows_.push_back(row);
        claims.cols_.push_back(col);
    }
    return claims;
}

/// Builds one domain's sorted row-major pattern and publishes the claim-slot
/// -> arena-offset half of the permutation for it.
///
/// The pattern is built through setFromTriplets, which is the same code path
/// the model's own returns are built through, so the structure arrays come out
/// equal to a `SpMatRM m = <model return>; m.makeCompressed()` rather than
/// merely equivalent. Explicit zeros are preserved there, which is what keeps a
/// structural-zero entry a claim rather than a hole.
///
/// THE CSR ORDER ASSUMPTION IS VERIFIED, NOT ASSUMED. Sorting the claims by
/// (row, then column) is expected to reproduce the pattern's own storage order
/// slot for slot; the walk below compares the two element by element and
/// refuses a mismatch. That check is the whole basis on which a per-evaluation
/// contiguous segment copy is allowed to stand in for a scatter.
void build_domain(const DomainClaims &claims, const ClaimBlock &block, int matrix_rows,
                  int matrix_cols, int arena_base, const char *domain, SpMatRM &pattern,
                  std::vector<int> &locations) {
    const int count = block.count_;

    std::vector<int> order(static_cast<std::size_t>(count));
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&claims](int left, int right) {
        if (claims.rows_[static_cast<std::size_t>(left)] !=
            claims.rows_[static_cast<std::size_t>(right)]) {
            return claims.rows_[static_cast<std::size_t>(left)] <
                   claims.rows_[static_cast<std::size_t>(right)];
        }
        return claims.cols_[static_cast<std::size_t>(left)] <
               claims.cols_[static_cast<std::size_t>(right)];
    });

    pattern.resize(matrix_rows, matrix_cols);
    if (count > 0) {
        std::vector<Eigen::Triplet<double>> triplets;
        triplets.reserve(static_cast<std::size_t>(count));
        for (int position = 0; position < count; ++position) {
            const std::size_t claim =
                static_cast<std::size_t>(order[static_cast<std::size_t>(position)]);
            triplets.emplace_back(claims.rows_[claim], claims.cols_[claim], 0.0);
        }
        pattern.setFromTriplets(triplets.begin(), triplets.end());
    }
    pattern.makeCompressed();

    if (static_cast<int>(pattern.nonZeros()) != count) {
        throw std::invalid_argument(fmt::format(
            "AggregateEvalSeam: the {0} block's {1} claims collapsed to {2} stored elements. Two "
            "claims naming one coordinate cannot be addressed by one arena offset each",
            domain, count, pattern.nonZeros()));
    }

    int position = 0;
    for (int outer = 0; outer < static_cast<int>(pattern.outerSize()); ++outer) {
        for (SpMatRM::InnerIterator it(pattern, outer); it; ++it, ++position) {
            const int claim_index = order[static_cast<std::size_t>(position)];
            const std::size_t claim = static_cast<std::size_t>(claim_index);
            if (static_cast<int>(it.row()) != claims.rows_[claim] ||
                static_cast<int>(it.col()) != claims.cols_[claim]) {
                throw std::invalid_argument(fmt::format(
                    "AggregateEvalSeam: the {0} block's sorted claim order does not reproduce its "
                    "own pattern -- position {1} holds ({2}, {3}) but the sorted claim there is "
                    "({4}, {5})",
                    domain, position, it.row(), it.col(), claims.rows_[claim],
                    claims.cols_[claim]));
            }
            // The permutation itself: the claim the provider will hand out at
            // slot `block.start_ + claim_index` lands at CSR position
            // `position` of this domain's segment.
            locations[static_cast<std::size_t>(block.start_ + claim_index)] = arena_base + position;
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Construction and layout
// ---------------------------------------------------------------------------

AggregateEvalSeam::AggregateEvalSeam(ClaimStreamSource &aggregate) : aggregate_(&aggregate) {
    this->lay();
}

void AggregateEvalSeam::lay() {
    // THE EPOCH IS READ FIRST, before a single structure is. The contract's
    // ordering guarantee makes the bump the last thing a re-lay does, so an
    // epoch read before the structures can only be STALE relative to them --
    // which costs one redundant re-lay at the next moment. Read after, a re-lay
    // landing in between would pair NEW structures with a NEW epoch that this
    // seam never actually laid against, and the staleness would never be
    // detected again.
    //
    // Read here, COMMITTED LAST. The read order is the argument above; the
    // commit point is a separate question, and it is the end of this function
    // because everything between can throw -- a declaration that refuses to
    // materialize its bounds, a claim stream this seam rejects, an allocation.
    // Committed here, such a throw would leave the new epoch standing over
    // half-rebuilt structures and relay_if_stale would never re-lay them again.
    // Committed last, a failed re-lay leaves the stale epoch, so the next
    // evaluation moment tries again.
    const StructureEpoch epoch_read_before_structures = aggregate_->structure_epoch();

    const AggregateDeclaration &declared = aggregate_->declaration();
    primal_vars_ = declared.primal_vars_;
    equality_rows_ = declared.equality_rows_;
    inequality_rows_ = declared.inequality_rows_;

    // The declaration's own intersection, in variable order, which is the
    // record the layout and the structural key's bound conjunct are both taken
    // over. Where the provider is the NlpModel bridge this is the model's own
    // lower()/upper() back again, and the same pair of vectors by
    // construction: that bridge lays one record per variable verbatim, and
    // intersecting a single record with (-inf, +inf) returns it.
    const std::vector<VariableBound> bounds = declared.materialize_variable_bounds();
    lower_.resize(primal_vars_);
    upper_.resize(primal_vars_);
    for (Index variable = 0; variable < primal_vars_; ++variable) {
        const VariableBound &record = bounds[static_cast<std::size_t>(variable)];
        lower_[variable] = record.lower_;
        upper_[variable] = record.upper_;
    }

    hessian_ = aggregate_->hessian_claims();
    equality_jacobian_ = aggregate_->equality_jacobian_claims();
    inequality_jacobian_ = aggregate_->inequality_jacobian_claims();

    Eigen::Ref<const Eigen::VectorXi> stream_rows = aggregate_->kkt_claim_rows();
    Eigen::Ref<const Eigen::VectorXi> stream_cols = aggregate_->kkt_claim_cols();
    const int total_claims = static_cast<int>(stream_rows.size());
    if (static_cast<int>(stream_cols.size()) != total_claims) {
        throw std::invalid_argument(
            fmt::format("AggregateEvalSeam: the claim stream publishes {0} rows and {1} columns",
                        total_claims, stream_cols.size()));
    }
    if (hessian_.count_ + equality_jacobian_.count_ + inequality_jacobian_.count_ != total_claims) {
        throw std::invalid_argument(fmt::format(
            "AggregateEvalSeam: the three claim blocks cover {0} of the stream's {1} slots",
            hessian_.count_ + equality_jacobian_.count_ + inequality_jacobian_.count_,
            total_claims));
    }

    // The counts summing to the stream length does NOT make the three ranges a
    // partition of it: overlapping blocks sum correctly and then hand two
    // domains the same slots, which corrupts the permutation kkt_locations_
    // carries and the arena segments publish_matrix copies out of. Both are
    // silent -- wrong values, no diagnostic -- so the ranges are checked here,
    // once per lay, for what the arena layout actually requires: in bounds,
    // pairwise disjoint, and in the order the arena is laid, Hessian then
    // equality Jacobian then inequality Jacobian.
    {
        const std::array<std::pair<const char *, const ClaimBlock *>, 3> ordered_blocks{
            {{"Hessian", &hessian_},
             {"equality Jacobian", &equality_jacobian_},
             {"inequality Jacobian", &inequality_jacobian_}}};
        int previous_end = 0;
        const char *previous_domain = "the start of the stream";
        for (const auto &[domain, block] : ordered_blocks) {
            if (block->start_ < 0 || block->count_ < 0 ||
                block->start_ + block->count_ > total_claims) {
                throw std::invalid_argument(
                    fmt::format("AggregateEvalSeam: the {0} block names slots [{1}, {2}) of a "
                                "claim stream {3} slots long",
                                domain, block->start_, block->start_ + block->count_,
                                total_claims));
            }
            if (block->start_ < previous_end) {
                throw std::invalid_argument(
                    fmt::format("AggregateEvalSeam: the {0} block starts at slot {1}, before {2} "
                                "ends at slot {3}; the three blocks must be disjoint and laid in "
                                "the order Hessian, equality Jacobian, inequality Jacobian",
                                domain, block->start_, previous_domain, previous_end));
            }
            previous_end = block->start_ + block->count_;
            previous_domain = domain;
        }
    }

    const int primal = static_cast<int>(primal_vars_);
    const int equality = static_cast<int>(equality_rows_);
    const int inequality = static_cast<int>(inequality_rows_);

    // Each domain's arena base is its own block's `start_`, which is also the
    // base seed_kkt_segment and publish_matrix address the segment by. Deriving
    // it here instead -- 0, then the Hessian count, then the Hessian plus
    // equality counts -- would give the same three numbers only for as long as
    // the provider keeps laying the stream H, Ae, Ai in that order with no gaps.
    // Reading `start_` at both ends makes them one number by construction, so a
    // later lay order cannot leave the permutation and the segment copies
    // pointing at different places.
    kkt_locations_.assign(static_cast<std::size_t>(total_claims), 0);
    build_domain(
        read_claims(stream_rows, stream_cols, hessian_, 0, primal, primal, true, "Hessian"),
        hessian_, primal, primal, hessian_.start_, "Hessian", hessian_pattern_, kkt_locations_);
    build_domain(read_claims(stream_rows, stream_cols, equality_jacobian_, primal, equality, primal,
                             false, "equality Jacobian"),
                 equality_jacobian_, equality, primal, equality_jacobian_.start_,
                 "equality Jacobian", equality_pattern_, kkt_locations_);
    build_domain(read_claims(stream_rows, stream_cols, inequality_jacobian_, primal + equality,
                             inequality, primal, false, "inequality Jacobian"),
                 inequality_jacobian_, inequality, primal, inequality_jacobian_.start_,
                 "inequality Jacobian", inequality_pattern_, kkt_locations_);

    // Uncontested by construction: this seam lays one arena slot per distinct
    // claimed coordinate and refuses a stream that names one twice within a
    // domain (the collapse refusal above), so no two slots it publishes address
    // one offset. There is therefore no clash mark to publish and no mutex
    // vector to key one against. The table's constructor accepts that form and
    // every scatter site reads only location().
    kkt_table_ = KktLocationTable(kkt_locations_.data(), total_claims, nullptr, 0, nullptr);

    // One padding slot when a model claims nothing at all -- an aggregate whose
    // Hessian and both Jacobians are structurally empty. The location table
    // still describes zero claims, which is what the provider checks; the
    // arena's own length exists only so the view is not the empty one a
    // KKT-bearing request is refused for.
    arena_.setConstant(std::max(total_claims, 1), kArenaSeed);
    require_locations_within(kkt_locations_, arena_.size(), "KKT location table");

    // The provider's own published rows, copied rather than referenced: the
    // table is a non-owning view, and the provider's storage moves under a
    // re-lay. Copying also carries a dropped-row sentinel through verbatim if
    // a provider ever publishes one.
    Eigen::Ref<const Eigen::VectorXi> gradient_rows = aggregate_->objective_gradient_claim_rows();
    gradient_rows_.assign(gradient_rows.data(), gradient_rows.data() + gradient_rows.size());
    gradient_table_ =
        RhsLocationTable(gradient_rows_.data(), static_cast<int>(gradient_rows_.size()));
    gradient_arena_.setConstant(primal_vars_, kArenaSeed);
    require_locations_within(gradient_rows_, gradient_arena_.size(),
                             "objective-gradient location table");

    // The commit, and the last statement for the reason the read comment above
    // states: nothing after this point can throw, and everything before it can.
    epoch_at_lay_ = epoch_read_before_structures;
}

void AggregateEvalSeam::require_bundle_matches_layout(const NlpEval &ev,
                                                      const char *moment) const {
    auto refuse = [&](const char *block, const std::string &held, const std::string &declared) {
        throw std::invalid_argument(
            fmt::format("AggregateEvalSeam::{0}: the bundle's {1} is {2}, but these structures "
                        "declare {3}. Row counts are frozen across a solve, so this bundle was "
                        "taken before a re-lay that changed one and must be re-taken",
                        moment, block, held, declared));
    };
    if (ev.ce.size() != equality_rows_) {
        refuse("equality residual block", fmt::format("{0} rows", ev.ce.size()),
               fmt::format("{0} rows", equality_rows_));
    }
    if (ev.ci.size() != inequality_rows_) {
        refuse("inequality residual block", fmt::format("{0} rows", ev.ci.size()),
               fmt::format("{0} rows", inequality_rows_));
    }
    if (ev.grad.size() != primal_vars_) {
        refuse("objective gradient", fmt::format("{0} rows", ev.grad.size()),
               fmt::format("{0} rows", primal_vars_));
    }
    if (ev.Je.rows() != equality_rows_ || ev.Je.cols() != primal_vars_) {
        refuse("equality Jacobian", fmt::format("{0} x {1}", ev.Je.rows(), ev.Je.cols()),
               fmt::format("{0} x {1}", equality_rows_, primal_vars_));
    }
    if (ev.Ji.rows() != inequality_rows_ || ev.Ji.cols() != primal_vars_) {
        refuse("inequality Jacobian", fmt::format("{0} x {1}", ev.Ji.rows(), ev.Ji.cols()),
               fmt::format("{0} x {1}", inequality_rows_, primal_vars_));
    }
}

void AggregateEvalSeam::relay_if_stale() {
    if (aggregate_->structure_epoch() != epoch_at_lay_) {
        this->lay();
    }
}

// ---------------------------------------------------------------------------
// The destinations, and the three assemble shapes
// ---------------------------------------------------------------------------

KktScatterView AggregateEvalSeam::kkt_view() {
    KktScatterView view;
    view.values_ = arena_.data();
    view.size_ = static_cast<int>(arena_.size());
    view.locations_ = &kkt_table_;
    return view;
}

RhsArenaView AggregateEvalSeam::gradient_view() {
    RhsArenaView view;
    view.values_ = gradient_arena_.data();
    view.size_ = static_cast<int>(gradient_arena_.size());
    view.locations_ = &gradient_table_;
    return view;
}

void AggregateEvalSeam::seed_kkt_segment(const ClaimBlock &block) {
    if (block.count_ > 0) {
        arena_.segment(block.start_, block.count_).setConstant(kArenaSeed);
    }
}

void AggregateEvalSeam::publish_matrix(const ClaimBlock &block, const SpMatRM &pattern,
                                       SpMatRM &out) const {
    out = pattern;
    if (block.count_ > 0) {
        Eigen::Map<Vec>(out.valuePtr(), block.count_) = arena_.segment(block.start_, block.count_);
    }
}

void AggregateEvalSeam::assemble_hessian(const Vec &x, const Vec &lambda_e, const Vec &lambda_i,
                                         double obj_scale) {
    this->seed_kkt_segment(hessian_);
    RhsScatterView rhs;
    aggregate_->assemble(CandidatePoint{x, lambda_e, lambda_i, obj_scale},
                         kRequestLagrangianHessian, this->kkt_view(), rhs);
}

void AggregateEvalSeam::assemble_gradient_and_jacobians(const Vec &x) {
    this->seed_kkt_segment(equality_jacobian_);
    this->seed_kkt_segment(inequality_jacobian_);
    gradient_arena_.setConstant(gradient_arena_.size(), kArenaSeed);
    // The shape consumes no multipliers, so the point carries EMPTY blocks --
    // "no multipliers", never zeros of the declared length. The objective scale
    // is 1.0 because this moment's gradient is the model's raw one; the
    // subproblem's scaling is applied where the subproblem is built.
    const Vec no_multipliers;
    RhsScatterView rhs;
    rhs.objective_gradient_ = this->gradient_view();
    aggregate_->assemble(CandidatePoint{x, no_multipliers, no_multipliers, 1.0},
                         kRequestGradientAndJacobians, this->kkt_view(), rhs);
}

void AggregateEvalSeam::assemble_jacobians(const Vec &x) {
    this->seed_kkt_segment(equality_jacobian_);
    this->seed_kkt_segment(inequality_jacobian_);
    const Vec no_multipliers;
    RhsScatterView rhs;
    aggregate_->assemble(CandidatePoint{x, no_multipliers, no_multipliers, 1.0},
                         kRequestConstraintJacobiansOnly, this->kkt_view(), rhs);
}

// ---------------------------------------------------------------------------
// The evaluation moments
// ---------------------------------------------------------------------------

NlpEval AggregateEvalSeam::eval_nlp(const Vec &x, const Vec &, const Vec &) {
    this->relay_if_stale();

    NlpEval ev;
    ev.ce.resize(equality_rows_);
    ev.ci.resize(inequality_rows_);
    // The values half. The candidate entry ASSIGNS (model/nlp_aggregate.h), so
    // these three blocks need no seeding, and the point's multiplier blocks are
    // legally empty on a values path.
    const Vec no_multipliers;
    CandidateValues values{ev.f, ev.ce, ev.ci};
    aggregate_->evaluate_candidate_values(CandidatePoint{x, no_multipliers, no_multipliers, 1.0},
                                          values);

    this->assemble_gradient_and_jacobians(x);
    ev.grad = gradient_arena_;
    this->publish_matrix(equality_jacobian_, equality_pattern_, ev.Je);
    this->publish_matrix(inequality_jacobian_, inequality_pattern_, ev.Ji);

    // Exactly eval_nlp's screen. The two residual conjuncts are unconditional
    // here where that function folds them under me()/mi() > 0, which is the
    // same predicate: a zero-row block is empty, and allFinite() on an empty
    // vector is vacuously true.
    ev.all_finite =
        std::isfinite(ev.f) && ev.grad.allFinite() && ev.ce.allFinite() && ev.ci.allFinite();
    return ev;
}

NlpEval AggregateEvalSeam::eval_nlp_values(const Vec &x) {
    this->relay_if_stale();

    NlpEval ev;
    ev.ce.resize(equality_rows_);
    ev.ci.resize(inequality_rows_);
    const Vec no_multipliers;
    CandidateValues values{ev.f, ev.ce, ev.ci};
    aggregate_->evaluate_candidate_values(CandidatePoint{x, no_multipliers, no_multipliers, 1.0},
                                          values);

    ev.all_finite = std::isfinite(ev.f) && ev.ce.allFinite() && ev.ci.allFinite();
    // The honestly-empty linearization, NOT this seam's claim patterns: a
    // caller that mistakenly reads a derivative here gets zeros of the right
    // shape rather than a stale structure.
    ev.grad = Vec::Zero(primal_vars_);
    ev.Je = SpMatRM(equality_rows_, primal_vars_);
    ev.Ji = SpMatRM(inequality_rows_, primal_vars_);
    return ev;
}

void AggregateEvalSeam::refresh_derivatives(NlpEval &ev, const Vec &x) {
    this->relay_if_stale();
    this->require_bundle_matches_layout(ev, "refresh_derivatives");

    this->assemble_gradient_and_jacobians(x);
    ev.grad = gradient_arena_;
    if (equality_rows_ > 0) {
        this->publish_matrix(equality_jacobian_, equality_pattern_, ev.Je);
    }
    if (inequality_rows_ > 0) {
        this->publish_matrix(inequality_jacobian_, inequality_pattern_, ev.Ji);
    }
    ev.all_finite = ev.all_finite && ev.grad.allFinite();
}

void AggregateEvalSeam::jacobians_only(NlpEval &ev, const Vec &x) {
    this->relay_if_stale();
    this->require_bundle_matches_layout(ev, "jacobians_only");

    this->assemble_jacobians(x);
    if (equality_rows_ > 0) {
        this->publish_matrix(equality_jacobian_, equality_pattern_, ev.Je);
    }
    if (inequality_rows_ > 0) {
        this->publish_matrix(inequality_jacobian_, inequality_pattern_, ev.Ji);
    }
}

QpProblem AggregateEvalSeam::build_subproblem(const NlpEval &ev, const Vec &x, const Vec &lambda_e,
                                              const Vec &lambda_i, double obj_scale) {
    this->relay_if_stale();
    this->require_bundle_matches_layout(ev, "build_subproblem");

    this->assemble_hessian(x, lambda_e, lambda_i, obj_scale);

    QpProblem qp;
    this->publish_matrix(hessian_, hessian_pattern_, qp.H);
    qp.H.makeCompressed();
    qp.g = obj_scale * ev.grad;

    qp.Ae = ev.Je;
    qp.Ae.makeCompressed();
    qp.be = -ev.ce;

    qp.Ai = ev.Ji;
    qp.Ai.makeCompressed();
    qp.bi = -ev.ci;

    qp.lower = lower_ - x;
    qp.upper = upper_ - x;
    return qp;
}

} // namespace hven::solvers
