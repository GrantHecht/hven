// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Default-drift tripwire for InteriorPointSolver::apply_preset()'s `classic`
// entry (interior_point_solver_presets.h). `classic` is pinned there as
// literals -- not read off a default-constructed Settings{} -- so the preset
// keeps its documented mechanism meaning even if a future change moves one of
// Settings{}'s own defaults. This test is the other half of that guarantee:
// it compares apply_preset("classic")'s result against a FRESH
// default-constructed Settings{} over the same nine globalization fields. If
// a future change ever moves one of those defaults, this test fails --
// forcing a conscious decision about whether `classic` should track the new
// default or keep its pinned meaning, rather than silently drifting out of
// sync with what "classic" is documented to mean.
//
// Ported from tycho's tests/cpp/solvers/test_interior_point_solver_presets.cpp
// (the file-name-uniqueness prefix there was PresetGate; kept here so the
// intent stays traceable, and because it does not collide with anything else
// in this unity-merged test binary).

#include <gtest/gtest.h>

#include "hven/detail/drivers/interior_point_solver_presets.h"
#include "hven/drivers/interior_point_solver.h"

using hven::solvers::InteriorPointSolver;
using hven::solvers::InteriorPointSolverPresetFields;

namespace {

// Field-by-field comparison against an expected InteriorPointSolverPresetFields value.
void PresetGateExpectFieldsMatch(const InteriorPointSolver::Settings &s,
                                 const InteriorPointSolverPresetFields &f) {
    EXPECT_EQ(s.acceptance_strategy_, f.acceptance_strategy_);
    EXPECT_EQ(s.merit_penalty_rule_, f.merit_penalty_rule_);
    EXPECT_EQ(s.barrier_governor_, f.barrier_governor_);
    EXPECT_EQ(s.never_monotone_, f.never_monotone_);
    EXPECT_EQ(s.restoration_mode_, f.restoration_mode_);
    EXPECT_EQ(s.inertia_mode_, f.inertia_mode_);
    EXPECT_EQ(s.max_soc_, f.max_soc_);
    EXPECT_EQ(s.ls_extended_iters_, f.ls_extended_iters_);
    EXPECT_EQ(s.watchdog_, f.watchdog_);
}

} // namespace

TEST(InteriorPointSolverPresetsTest, PresetGateClassicMatchesFreshDefaultSettings) {
    InteriorPointSolver opt;
    opt.apply_preset("classic");

    InteriorPointSolver::Settings fresh; // default-constructed -- the drift tripwire target
    InteriorPointSolverPresetFields fresh_as_fields{
        fresh.acceptance_strategy_,
        fresh.merit_penalty_rule_,
        fresh.barrier_governor_,
        fresh.never_monotone_,
        fresh.restoration_mode_,
        fresh.inertia_mode_,
        fresh.max_soc_,
        fresh.ls_extended_iters_,
        fresh.watchdog_,
    };
    PresetGateExpectFieldsMatch(opt.settings(), fresh_as_fields);
}
