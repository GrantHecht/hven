// The structure epoch as API: the strong type, the monotone counter, and the
// two ordering rules the contract states -- the bump is observable before any
// evaluation of the new structures is, and a rejected reconfiguration that
// re-laid structures bumps under that same guarantee.
//
// Everything here is pinned at the CONTRACT level, against the fake aggregate
// in support/. The live-engine versions of the same pins belong with the
// engine's own implementation of this contract.

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "hven/model/structure_identity.h"
#include "support/fake_aggregate.h"

using hven::solvers::StructureEpoch;
using hven::solvers::StructureEpochCounter;

TEST(StructureEpochType, DefaultsToNothingLaidYet) {
    const StructureEpoch epoch;
    EXPECT_EQ(epoch.value(), 0u);
    EXPECT_EQ(epoch, StructureEpoch());
}

TEST(StructureEpochType, ComparesByValueOnly) {
    EXPECT_EQ(StructureEpoch(7), StructureEpoch(7));
    EXPECT_NE(StructureEpoch(7), StructureEpoch(8));
}

TEST(StructureEpochCounterTest, StartsBeforeTheFirstLayout) {
    const StructureEpochCounter counter;
    EXPECT_EQ(counter.current(), StructureEpoch());
    EXPECT_EQ(counter.current().value(), 0u);
}

TEST(StructureEpochCounterTest, FirstBumpLandsOnOne) {
    StructureEpochCounter counter;
    EXPECT_EQ(counter.bump(), StructureEpoch(1));
    EXPECT_EQ(counter.current(), StructureEpoch(1));
}

TEST(StructureEpochCounterTest, BumpIsMonotoneAcrossManyCalls) {
    StructureEpochCounter counter;
    std::uint64_t previous = counter.current().value();
    for (int i = 0; i < 1000; ++i) {
        const std::uint64_t bumped = counter.bump().value();
        EXPECT_EQ(bumped, previous + 1);
        EXPECT_EQ(counter.current().value(), bumped);
        previous = bumped;
    }
}

TEST(StructureEpochCounterTest, CurrentDoesNotAdvanceTheCounter) {
    StructureEpochCounter counter;
    counter.bump();
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(counter.current(), StructureEpoch(1));
    }
}

// The release/acquire pairing is what makes the program-order obligation hold
// across threads as well. A reader can legally miss a bump; it may never
// observe the counter going BACKWARDS, and it may never observe a value the
// writer has not reached.
TEST(StructureEpochCounterTest, ConcurrentReadsNeverObserveARegression) {
    StructureEpochCounter counter;
    constexpr int kBumps = 20000;
    std::atomic<bool> done{false};
    std::atomic<bool> regressed{false};
    std::atomic<bool> overshot{false};

    std::thread reader([&] {
        std::uint64_t last = 0;
        while (!done.load(std::memory_order_acquire)) {
            const std::uint64_t seen = counter.current().value();
            if (seen < last) {
                regressed.store(true, std::memory_order_release);
            }
            if (seen > static_cast<std::uint64_t>(kBumps)) {
                overshot.store(true, std::memory_order_release);
            }
            last = seen;
        }
    });

    for (int i = 0; i < kBumps; ++i) {
        counter.bump();
    }
    done.store(true, std::memory_order_release);
    reader.join();

    EXPECT_FALSE(regressed.load(std::memory_order_acquire));
    EXPECT_FALSE(overshot.load(std::memory_order_acquire));
    EXPECT_EQ(counter.current().value(), static_cast<std::uint64_t>(kBumps));
}

// ---------------------------------------------------------------------------
// The aggregate-level rules, against the fake.
// ---------------------------------------------------------------------------

namespace {

using hven::model_tests::FakeAggregate;

/// Runs one evaluation and reports what the aggregate saw from inside it.
void evaluate_once(FakeAggregate &aggregate) {
    hven::Vec x = hven::Vec::Zero(FakeAggregate::kPrimalVars);
    aggregate.probe_identity(x);
}

} // namespace

TEST(AggregateEpochContract, TheFirstLayoutLeavesTheNothingLaidYetValue) {
    FakeAggregate aggregate;
    EXPECT_NE(aggregate.structure_epoch(), StructureEpoch());
}

TEST(AggregateEpochContract, ARelayIsVisibleBeforeAnyEvaluationOfTheNewStructures) {
    FakeAggregate aggregate;
    for (int round = 0; round < 5; ++round) {
        const StructureEpoch before = aggregate.structure_epoch();
        aggregate.relay_structures();
        const StructureEpoch after = aggregate.structure_epoch();
        EXPECT_NE(before, after);

        evaluate_once(aggregate);
        // No window in which an evaluation of the NEW structures reports the
        // OLD epoch: the layout the evaluation ran against and the epoch it
        // reported agree.
        EXPECT_EQ(aggregate.epoch_seen_at_last_evaluation(), after);
        EXPECT_EQ(aggregate.layout_serial_seen_at_last_evaluation(), aggregate.layout_serial());
    }
}

TEST(AggregateEpochContract, PartitionRenegotiationBumpsEvenWithUnchangedClaims) {
    FakeAggregate aggregate;
    const StructureEpoch before = aggregate.structure_epoch();

    // Renegotiating to the count already adopted still re-lays the arenas, so
    // it still bumps: claim ORDER moved even though the claim structure did not.
    const int adopted = aggregate.negotiate_partition_count(aggregate.adopted_partitions());
    EXPECT_EQ(adopted, aggregate.adopted_partitions());
    EXPECT_NE(aggregate.structure_epoch(), before);
}

TEST(AggregateEpochContract, NegotiationReportsTheAdoptedCountNotTheRequest) {
    FakeAggregate aggregate;
    const int adopted = aggregate.negotiate_partition_count(FakeAggregate::kMaxPartitions + 4);
    EXPECT_EQ(adopted, FakeAggregate::kMaxPartitions);
    EXPECT_EQ(aggregate.declaration().partition_count_, adopted);
}

TEST(AggregateEpochContract, ARejectedReconfigurationBumpsTheEpoch) {
    FakeAggregate aggregate;
    const StructureEpoch before = aggregate.structure_epoch();
    const int layout_before = aggregate.layout_serial();

    EXPECT_THROW(aggregate.reconfigure_and_reject(), std::invalid_argument);

    // The structures on hand are the ones that were restored, not the ones the
    // consumer last saw -- restoring is itself a structural event.
    EXPECT_EQ(aggregate.layout_serial(), layout_before);
    EXPECT_NE(aggregate.structure_epoch(), before);
}

TEST(AggregateEpochContract, RestoredStructuresAreVisibleUnderTheNewEpoch) {
    FakeAggregate aggregate;
    EXPECT_THROW(aggregate.reconfigure_and_reject(), std::invalid_argument);
    const StructureEpoch after_restore = aggregate.structure_epoch();

    evaluate_once(aggregate);
    EXPECT_EQ(aggregate.epoch_seen_at_last_evaluation(), after_restore);
}

TEST(AggregateEpochContract, EveryStructuralEventAdvancesTheEpochOnceMore) {
    FakeAggregate aggregate;
    std::uint64_t previous = aggregate.structure_epoch().value();
    for (int round = 0; round < 4; ++round) {
        aggregate.relay_structures();
        const std::uint64_t now = aggregate.structure_epoch().value();
        EXPECT_GT(now, previous);
        previous = now;
    }
}
