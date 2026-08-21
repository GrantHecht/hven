// A minimal NlpAggregate implementation, written against the Level 2 contract
// surface alone. It exists so the contract's own semantics -- the structure
// epoch's ordering guarantee, the failure-restore rule, and the request
// masking rule -- can be pinned WITHOUT the partitioned evaluation engine.
//
// It is not a solver and computes nothing meaningful: every arena a request
// names is filled with a recognizable marker so a test can tell "written" from
// "left alone". The live-engine versions of these pins arrive when the engine
// implements this contract.

#pragma once

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include <fmt/format.h>

#include "hven/core/types.h"
#include "hven/model/nlp_aggregate.h"

namespace hven::model_tests {

using hven::ConstVecRef;
using hven::Vec;
using hven::solvers::AggregateCapability;
using hven::solvers::AggregateDeclaration;
using hven::solvers::CandidateFirstOrder;
using hven::solvers::CandidatePoint;
using hven::solvers::CandidateValues;
using hven::solvers::EvalRequest;
using hven::solvers::IdentityProbe;
using hven::solvers::KktScatterView;
using hven::solvers::ModelStructureKey;
using hven::solvers::NlpAggregate;
using hven::solvers::RhsScatterView;
using hven::solvers::StructureEpoch;

/// The marker every filled slot receives. Distinct from the sentinel a test
/// pre-fills its buffers with, so "this arena was written" is observable.
inline constexpr double kFillMarker = 7.5;

/// The sentinel a test pre-fills destination storage with.
inline constexpr double kUntouchedSentinel = -101.25;

class FakeAggregate final : public NlpAggregate {
  public:
    static constexpr int kPrimalVars = 4;
    static constexpr int kEqualityRows = 2;
    static constexpr int kInequalityRows = 3;
    static constexpr int kMaxPartitions = 3;

    FakeAggregate() {
        declaration_.primal_vars_ = kPrimalVars;
        declaration_.equality_rows_ = kEqualityRows;
        declaration_.inequality_rows_ = kInequalityRows;
        declaration_.partition_count_ = adopted_partitions_;
        // A claim stream in the shape a claim arena holds one: two index
        // arrays, filled in claim order.
        claim_rows_.resize(4);
        claim_rows_ << 0, 1, 1, 2;
        claim_cols_.resize(4);
        claim_cols_ << 0, 0, 1, 2;
        // The first layout: a provider lays structures before it can be
        // evaluated, and that first lay is a structural event like any other.
        this->relay_structures();
    }

    const AggregateDeclaration &declaration() const override { return declaration_; }

    int negotiate_partition_count(int requested) override {
        // Refused, not corrected: capping is this method's job and is reported
        // through the return value, while a non-positive request names no
        // partitioning at all.
        if (requested < 1) {
            throw std::invalid_argument(fmt::format(
                "FakeAggregate: a partition count must be at least 1 (got {0})", requested));
        }
        const int adopted = std::min(requested, kMaxPartitions);
        adopted_partitions_ = adopted;
        declaration_.partition_count_ = adopted;
        // A renegotiation re-lays the arenas even when the claim structure is
        // untouched -- claim ORDER moves, so a consumer's slot-indexed state is
        // stale. The re-lay bumps.
        this->relay_structures();
        return adopted;
    }

    int evaluation_threads() const override { return threads_; }
    void set_evaluation_threads(int n) override { threads_ = n; }

    ModelStructureKey model_structure_key() const override { return key_; }
    AggregateCapability capabilities() const override { return capabilities_; }

    /// Routed through the public values entry, so it inherits that entry's
    /// validation instead of repeating it -- which is what "a probe is a values
    /// evaluation plus a hash" comes to once the entries are non-virtual.
    IdentityProbe probe_identity(ConstVecRef x) override;

    // ---- test hooks -------------------------------------------------------

    /// Re-lays the structures and re-keys them from the declaration as it now
    /// stands. The bump is the LAST thing this does, so no evaluation of the new
    /// structures is reachable under the old epoch.
    void relay_structures() {
        layout_serial_++;
        this->rekey();
        this->bump_structure_epoch();
    }

    /// A reconfiguration that re-lays the arenas and is then rejected. The
    /// restore is itself a structural event: the structures on hand are not the
    /// ones the consumer last saw, so it bumps under the same guarantee, and
    /// only then does the rejection propagate.
    void reconfigure_and_reject() {
        const int restore_to = layout_serial_;
        layout_serial_++; // the rejected lay
        layout_serial_ = restore_to;
        this->rekey();
        this->bump_structure_epoch();
        throw std::invalid_argument("FakeAggregate: reconfiguration rejected");
    }

    /// Declares one more bound record and re-materializes. Declaring a bound
    /// changes what the layout will be -- possibly only through the
    /// intersection, which is the case worth pinning -- so it re-lays.
    void declare_variable_bound(int index, double lower, double upper) {
        declaration_.variable_bounds_.push_back(hven::solvers::VariableBound{index, lower, upper});
        this->relay_structures();
    }

    void set_capabilities(AggregateCapability capabilities) { capabilities_ = capabilities; }

    /// Binds this fake's location tables to a destination, the way a provider
    /// that computes its offsets against one particular value array does.
    /// nullptr -- the default -- is the unbound provider the rest of this suite
    /// exercises, and the entry then checks nothing.
    void bind_kkt_destination(const double *destination) { bound_destination_ = destination; }
    const double *bound_kkt_destination() const override { return bound_destination_; }

    int layout_serial() const { return layout_serial_; }
    int adopted_partitions() const { return adopted_partitions_; }

    /// What structure_epoch() reported from INSIDE the last evaluation, and the
    /// layout that evaluation ran against. The ordering guarantee is the
    /// statement that these two never disagree.
    StructureEpoch epoch_seen_at_last_evaluation() const { return epoch_seen_; }
    int layout_serial_seen_at_last_evaluation() const { return layout_serial_seen_; }

    int values_calls() const { return values_calls_; }

  protected:
    // The work hooks. No validation here -- not of the request, not of the
    // point, and not of the scatter views: the public entries own all three,
    // and this fake is where "an implementation cannot skip it" gets tested. It
    // does not perform a single check of its own.
    void assemble_impl(const CandidatePoint &point, EvalRequest request, KktScatterView kkt,
                       RhsScatterView rhs) override;
    void evaluate_candidate_values_impl(const CandidatePoint &point, CandidateValues out) override;
    void evaluate_candidate_first_order_impl(const CandidatePoint &point,
                                             CandidateFirstOrder out) override;

  private:
    void record_evaluation() {
        epoch_seen_ = this->structure_epoch();
        layout_serial_seen_ = layout_serial_;
    }

    /// Recomputes the structural key from the declaration, through the two
    /// public conjunct builders plus the adopted partition count.
    void rekey();

    Eigen::VectorXi claim_rows_;
    Eigen::VectorXi claim_cols_;

    /// A named empty vector: the multiplier blocks of a values-only candidate
    /// point are legally empty, and a point's blocks must outlive the call that
    /// reads them, so this is a member rather than a temporary at the call site.
    Vec empty_multipliers_;

    AggregateDeclaration declaration_;
    ModelStructureKey key_;
    AggregateCapability capabilities_ = AggregateCapability::kNone;
    const double *bound_destination_ = nullptr;
    int adopted_partitions_ = 1;
    int threads_ = 1;
    int layout_serial_ = 0;
    int values_calls_ = 0;
    StructureEpoch epoch_seen_;
    int layout_serial_seen_ = -1;
};

} // namespace hven::model_tests
