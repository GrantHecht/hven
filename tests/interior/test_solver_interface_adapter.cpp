// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

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

using adapter_fixture::StubAdapterMissingObjective;
using adapter_fixture::StubConstraintOnly;
using adapter_fixture::StubDefectFamily;
using adapter_fixture::StubHandle;
using adapter_fixture::StubMixinConstraintOnly;
using adapter_fixture::StubMixinObjectiveOnly;
using adapter_fixture::StubPartialObjective;
using adapter_fixture::StubPlainObjective;
using adapter_fixture::StubScalarObjective;
using adapter_fixture::StubUnregistered;
using hven::solvers::ConstraintInterface;
using hven::solvers::ConstraintModel;
using hven::solvers::ObjectiveInterface;
using hven::solvers::ObjectiveModel;
using hven::solvers::SolverInterfaceAdapter;

// The primary template is the default policy and is safe to INSTANTIATE and
// inspect: `registered` reads false for a type no specialization names.
TEST(SolverInterfaceAdapter, PrimaryTemplateIsInspectableForAnUnregisteredType) {
    EXPECT_FALSE(SolverInterfaceAdapter<StubUnregistered>::registered);
    EXPECT_FALSE(SolverInterfaceAdapter<StubPlainObjective>::registered);
    EXPECT_FALSE(SolverInterfaceAdapter<StubHandle>::registered);
}

// A plain type needs no specialization: the default adapter stores it as itself,
// one erasure and one virtual dispatch per evaluation call (AP-2 B6, friction #20).
TEST(SolverInterfaceAdapter, AnUnregisteredPlainTypeIsStoredAsItself) {
    const StubUnregistered f{};
    const ConstraintInterface ci(f);
    const auto &stored = ci.storage_.get();
    EXPECT_EQ(typeid(stored), typeid(ConstraintModel<StubUnregistered>));
}

// An objective piece needs no constraint surface and no output count (AP-2 B6,
// friction #19); the handle reports the one output itself.
TEST(SolverInterfaceAdapter, AnObjectiveWithoutAConstraintSurfaceIsStoredAsItself) {
    const StubPlainObjective f{};
    const ObjectiveInterface oi(f);
    const auto &stored = oi.storage_.get();
    EXPECT_EQ(typeid(stored), typeid(ObjectiveModel<StubPlainObjective>));
    EXPECT_EQ(oi.output_rows(), 1);
    EXPECT_EQ(oi.input_rows(), 1);
}

// The single-dispatch pin for a handle with the hooks: the default adapter
// detects them, and the stored dynamic type is the CONCRETE payload's model --
// not a model of the handle -- on both routes. The handle exposes nothing of the
// piece surface, so no evaluation can dispatch through it: one virtual call per
// evaluation, the stored model's.
TEST(SolverInterfaceAdapter, AHookTypeStillSingleDispatches) {
    const StubHandle as_constraint{
        std::make_shared<StubHandle::Model<StubDefectFamily<2>>>(StubDefectFamily<2>{})};
    const ConstraintInterface ci(as_constraint);
    const auto &stored_constraint = ci.storage_.get();
    EXPECT_EQ(typeid(stored_constraint), typeid(ConstraintModel<StubDefectFamily<2>>));
    EXPECT_EQ(ci.output_rows(), 2);

    const StubHandle as_objective{
        std::make_shared<StubHandle::Model<StubScalarObjective>>(StubScalarObjective{})};
    const ObjectiveInterface oi(as_objective);
    const auto &stored_objective = oi.storage_.get();
    EXPECT_EQ(typeid(stored_objective), typeid(ObjectiveModel<StubScalarObjective>));
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

// The mixin route is a real registration, not a way of opting out of one: the
// direction it DOES support must still work, and must still store directly.
TEST(SolverInterfaceAdapter, MixinDeclaredConstraintOnlyTypeStillEntersTheConstraintInterface) {
    EXPECT_TRUE(SolverInterfaceAdapter<StubMixinConstraintOnly>::registered);

    const StubMixinConstraintOnly f{};
    const ConstraintInterface ci(f);
    EXPECT_EQ(typeid(ci.storage_.get()), typeid(ConstraintModel<StubMixinConstraintOnly>));
}

// The mirror half-policy, and the reason it needs its own case rather than
// riding on the one above: `registered` is inherited from the mixin, so a
// type could report registered while its supported route silently failed to
// store directly. The typeid is what says the objective route really did the
// one-erasure thing. The refusal on the other route is the constraint_unsupported_mixin
// compile-fail probe's job -- it cannot be observed from here.
TEST(SolverInterfaceAdapter, MixinDeclaredObjectiveOnlyTypeStillEntersTheObjectiveInterface) {
    EXPECT_TRUE(SolverInterfaceAdapter<StubMixinObjectiveOnly>::registered);

    const StubMixinObjectiveOnly f{};
    const ObjectiveInterface oi(f);
    EXPECT_EQ(typeid(oi.storage_.get()), typeid(ObjectiveModel<StubMixinObjectiveOnly>));
}

// Same for the adapter that merely omits install_objective: its constraint
// route is unaffected, and only the objective route is diagnosed (by the
// compile-fail probe -- an omission cannot be observed from here).
TEST(SolverInterfaceAdapter, AdapterMissingAnInstallStillServesTheOneItHas) {
    const StubAdapterMissingObjective f{};
    const ConstraintInterface ci(f);
    EXPECT_EQ(typeid(ci.storage_.get()), typeid(ConstraintModel<StubAdapterMissingObjective>));
}

// The surface probes must agree with what the Models actually call -- that
// correspondence is what lets the install bodies refuse a type before
// instantiating a Model on it. These pin the discriminating cases the
// compile-fail probes exercise from the other side.
TEST(SolverInterfaceAdapter, SurfaceProbesMatchWhatTheModelsRequire) {
    using hven::solvers::SolverConstraintFunction;
    using hven::solvers::SolverObjectiveFunction;
    using hven::solvers::SolverObjectiveSurface;

    // Every fixture carries the full constraint surface; that is not what
    // separates them.
    EXPECT_TRUE(SolverConstraintFunction<StubScalarObjective>);
    EXPECT_TRUE(SolverConstraintFunction<StubConstraintOnly>);
    EXPECT_TRUE(SolverConstraintFunction<StubPartialObjective>);

    EXPECT_TRUE(SolverObjectiveSurface<StubScalarObjective>);
    EXPECT_FALSE(SolverObjectiveSurface<StubConstraintOnly>);
    // The case a probe of objective() alone would have waved through.
    EXPECT_FALSE(SolverObjectiveSurface<StubPartialObjective>);

    EXPECT_TRUE(SolverObjectiveFunction<StubScalarObjective>);
    EXPECT_FALSE(SolverObjectiveFunction<StubPartialObjective>);
    // An objective needs the piece surface and the objective members, nothing else.
    EXPECT_TRUE(SolverObjectiveFunction<StubPlainObjective>);
    EXPECT_FALSE(SolverConstraintFunction<StubPlainObjective>);
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
