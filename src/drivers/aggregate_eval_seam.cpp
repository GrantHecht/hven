// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The consumer-side binding's claim pass and its evaluation moments. The
// contract text it implements against is model/nlp_aggregate.h; the moments it
// reproduces are the free functions in drivers/sqp_driver.h, which this TU
// includes for NlpEval (see the seam header's NOT SELF-CONTAINED note).

#include <hven/drivers/sqp_driver.h>

#include "hven/detail/drivers/aggregate_eval_seam.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
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

/// @brief Refuses a published location table whose largest location is past the
///        end of the destination it addresses.
///
/// Run wherever a table is BOUND -- which for this seam is the one lay() that
/// rebuilds every table and its destination together -- and never on an
/// evaluation. No table is handed a destination length at construction, so none
/// can check the upper bound; this is the only place both are in scope.
///
/// The LOWER end is not this scan's: RhsLocationTable's constructor rejects a
/// row below -1, and KktLocationTable element-checks only its clash marks, so a
/// consumer binding a KKT table whose locations could be negative owes that
/// check itself.
///
/// It matters most for the rows a provider publishes and this seam copies
/// verbatim: one past the end is a heap write during the provider's own
/// scatter, in a build with Eigen's own asserts compiled out.
///
/// @param locations          the published table's array.
/// @param destination_length the length of the array those locations index.
/// @param table              the table's name, for the refusal message.
/// @throws std::invalid_argument naming the table, the offending slot, its
///         location and the destination length.
void require_locations_within(const int *locations, std::size_t count,
                              Eigen::Index destination_length, const char *table) {
    for (std::size_t slot = 0; slot < count; ++slot) {
        if (locations[slot] >= destination_length) {
            throw std::invalid_argument(
                fmt::format("AggregateEvalSeam: the {0}'s slot {1} names location {2}, but the "
                            "destination it is bound to is {3} long",
                            table, slot, locations[slot], destination_length));
        }
    }
}

/// The same check over an owned table.
void require_locations_within(const std::vector<int> &locations, Eigen::Index destination_length,
                              const char *table) {
    require_locations_within(locations.data(), locations.size(), destination_length, table);
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
///
/// THIS SEAM'S PUBLISHED SUPPORT STATEMENT: IT SERVES INJECTIVE CLAIM STREAMS.
/// A stream that names one coordinate twice within a domain is refused here,
/// and that refusal is this consumer declaring what it supports -- not Level 2
/// prohibiting anything. A claim stream is free to be non-injective on
/// coordinates (see nlp_aggregate.h's CLAIM EXCLUSIVITY paragraph: a slot is
/// not a coordinate), and resolving several slots onto one destination is the
/// consumer's to choose. This one chose not to: it lays exactly one arena slot
/// per stored pattern element and publishes a domain by copying that
/// contiguous segment straight onto the pattern's value array
/// (publish_matrix), so a coordinate named twice has one offset to hand to two
/// claims. A consumer that publishes clash marks and a lock vector instead
/// accepts colliding claims and accumulates them -- the interior engine does
/// exactly that. A provider whose pieces overlap therefore pairs with that
/// consumer, and meets a NAMED refusal here rather than a wrong answer.
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
            "AggregateEvalSeam: the {0} block's {1} claims name only {2} distinct coordinates. "
            "This seam supports injective claim streams: it lays one arena slot per stored "
            "pattern element and publishes the domain by copying that segment onto the pattern's "
            "value array, so a coordinate named twice has one offset for two claims. A "
            "non-injective stream is legal and is served by a consumer that publishes clash marks "
            "and a lock vector; this one does not",
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
    // COMMITTED LAST, at the end of this function, because everything between
    // can throw -- a declaration that refuses to materialize its bounds, a
    // claim stream this seam rejects, an allocation. Committed here, such a
    // throw would leave the new epoch standing over half-rebuilt structures and
    // relay_if_stale would never re-lay them again.
    const StructureEpoch epoch_read_before_structures = aggregate_->structure_epoch();

    const AggregateDeclaration &declared = aggregate_->declaration();

    // THE INSTALLED W0.2 FACTORS ARE INDEXED BY ROW, and a re-lay that changes a
    // row count would leave every apply site reading past the end of a factor
    // block -- out of bounds in Release, where Eigen's own asserts are compiled
    // out (CLAUDE.md section 4). The factors are computed once, from the start
    // point's derivatives, and are FIXED for the solve by design, so there is no
    // correct way to carry them across such a re-lay: they describe rows that no
    // longer exist. Refused here, in the same voice require_bundle_matches_layout
    // refuses a bundle these structures no longer describe, and BEFORE anything
    // is written, so the refusal leaves this seam exactly as it was.
    if (scaling_.active && (scaling_.eq_rows.size() != declared.equality_rows_ ||
                            scaling_.ineq_rows.size() != declared.inequality_rows_)) {
        throw std::invalid_argument(fmt::format(
            "AggregateEvalSeam: the aggregate re-laid to ({}, {}) constraint rows while a problem "
            "scaling sized for ({}, {}) was installed; the factors are fixed for a solve and "
            "cannot describe rows that changed under them",
            declared.equality_rows_, declared.inequality_rows_, scaling_.eq_rows.size(),
            scaling_.ineq_rows.size()));
    }

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
    // Narrowed once, checked once. Everything below reasons in int about this
    // number, and the block scan is hardened precisely because provider-supplied
    // ints can reach the edges of the type; a stream longer than an int can
    // count is refused here rather than silently becoming a different number.
    if (stream_rows.size() > static_cast<Eigen::Index>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument(
            fmt::format("AggregateEvalSeam: the claim stream publishes {0} slots, more than this "
                        "seam can address",
                        stream_rows.size()));
    }
    const int total_claims = static_cast<int>(stream_rows.size());
    if (stream_cols.size() > static_cast<Eigen::Index>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument(
            fmt::format("AggregateEvalSeam: the claim stream publishes {0} columns, more than "
                        "this seam can address",
                        stream_cols.size()));
    }
    if (static_cast<int>(stream_cols.size()) != total_claims) {
        throw std::invalid_argument(
            fmt::format("AggregateEvalSeam: the claim stream publishes {0} rows and {1} columns",
                        total_claims, stream_cols.size()));
    }
    // THE WHOLE BLOCK SCAN IS ARITHMETIC ON PROVIDER-SUPPLIED INTS, so it is
    // ordered to keep every intermediate representable. Non-negativity is
    // established for all three blocks BEFORE anything is summed or subtracted;
    // the coverage sum is then taken in a wider type, because three int counts
    // can carry past an int between them; and the one end that is formed BEFORE
    // its block is known to fit -- the one the out-of-bounds refusal prints --
    // is formed wide, since the value being reported may be exactly the one that
    // would not fit. Ends are formed in int only after the bound check has
    // proved them no larger than total_claims, which is itself an int. A
    // provider that hands over nonsense gets a refusal naming it, rather than
    // undefined behaviour on the way to one.
    //
    // In DECLARATION order -- Hessian, equality Jacobian, inequality Jacobian --
    // which is the order the three accessors are named in and nothing more. The
    // per-block checks below are order-independent; the pairwise check sorts a
    // copy of this by start_ before it looks at neighbours.
    const std::array<std::pair<const char *, const ClaimBlock *>, 3> declared_blocks{
        {{"Hessian", &hessian_},
         {"equality Jacobian", &equality_jacobian_},
         {"inequality Jacobian", &inequality_jacobian_}}};

    for (const auto &[domain, block] : declared_blocks) {
        if (block->start_ < 0 || block->count_ < 0) {
            throw std::invalid_argument(
                fmt::format("AggregateEvalSeam: the {0} block reports start {1} and count {2}; "
                            "neither may be negative",
                            domain, block->start_, block->count_));
        }
    }

    const std::int64_t covered = static_cast<std::int64_t>(hessian_.count_) +
                                 static_cast<std::int64_t>(equality_jacobian_.count_) +
                                 static_cast<std::int64_t>(inequality_jacobian_.count_);
    if (covered != static_cast<std::int64_t>(total_claims)) {
        throw std::invalid_argument(fmt::format(
            "AggregateEvalSeam: the three claim blocks cover {0} of the stream's {1} slots",
            covered, total_claims));
    }

    // WHAT THE THREE BLOCKS MUST BE, and what they need not be. The arena is one
    // slot per claim, and each domain is seeded and published through its OWN
    // block's start_ (seed_kkt_segment, publish_matrix, and the arena_base
    // handed to build_domain), so the layout requires that the three ranges be
    // in bounds and pairwise disjoint -- together they then partition
    // [0, total_claims) exactly, since their counts already sum to it and each
    // sits inside it. It does NOT require them to arrive in any particular
    // ORDER: nothing here derives one domain's base from another's count, so a
    // provider that lays its equality Jacobian ahead of its Hessian is served
    // correctly. The claim-stream interface fixes contiguity per domain, not an
    // order between domains, and this scan says exactly that and no more.
    //
    // The counts summing to the stream length is not enough on its own:
    // overlapping blocks sum correctly and then hand two domains the same
    // slots, which corrupts both the permutation kkt_locations_ carries and the
    // arena segments publish_matrix copies out of -- silently, with wrong
    // values and no diagnostic. So disjointness is checked here, once per lay,
    // over a copy of the three taken in start_ order rather than in the order
    // the accessors are declared.
    for (const auto &[domain, block] : declared_blocks) {
        // Compared as start_ > total_claims - count_ rather than
        // start_ + count_ > total_claims: both operands are non-negative by the
        // pass above and total_claims is non-negative, so the subtraction is
        // representable, while the sum need not be.
        if (block->start_ > total_claims - block->count_) {
            throw std::invalid_argument(fmt::format(
                "AggregateEvalSeam: the {0} block names slots [{1}, {2}) of a "
                "claim stream {3} slots long",
                domain, block->start_, static_cast<std::int64_t>(block->start_) + block->count_,
                total_claims));
        }
    }

    {
        // An empty block claims nothing, overlaps nothing, and its start_ says
        // nothing about ownership, so only the blocks that actually hold slots
        // take part. Taking those in start_ order is what makes the check
        // order-agnostic: a neighbouring pair in that order is the only pair
        // that can overlap, so one pass over at most three entries decides it.
        //
        // Ordered by hand rather than through a library sort: three entries and
        // a one-integer comparison, where a generic sort would instantiate its
        // merge machinery into this function for no benefit.
        std::array<std::pair<const char *, const ClaimBlock *>, 3> occupied{};
        int occupied_count = 0;
        for (const auto &entry : declared_blocks) {
            if (entry.second->count_ == 0) {
                continue;
            }
            int position = occupied_count;
            while (position > 0 && occupied[position - 1].second->start_ > entry.second->start_) {
                occupied[position] = occupied[position - 1];
                --position;
            }
            occupied[position] = entry;
            ++occupied_count;
        }

        for (int index = 0; index + 1 < occupied_count; ++index) {
            const auto &current = occupied[index];
            const auto &following = occupied[index + 1];
            // Both ends are representable: the bound pass above proved each
            // start_ + count_ no larger than total_claims, which is an int.
            const int current_end = current.second->start_ + current.second->count_;
            if (following.second->start_ < current_end) {
                throw std::invalid_argument(fmt::format(
                    "AggregateEvalSeam: the {0} block names slots [{1}, {2}) and the {3} block "
                    "names [{4}, {5}); the three blocks must be pairwise disjoint",
                    current.first, current.second->start_, current_end, following.first,
                    following.second->start_, following.second->start_ + following.second->count_));
            }
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

    // Uncontested BY THIS SEAM'S OWN SUPPORT STATEMENT, not by anything the
    // provider owes: it serves injective claim streams and refuses a stream
    // that names one coordinate twice within a domain (the collapse refusal
    // above), so no two slots it publishes address one offset. There is
    // therefore no clash mark to publish and no mutex vector to key one
    // against. The table's constructor accepts that form and every scatter
    // site reads only location().
    kkt_table_ = KktLocationTable(kkt_locations_.data(), total_claims, nullptr, 0, nullptr);

    // One padding slot when a model claims nothing at all -- an aggregate whose
    // Hessian and both Jacobians are structurally empty. The location table
    // still describes zero claims, which is what the provider checks; the
    // arena's own length exists only so the view is not the empty one a
    // KKT-bearing request is refused for.
    arena_.setConstant(std::max(total_claims, 1), kArenaSeed);
    require_locations_within(kkt_locations_, arena_.size(), "KKT location table");

    // THE PROVIDER'S OWN PUBLISHED ROWS, VIEWED RATHER THAN COPIED, and the table
    // holds that view across every evaluation of this layout. What makes that
    // sound is one NORMATIVE sentence of the claim-stream contract, quoted here
    // because the guard below is stated against the OTHER epoch: "a provider
    // republishes claim-stream storage only from inside a re-lay, so a
    // claim-stream epoch bump is always accompanied by a structure epoch bump"
    // (model/claim_stream_source.h, VIEW VALIDITY). This seam re-lays on the
    // structure epoch (relay_if_stale), which is the coarser of the two signals,
    // so it re-reads at least as often as the rows can move. No evaluation can
    // reach a table bound to rows the provider has replaced.
    //
    // The dropped-row sentinel a provider may publish for an eliminated
    // coordinate rides through untouched, exactly as the copy carried it: this
    // reads the array, it does not interpret it.
    Eigen::Ref<const Eigen::VectorXi> gradient_rows = aggregate_->objective_gradient_claim_rows();
    gradient_table_ =
        RhsLocationTable(gradient_rows.data(), static_cast<int>(gradient_rows.size()));
    gradient_arena_.setConstant(primal_vars_, kArenaSeed);
    require_locations_within(gradient_rows.data(), static_cast<std::size_t>(gradient_rows.size()),
                             gradient_arena_.size(), "objective-gradient location table");

    // The commit, and the last statement for the reason the read comment above
    // states: nothing after this point can throw, and everything before it can.
    epoch_at_lay_ = epoch_read_before_structures;
}

void AggregateEvalSeam::require_bundle_matches_layout(const NlpEval &ev, const char *moment) const {
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

// ===========================================================================
// THE W0.2 PROBLEM-SCALING LAYER'S APPLY SITES.
//
// detail/drivers/problem_scaling.h carries the transformation, its inverse and
// the argument for both. What is here is only the applying, and it is here
// rather than in that TU because this is where the values are: a scale is one
// multiply per element on a buffer this seam has just filled and still owns.
//
// EVERY SITE IS BEHIND `scaling_.active`. At the shipped default
// (SqpOptions::enable_scaling == false) no factor is ever installed, every
// predicate below is false, and this seam does the arithmetic it has always
// done -- which is what the OFF-path bit-identity pin asserts.
// ===========================================================================

void AggregateEvalSeam::install_scaling(detail::ProblemScaling scaling) {
    if (scaling.active) {
        // Sized against THIS seam's laid row counts, because that is what every
        // apply site indexes against. A mismatch is a caller error and is
        // refused here rather than becoming an out-of-range read per major --
        // CLAUDE.md section 4's boundary rule, and Eigen's own asserts are
        // compiled out under NDEBUG so they cannot be the guard.
        if (scaling.eq_rows.size() != equality_rows_ ||
            scaling.ineq_rows.size() != inequality_rows_) {
            throw std::invalid_argument(fmt::format(
                "AggregateEvalSeam::install_scaling: factor blocks are ({}, {}) but this seam is "
                "laid for ({}, {}) constraint rows",
                scaling.eq_rows.size(), scaling.ineq_rows.size(), equality_rows_,
                inequality_rows_));
        }
    }
    scaling_ = std::move(scaling);
}

void AggregateEvalSeam::scale_jacobian_rows(const Vec &factors, SpMatRM &jac) {
    // ROW-MAJOR is what makes this one pass with no indirection: a row's stored
    // entries are contiguous in the outer index, so the factor is loaded once
    // per row rather than once per entry.
    for (Index r = 0; r < jac.rows(); ++r) {
        const double f = factors(r);
        for (SpMatRM::InnerIterator it(jac, r); it; ++it) {
            it.valueRef() *= f;
        }
    }
}

void AggregateEvalSeam::scale_values(NlpEval &ev) const {
    if (!scaling_.active) {
        return;
    }
    ev.f *= scaling_.obj;
    if (equality_rows_ > 0) {
        ev.ce.array() *= scaling_.eq_rows.array();
    }
    if (inequality_rows_ > 0) {
        ev.ci.array() *= scaling_.ineq_rows.array();
    }
}

void AggregateEvalSeam::scale_derivatives(NlpEval &ev, bool include_gradient) const {
    if (!scaling_.active) {
        return;
    }
    if (include_gradient) {
        ev.grad *= scaling_.obj;
    }
    if (equality_rows_ > 0) {
        scale_jacobian_rows(scaling_.eq_rows, ev.Je);
    }
    if (inequality_rows_ > 0) {
        scale_jacobian_rows(scaling_.ineq_rows, ev.Ji);
    }
}

void AggregateEvalSeam::to_caller_scale(NlpEval &ev) const {
    if (!scaling_.active) {
        return;
    }
    // THE SAME REFUSAL EVERY OTHER BUNDLE-TAKING MOMENT MAKES, and for the same
    // reason: this takes a bundle the CALLER has been holding, possibly across a
    // re-lay that changed a row count, and it indexes the factor blocks by row.
    // CLAUDE.md section 4 puts the check here rather than trusting Eigen's,
    // which are compiled out under NDEBUG.
    this->require_bundle_matches_layout(ev, "to_caller_scale");
    // The exact inverse of the two functions above, and it is exact rather than
    // approximate because every factor is finite and strictly positive by
    // construction (problem_scaling.h) -- there is no divide-by-zero arm to
    // guard and no branch on magnitude.
    ev.f /= scaling_.obj;
    ev.grad /= scaling_.obj;
    if (equality_rows_ > 0) {
        ev.ce.array() /= scaling_.eq_rows.array();
        scale_jacobian_rows(scaling_.eq_rows.cwiseInverse(), ev.Je);
    }
    if (inequality_rows_ > 0) {
        ev.ci.array() /= scaling_.ineq_rows.array();
        scale_jacobian_rows(scaling_.ineq_rows.cwiseInverse(), ev.Ji);
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

    // BEFORE THE SCREEN, DELIBERATELY. A factor is finite and positive, so it
    // cannot turn a finite value non-finite by itself -- but the PRODUCT can
    // overflow, and a screen taken before the multiply would certify a bundle
    // the driver then reads as infinite. Screening the values the driver will
    // actually read is the only reading of `all_finite` that means anything.
    this->scale_values(ev);
    this->scale_derivatives(ev, /*include_gradient=*/true);

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

    // The value half only: this moment fills no derivative, and the empty
    // linearization written below is empty in BOTH spaces.
    this->scale_values(ev);
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
    // The derivative half only: this moment deliberately does not re-evaluate
    // the values, which were scaled when they were produced.
    this->scale_derivatives(ev, /*include_gradient=*/true);
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
    // NOT the gradient: this moment fills none, and the bundle's gradient block
    // was scaled by whichever moment did fill it.
    this->scale_derivatives(ev, /*include_gradient=*/false);
}

QpProblem AggregateEvalSeam::build_subproblem(const NlpEval &ev, const Vec &x, const Vec &lambda_e,
                                              const Vec &lambda_i, double obj_scale) {
    this->relay_if_stale();
    this->require_bundle_matches_layout(ev, "build_subproblem");

    if (scaling_.active) {
        // THE ONE STEP OF THE W0.2 LAYER THAT IS NOT A PLAIN MULTIPLY, and
        // problem_scaling.h's THE HESSIAN paragraph carries the derivation.
        // The provider computes `w*hess(f) + sum_j mu_j*hess(c_j)` from an
        // objective weight and a multiplier block IN ITS OWN UNITS, so to get
        // hess(L~) it must be asked for `w = obj_scale * obj` and for the
        // engine's multipliers mapped back to those units, `mu_j = s_j *
        // lambda~_j`. Exact at every weight including the restoration weight
        // 0.0, where hess(f) drops out of both sides.
        //
        // The size guards are not defensive noise: they are the same "full
        // length, or empty meaning no multipliers" contract eval_nlp documents,
        // and an empty block must stay empty rather than become a zero vector.
        const Vec mu_e = lambda_e.size() == equality_rows_
                             ? Vec(lambda_e.array() * scaling_.eq_rows.array())
                             : lambda_e;
        const Vec mu_i = lambda_i.size() == inequality_rows_
                             ? Vec(lambda_i.array() * scaling_.ineq_rows.array())
                             : lambda_i;
        this->assemble_hessian(x, mu_e, mu_i, obj_scale * scaling_.obj);
    } else {
        this->assemble_hessian(x, lambda_e, lambda_i, obj_scale);
    }

    QpProblem qp;
    this->publish_matrix(hessian_, hessian_pattern_, qp.H);
    qp.H.makeCompressed();
    // `ev.grad` ALREADY CARRIES the objective factor -- it was applied where the
    // gradient was produced -- so the layer must not appear a second time here.
    // What multiplies it is only the caller's own Lagrangian objective weight,
    // exactly as before the layer existed.
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
