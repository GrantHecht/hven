// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_request_destinations.cpp — the assembly contract's boundary checks, called directly
// (model/nlp_assembly.h, model/candidate_point.h, model/claim_space.h): which destination a
// request needs, which reason a missing one is refused for, the bound-destination guard, the
// location tables' construction refusals by message, the candidate-block size refusals, and
// the capability and domain-set algebra. test_aggregate_contract.cpp exercises the same rules
// through an assembling provider; here each message is pinned at its source.

#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <gtest/gtest.h>

#include <hven/model/assembly_declaration.h>
#include <hven/model/candidate_point.h>
#include <hven/model/claim_space.h>
#include <hven/model/nlp_assembly.h>

namespace hven {
namespace {

template <class F> void expect_refusal(F &&f, std::initializer_list<const char *> fragments) {
    try {
        f();
        ADD_FAILURE() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string message(e.what());
        for (const char *fragment : fragments) {
            EXPECT_NE(message.find(fragment), std::string::npos)
                << "missing '" << fragment << "' in: " << message;
        }
    }
}

// ---------------------------------------------------------------------------
// validate_request_destinations
// ---------------------------------------------------------------------------

struct Destinations {
    AssemblyDeclaration declared;
    std::vector<double> kkt_values = std::vector<double>(8, 0.0);
    std::vector<int> kkt_locations = std::vector<int>(8, 0);
    std::vector<int> clash = std::vector<int>(0);
    std::vector<std::mutex> locks = std::vector<std::mutex>(0);
    KktLocationTable kkt_table;
    std::vector<double> grad = std::vector<double>(3, 0.0);
    std::vector<int> grad_rows = {0, 1, 2};
    RhsLocationTable grad_table{grad_rows.data(), 3};
    std::vector<double> adj = std::vector<double>(3, 0.0);
    RhsLocationTable adj_table{grad_rows.data(), 3};
    std::vector<double> eq = std::vector<double>(2, 0.0);
    std::vector<int> eq_rows = {0, 1};
    RhsLocationTable eq_table{eq_rows.data(), 2};
    std::vector<double> ineq = std::vector<double>(1, 0.0);
    std::vector<int> ineq_rows = {0};
    RhsLocationTable ineq_table{ineq_rows.data(), 1};
    double objective = 0.0;

    Destinations() {
        declared.primal_vars = 3;
        declared.equality_rows = 2;
        declared.inequality_rows = 1;
        kkt_table = KktLocationTable(kkt_locations.data(), 8, clash.data(), 0, &locks);
    }
    KktScatterView kkt() { return {kkt_values.data(), 8, &kkt_table}; }
    RhsScatterView rhs() {
        RhsScatterView r;
        r.objective_gradient = {grad.data(), 3, &grad_table};
        r.constraint_adjoint_gradient = {adj.data(), 3, &adj_table};
        r.equality_residuals = {eq.data(), 2, &eq_table};
        r.inequality_residuals = {ineq.data(), 1, &ineq_table};
        r.objective = &objective;
        return r;
    }
};

TEST(RequestDestinations, EveryDestinationPresentSatisfiesTheFullKktShape) {
    Destinations d;
    EXPECT_NO_THROW(validate_request_destinations(kRequestFullKkt, d.declared, d.kkt(), d.rhs()));
}

TEST(RequestDestinations, TheObjectiveValueNeedsItsOutSlot) {
    Destinations d;
    RhsScatterView rhs = d.rhs();
    rhs.objective = nullptr;
    expect_refusal([&] { validate_request_destinations(kRequestObjectiveOnly, d.declared, d.kkt(), rhs); },
                   {"objective value", "no out slot"});
    // A request that does not name the objective value does not need the slot.
    EXPECT_NO_THROW(validate_request_destinations(kRequestConstraintJacobiansOnly, d.declared,
                                                  d.kkt(), rhs));
}

TEST(RequestDestinations, AMissingGradientArenaIsRefusedForTheReasonItIsMissing) {
    Destinations d;
    const auto refused = [&](RhsScatterView rhs, const char *reason) {
        expect_refusal(
            [&] {
                validate_request_destinations(kRequestObjectiveGradientAndConstraints, d.declared,
                                              d.kkt(), rhs);
            },
            {"objective gradient", reason});
    };
    RhsScatterView no_storage = d.rhs();
    no_storage.objective_gradient.values = nullptr;
    refused(no_storage, "supplies no storage");
    RhsScatterView no_table = d.rhs();
    no_table.objective_gradient.locations = nullptr;
    refused(no_table, "supplies no location table");
    RhsScatterView zero = d.rhs();
    zero.objective_gradient.size = 0;
    refused(zero, "rows (its length is zero)");
}

TEST(RequestDestinations, AMissingAdjointArenaIsNamedForTheAdjointRequests) {
    Destinations d;
    RhsScatterView rhs = d.rhs();
    rhs.constraint_adjoint_gradient.locations = nullptr;
    expect_refusal([&] { validate_request_destinations(kRequestFirstOrderRhs, d.declared, d.kkt(), rhs); },
                   {"constraint adjoint gradient", "location table"});
    // The adjoint arena is not needed by a values-only request.
    EXPECT_NO_THROW(validate_request_destinations(kRequestObjectiveAndConstraints, d.declared,
                                                  d.kkt(), rhs));
}

TEST(RequestDestinations, ResidualArenasAreCheckedAgainstTheDeclaredRowCounts) {
    Destinations d;
    {
        RhsScatterView rhs = d.rhs();
        rhs.equality_residuals.values = nullptr;
        expect_refusal(
            [&] { validate_request_destinations(kRequestObjectiveAndConstraints, d.declared, d.kkt(), rhs); },
            {"equality residual", "declared length is 2", "no storage"});
    }
    {
        RhsScatterView rhs = d.rhs();
        rhs.inequality_residuals.locations = nullptr;
        expect_refusal(
            [&] { validate_request_destinations(kRequestObjectiveAndConstraints, d.declared, d.kkt(), rhs); },
            {"inequality residual", "declared length is 1", "no location table"});
    }
    {
        RhsScatterView rhs = d.rhs();
        rhs.equality_residuals.size = 5;
        expect_refusal(
            [&] { validate_request_destinations(kRequestObjectiveAndConstraints, d.declared, d.kkt(), rhs); },
            {"equality residual arena's view is 5 rows", "declares 2"});
    }
    // A block that declares zero rows needs no arena at all, whatever the view holds.
    d.declared.equality_rows = 0;
    RhsScatterView rhs = d.rhs();
    rhs.equality_residuals = {};
    EXPECT_NO_THROW(
        validate_request_destinations(kRequestObjectiveAndConstraints, d.declared, d.kkt(), rhs));
}

TEST(RequestDestinations, AKktBearingRequestNeedsANonEmptyKktView) {
    Destinations d;
    const KktScatterView empty;
    for (const EvalRequest request : {kRequestFullKkt, kRequestLagrangianHessian,
                                      kRequestConstraintJacobiansOnly,
                                      kRequestConstraintResidualsAndJacobian}) {
        expect_refusal([&] { validate_request_destinations(request, d.declared, empty, d.rhs()); },
                       {"KKT-bearing output", "KKT scatter view is empty"});
    }
    // A request with no KKT-bearing flag tolerates the empty view.
    EXPECT_NO_THROW(
        validate_request_destinations(kRequestFirstOrderRhs, d.declared, empty, d.rhs()));
    // A view with storage but a zero size, or no table, is as empty as none.
    KktScatterView zero = d.kkt();
    zero.size = 0;
    EXPECT_TRUE(zero.empty());
    KktScatterView no_table = d.kkt();
    no_table.locations = nullptr;
    EXPECT_TRUE(no_table.empty());
    EXPECT_FALSE(d.kkt().empty());
}

TEST(RequestDestinations, TheBoundDestinationMustBeTheViewsValueArrayWhenBothExist) {
    Destinations d;
    EXPECT_NO_THROW(validate_bound_destination(d.kkt_values.data(), d.kkt()));
    EXPECT_NO_THROW(validate_bound_destination(nullptr, d.kkt())) << "an unbound provider accepts any";
    EXPECT_NO_THROW(validate_bound_destination(d.kkt_values.data(), KktScatterView{}))
        << "an empty view writes nothing, so there is nothing to mismatch";
    std::vector<double> other(8, 0.0);
    expect_refusal([&] { validate_bound_destination(other.data(), d.kkt()); },
                   {"KKT scatter view names value array", "bound at analysis time", "re-run the analysis"});
}

// ---------------------------------------------------------------------------
// Location tables: construction refusals by message.
// ---------------------------------------------------------------------------

TEST(LocationTables, TheKktTableRefusesEachMalformedPublicationByName) {
    std::vector<int> locations = {0, 1, 2};
    std::vector<int> clash = {-1, 0, -1};
    std::vector<std::mutex> locks(1);
    expect_refusal([&] { KktLocationTable(locations.data(), -1, clash.data(), 0, &locks); },
                   {"KktLocationTable", "negative array size", "locations -1"});
    expect_refusal([&] { KktLocationTable(locations.data(), 3, clash.data(), -2, &locks); },
                   {"negative array size", "clash marks -2"});
    expect_refusal([&] { KktLocationTable(nullptr, 3, clash.data(), 3, &locks); },
                   {"3 claim slots were published with no location array"});
    expect_refusal([&] { KktLocationTable(locations.data(), 3, nullptr, 3, &locks); },
                   {"3 columns were published with no clash-mark array"});
    clash = {-1, -2, -1};
    expect_refusal([&] { KktLocationTable(locations.data(), 3, clash.data(), 3, &locks); },
                   {"column 1 carries clash mark -2", "only negative mark"});
    clash = {-1, 1, -1};
    expect_refusal([&] { KktLocationTable(locations.data(), 3, clash.data(), 3, &locks); },
                   {"column 1", "lock index 1", "1 lock"});
    expect_refusal([&] { KktLocationTable(locations.data(), 3, clash.data(), 3, nullptr); },
                   {"lock index 1", "0 lock"});
}

TEST(LocationTables, ALegalKktTableReadsBackWhatItWasGiven) {
    std::vector<int> locations = {4, 5, -1};
    std::vector<int> clash = {-1, 0};
    std::vector<std::mutex> locks(1);
    const KktLocationTable table(locations.data(), 3, clash.data(), 2, &locks);
    EXPECT_EQ(table.size(), 3);
    EXPECT_EQ(table.clash_size(), 2);
    EXPECT_FALSE(table.empty());
    EXPECT_EQ(table.location(0), 4);
    EXPECT_EQ(table.location(2), -1);
    EXPECT_EQ(table.clash_lock(0), -1);
    EXPECT_EQ(table.clash_lock(1), 0);
    EXPECT_EQ(&table.lock(0), &locks[0]);
    EXPECT_TRUE(KktLocationTable().empty());
    EXPECT_EQ(KktLocationTable().clash_size(), 0);
}

TEST(LocationTables, TheRhsTableRefusesEachMalformedPublicationByName) {
    std::vector<int> rows = {0, 1};
    expect_refusal([&] { RhsLocationTable(rows.data(), -3); }, {"RhsLocationTable", "negative array size (-3)"});
    expect_refusal([&] { RhsLocationTable(nullptr, 2); },
                   {"2 claim slots were published with no row array"});
    rows = {0, -2};
    expect_refusal([&] { RhsLocationTable(rows.data(), 2); },
                   {"claim slot 1 maps to row -2"});
    rows = {0, -1}; // the dropped-row sentinel is legal
    const RhsLocationTable ok(rows.data(), 2);
    EXPECT_EQ(ok.size(), 2);
    EXPECT_EQ(ok.location(1), -1);
    EXPECT_FALSE(ok.empty());
    EXPECT_TRUE(RhsLocationTable().empty());
}

// ---------------------------------------------------------------------------
// Candidate blocks.
// ---------------------------------------------------------------------------

TEST(CandidateBlocks, EveryBlockSizeRefusalNamesTheBlockAndBothNumbers) {
    Vec x = Vec::Zero(3), le = Vec::Zero(2), li = Vec::Zero(1);
    const Vec none(0);
    expect_refusal([&] { validate_candidate_point(CandidatePoint{x, none, none, 1.0}, 4, 2, 1); },
                   {"CandidatePoint", "primal block has 3 rows"});
    expect_refusal([&] { validate_candidate_point(CandidatePoint{x, Vec::Zero(1), none, 1.0}, 3, 2, 1); },
                   {"equality-multiplier block has 1 rows"});
    expect_refusal([&] { validate_candidate_point(CandidatePoint{x, none, Vec::Zero(4), 1.0}, 3, 2, 1); },
                   {"inequality-multiplier block has 4 rows"});
    // Empty multiplier blocks are legal for a values-only read; exact ones are legal too.
    EXPECT_NO_THROW(validate_candidate_point(CandidatePoint{x, none, none, 1.0}, 3, 2, 1));
    EXPECT_NO_THROW(validate_candidate_point(CandidatePoint{x, le, li, 1.0}, 3, 2, 1));

    double f = 0.0;
    Vec eq = Vec::Zero(2), iq = Vec::Zero(1), g = Vec::Zero(3), a = Vec::Zero(3);
    expect_refusal([&] { validate_candidate_values({f, eq, iq}, 3, 1); },
                   {"CandidateValues", "equality-residual block has 2 rows"});
    expect_refusal([&] { validate_candidate_values({f, eq, iq}, 2, 2); },
                   {"inequality-residual block has 1 rows"});
    EXPECT_NO_THROW(validate_candidate_values({f, eq, iq}, 2, 1));
    expect_refusal([&] { validate_candidate_first_order({{f, eq, iq}, g, a}, 4, 2, 1); },
                   {"CandidateFirstOrder", "objective-gradient block has 3 rows"});
    Vec short_adj = Vec::Zero(2);
    expect_refusal([&] { validate_candidate_first_order({{f, eq, iq}, g, short_adj}, 3, 2, 1); },
                   {"constraint-adjoint", "has 2 rows"});
    EXPECT_NO_THROW(validate_candidate_first_order({{f, eq, iq}, g, a}, 3, 2, 1));
}

TEST(CandidateBlocks, FullMultipliersAreRequiredWhereTheyAreRead) {
    Vec x = Vec::Zero(3), le = Vec::Zero(2), li = Vec::Zero(1);
    const Vec none(0);
    expect_refusal([&] { validate_full_multipliers(CandidatePoint{x, none, li, 1.0}, 2, 1); },
                   {"equality-multiplier block must carry all 2 rows", "has 0"});
    expect_refusal([&] { validate_full_multipliers(CandidatePoint{x, le, none, 1.0}, 2, 1); },
                   {"inequality-multiplier block must carry all 1 rows", "has 0"});
    EXPECT_NO_THROW(validate_full_multipliers(CandidatePoint{x, le, li, 1.0}, 2, 1));
}

TEST(CandidateBlocks, TheValueDigestSeparatesObjectiveAndTheTwoBlocks) {
    double f = 1.5;
    Vec eq(2), iq(1);
    eq << 1.0, 2.0;
    iq << 3.0;
    const std::uint64_t base = candidate_value_digest({f, eq, iq});
    EXPECT_EQ(base, candidate_value_digest({f, eq, iq}));
    double f2 = 1.5000000001;
    EXPECT_NE(base, candidate_value_digest({f2, eq, iq}));
    Vec eq2 = eq;
    eq2(1) = 2.0000000001;
    EXPECT_NE(base, candidate_value_digest({f, eq2, iq}));
    // The block lengths are part of the digest: moving a value across the boundary changes it.
    Vec eq3(1), iq3(2);
    eq3 << 1.0;
    iq3 << 2.0, 3.0;
    EXPECT_NE(base, candidate_value_digest({f, eq3, iq3}));
}

// ---------------------------------------------------------------------------
// Capability and domain algebra.
// ---------------------------------------------------------------------------

TEST(AssemblyCapabilityAlgebra, UnionIntersectionAndProbe) {
    AssemblyCapability c = AssemblyCapability::kNone;
    EXPECT_TRUE(has_capability(c, AssemblyCapability::kNone));
    EXPECT_FALSE(has_capability(c, AssemblyCapability::kDirectScatter));
    c |= AssemblyCapability::kDirectScatter;
    EXPECT_TRUE(has_capability(c, AssemblyCapability::kDirectScatter));
    EXPECT_FALSE(has_capability(c, AssemblyCapability::kDirectScatter | AssemblyCapability::kValuesFastPath));
    c = c | AssemblyCapability::kValuesFastPath;
    EXPECT_TRUE(has_capability(c, AssemblyCapability::kDirectScatter | AssemblyCapability::kValuesFastPath));
    c &= AssemblyCapability::kValuesFastPath;
    EXPECT_FALSE(has_capability(c, AssemblyCapability::kDirectScatter));
    EXPECT_EQ(c & AssemblyCapability::kDirectScatter, AssemblyCapability::kNone);
}

TEST(ClaimDomainSetAlgebra, BitsReflectMembershipAndInPlaceIntersectionNarrows) {
    ClaimDomainSet set;
    EXPECT_EQ(set.bits(), 0u);
    set |= ClaimDomain::kHessian | ClaimDomain::kEqualityJacobian;
    EXPECT_EQ(set.bits(), 0b0011u);
    EXPECT_TRUE(set.contains(ClaimDomain::kEqualityJacobian));
    EXPECT_FALSE(set.contains(ClaimDomain::kVariableBound));
    set &= ClaimDomainSet(ClaimDomain::kEqualityJacobian);
    EXPECT_EQ(set.bits(), 0b0010u);
    set &= ClaimDomainSet(ClaimDomain::kVariableBound);
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(ClaimDomainSet::all().bits(), 0b1111u);
}

} // namespace
} // namespace hven
