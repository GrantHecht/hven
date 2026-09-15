// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Default-drift tripwire for ipm_preset()'s `classic` entry
// (ipm_solver_presets.h). `classic` is pinned there as
// literals -- not read off a default-constructed IpmOptions{} -- so the preset
// keeps its documented mechanism meaning even if a future change moves one of
// IpmOptions{}'s own defaults. This test is the other half of that guarantee:
// it compares ipm_preset("classic")'s result against a FRESH
// default-constructed IpmOptions{} over the same nine globalization fields. If
// a future change ever moves one of those defaults, this test fails --
// forcing a conscious decision about whether `classic` should track the new
// default or keep its pinned meaning, rather than silently drifting out of
// sync with what "classic" is documented to mean.
//
// Ported from tycho's tests/cpp/solvers/test_ipm_solver_presets.cpp
// (the file-name-uniqueness prefix there was PresetGate; kept here so the
// intent stays traceable, and because it does not collide with anything else
// in this unity-merged test binary).

#include <gtest/gtest.h>

#include "hven/detail/drivers/ipm_solver_presets.h"
#include "hven/drivers/ipm_solver_types.h"

using hven::solvers::InteriorPointSolverPresetFields;
using hven::solvers::IpmOptions;

namespace {

// Field-by-field comparison against an expected InteriorPointSolverPresetFields value.
void PresetGateExpectFieldsMatch(const IpmOptions &s, const InteriorPointSolverPresetFields &f) {
    EXPECT_EQ(s.acceptance_strategy, f.acceptance_strategy_);
    EXPECT_EQ(s.merit_penalty_rule, f.merit_penalty_rule_);
    EXPECT_EQ(s.barrier_governor, f.barrier_governor_);
    EXPECT_EQ(s.never_monotone, f.never_monotone_);
    EXPECT_EQ(s.restoration_mode, f.restoration_mode_);
    EXPECT_EQ(s.inertia_mode, f.inertia_mode_);
    EXPECT_EQ(s.max_soc, f.max_soc_);
    EXPECT_EQ(s.ls_extended_iters, f.ls_extended_iters_);
    EXPECT_EQ(s.watchdog, f.watchdog_);
}

} // namespace

TEST(InteriorPointSolverPresetsTest, PresetGateClassicMatchesFreshDefaultSettings) {
    const IpmOptions classic = hven::solvers::ipm_preset("classic");

    IpmOptions fresh; // default-constructed -- the drift tripwire target
    InteriorPointSolverPresetFields fresh_as_fields{
        fresh.acceptance_strategy,
        fresh.merit_penalty_rule,
        fresh.barrier_governor,
        fresh.never_monotone,
        fresh.restoration_mode,
        fresh.inertia_mode,
        fresh.max_soc,
        fresh.ls_extended_iters,
        fresh.watchdog,
    };
    PresetGateExpectFieldsMatch(classic, fresh_as_fields);
}
