// The function-entry seam: what SolverInterfaceAdapter<T> routes into
// ConstraintInterface / ObjectiveInterface, and what it refuses to.
//
// The refusals are compile-time, so they are covered by the compile-fail
// probes in compile_fail/ (wired in tests/interior/CMakeLists.txt) rather
// than here. What this file pins is the acceptance side and, crucially, the
// STORED DYNAMIC TYPE -- the observation that distinguishes one virtual
// dispatch from two. Sizes and numerics cannot tell those apart; typeid can.

#include <typeinfo>

#include <gtest/gtest.h>

#include "adapter_fixture_functions.h"

using adapter_fixture::StubConstraintOnly;
using adapter_fixture::StubDefectFamily;
using adapter_fixture::StubScalarObjective;
using adapter_fixture::StubUnregistered;
using hven::solvers::ConstraintInterface;
using hven::solvers::ConstraintModel;
using hven::solvers::ObjectiveInterface;
using hven::solvers::ObjectiveModel;
using hven::solvers::SolverInterfaceAdapter;

// The primary template must be safe to INSTANTIATE and inspect. Only calling
// an install function on an unregistered type is a diagnostic, so this probe
// has to compile -- if the primary template's asserts were not body-local,
// this line alone would fail the build.
TEST(SolverInterfaceAdapter, PrimaryTemplateIsInspectableForAnUnregisteredType) {
    EXPECT_FALSE(SolverInterfaceAdapter<StubUnregistered>::registered);
}

TEST(SolverInterfaceAdapter, RegisteredTypesReportRegistered) {
    EXPECT_TRUE(SolverInterfaceAdapter<StubScalarObjective>::registered);
    EXPECT_TRUE(SolverInterfaceAdapter<StubConstraintOnly>::registered);
}

// The constrained partial specialization selects on its constraint, not just
// on the class template argument: a family member that fails the constraint
// falls back to the unregistered primary template.
TEST(SolverInterfaceAdapter, ConstrainedPartialSpecializationSelectsOnItsConstraint) {
    EXPECT_TRUE(SolverInterfaceAdapter<StubDefectFamily<3>>::registered);
    EXPECT_TRUE(SolverInterfaceAdapter<StubDefectFamily<1>>::registered);
    EXPECT_FALSE(SolverInterfaceAdapter<StubDefectFamily<0>>::registered);
    EXPECT_FALSE(SolverInterfaceAdapter<StubDefectFamily<-1>>::registered);
}

TEST(SolverInterfaceAdapter, RegisteredFamilyMemberIsStoredAsItself) {
    const StubDefectFamily<3> f{};
    const ConstraintInterface ci(f);

    // One erasure: the stored object is the function itself, so exactly one
    // virtual dispatch stands between a solver call and the function's body.
    EXPECT_EQ(typeid(ci.storage_.get()), typeid(ConstraintModel<StubDefectFamily<3>>));
    EXPECT_EQ(ci.output_rows(), 3);
}

TEST(SolverInterfaceAdapter, RegisteredConstraintOnlyTypeEntersTheConstraintInterface) {
    const StubConstraintOnly f{};
    const ConstraintInterface ci(f);
    EXPECT_EQ(typeid(ci.storage_.get()), typeid(ConstraintModel<StubConstraintOnly>));
}

TEST(SolverInterfaceAdapter, RegisteredScalarObjectiveIsStoredAsItself) {
    const StubScalarObjective f{};
    const ObjectiveInterface oi(f);
    EXPECT_EQ(typeid(oi.storage_.get()), typeid(ObjectiveModel<StubScalarObjective>));
}

// Copying an interface must reproduce the stored dynamic type, not collapse
// it back to the base: clone_into() is the only route by which the SBO
// container can preserve it.
TEST(SolverInterfaceAdapter, CopyingAnInterfacePreservesTheStoredDynamicType) {
    const StubDefectFamily<2> f{};
    const ConstraintInterface ci(f);
    const ConstraintInterface copy = ci;
    EXPECT_EQ(typeid(copy.storage_.get()), typeid(ConstraintModel<StubDefectFamily<2>>));
}
