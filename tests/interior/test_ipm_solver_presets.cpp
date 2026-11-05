// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// THE DEFAULT-DRIFT TRIPWIRE FIRED, AND THIS FILE IS THE DECISION IT ASKED FOR.
//
// Until M7 W2 D1 (2026-09-18) this file asserted that ipm_preset("classic")
// equalled a fresh default-constructed IpmOptions{} over the nine globalization
// fields, with a header saying that if a future change ever moved one of those
// defaults the failure would force "a conscious decision about whether `classic`
// should track the new default or keep its pinned meaning, rather than silently
// drifting out of sync with what `classic` is documented to mean".
//
// That change is W2 D1, the declared IPM-default re-derivation event
// (CLAUDE.md section 7; docs/notes/data/2026-09-m7-w2-d1-ipm-default/). Three
// IpmOptions defaults moved:
//
//     acceptance_mode   classic_merit    -> filter
//     barrier_governor_mode      classic_adaptive -> monitored
//     max_iters             500              -> 1000
//
// THE DECISION: `classic` KEEPS ITS PINNED MEANING. It is the pre-2026-09-18
// default, not "whatever the struct currently defaults to", and
// ipm_solver_presets.h says so in those words. That is what makes the old
// default REACHABLE rather than merely remembered, and reachability is the gate
// this event ships on -- so the tripwire is not deleted, it is INVERTED: it now
// pins the difference, exactly, in both directions.
//
// Two assertions carry that, and they are deliberately different instruments:
//
//   1. FIELD BY FIELD over the nine preset fields (PresetGateClassicDiffersFrom
//      TheDefaultInExactlyTheTwoGlobalizationFields). Sharp about WHICH fields,
//      blind to everything outside the preset surface.
//   2. DUMP LINE BY DUMP LINE over the WHOLE struct (PresetGateClassicPlusFive
//      HundredReconstructsThePreEventDefault). dump_options() prints every
//      field in declaration order, one `name = value` line each, so diffing two
//      dumps is diffing two configurations in full -- all sixty-odd fields, not
//      just the nine. This is the OFF proof the event's brief asks for: the old
//      default remains reachable, bit-identically, as ipm_preset("classic")
//      with max_iters = 500, and the diff against IpmOptions{} is EXACTLY the
//      three lines the event moved.
//
// Ported from tycho's tests/cpp/solvers/test_ipm_solver_presets.cpp
// (the file-name-uniqueness prefix there was PresetGate; kept here so the
// intent stays traceable, and because it does not collide with anything else
// in this unity-merged test binary).

#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "hven/detail/drivers/ipm_solver_presets.h"
#include "hven/drivers/ipm_solver_types.h"

using hven::AcceptanceMode;
using hven::BarrierGovernorMode;
using hven::InteriorPointSolverPresetFields;
using hven::IpmOptions;

namespace {

// The nine preset fields of an IpmOptions value, in the preset table's order.
InteriorPointSolverPresetFields preset_fields_of(const IpmOptions &s) {
    return InteriorPointSolverPresetFields{
        s.advanced.acceptance_mode,
        s.advanced.merit_penalty_rule,
        s.advanced.barrier_governor_mode,
        s.advanced.never_monotone,
        s.advanced.restoration_mode,
        s.advanced.inertia_mode,
        s.advanced.max_second_order_corrections,
        s.advanced.max_extended_backtracks,
        s.advanced.watchdog,
    };
}

// The value IpmOptions{} carried before M7 W2 D1, built the way a CONSUMER
// would have to build it today: the preset for the nine globalization fields,
// and the one non-preset field the event also moved written on top. Nothing
// else -- if reconstructing the old default needed a tenth write, this helper
// would have to make it and the dump diff below would say so.
IpmOptions pre_d1_default() {
    IpmOptions o = hven::ipm_preset("classic");
    o.common.max_iterations = 500;
    return o;
}

std::string dump_to_string(const IpmOptions &o) {
    std::ostringstream os;
    hven::dump_options(o, os);
    return os.str();
}

std::vector<std::string> lines_of(const std::string &text) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t end = text.find('\n', start);
        if (end == std::string::npos) {
            out.push_back(text.substr(start));
            break;
        }
        out.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    return out;
}

// Field-by-field comparison against an expected InteriorPointSolverPresetFields value.
void PresetGateExpectFieldsMatch(const IpmOptions &s, const InteriorPointSolverPresetFields &f) {
    EXPECT_EQ(s.advanced.acceptance_mode, f.acceptance_strategy_);
    EXPECT_EQ(s.advanced.merit_penalty_rule, f.merit_penalty_rule_);
    EXPECT_EQ(s.advanced.barrier_governor_mode, f.barrier_governor_);
    EXPECT_EQ(s.advanced.never_monotone, f.never_monotone_);
    EXPECT_EQ(s.advanced.restoration_mode, f.restoration_mode_);
    EXPECT_EQ(s.advanced.inertia_mode, f.inertia_mode_);
    EXPECT_EQ(s.advanced.max_second_order_corrections, f.max_soc_);
    EXPECT_EQ(s.advanced.max_extended_backtracks, f.ls_extended_iters_);
    EXPECT_EQ(s.advanced.watchdog, f.watchdog_);
}

} // namespace

// Instrument 1: the nine preset fields. `classic` and the live default now
// differ, and they differ in EXACTLY the two globalization fields W2 D1 moved
// -- the other seven still match, which is what says the event was two fields
// wide on this surface and not three or nine.
TEST(InteriorPointSolverPresetsTest,
     PresetGateClassicDiffersFromTheDefaultInExactlyTheTwoGlobalizationFields) {
    const IpmOptions classic = hven::ipm_preset("classic");
    const IpmOptions fresh; // default-constructed -- the post-W2-D1 default

    // The two that moved, asserted in both directions so a future revert of
    // either side fails here rather than passing quietly.
    EXPECT_EQ(classic.advanced.acceptance_mode, AcceptanceMode::kClassicMerit);
    EXPECT_EQ(fresh.advanced.acceptance_mode, AcceptanceMode::kFilter);
    EXPECT_EQ(classic.advanced.barrier_governor_mode, BarrierGovernorMode::kClassicAdaptive);
    EXPECT_EQ(fresh.advanced.barrier_governor_mode, BarrierGovernorMode::kMonitored);

    // The other seven preset fields are untouched by the event: take the
    // default's nine, put the two pre-event values back, and the whole preset
    // must match. If a tenth field ever drifts, this fails.
    InteriorPointSolverPresetFields fresh_as_fields = preset_fields_of(fresh);
    fresh_as_fields.acceptance_strategy_ = AcceptanceMode::kClassicMerit;
    fresh_as_fields.barrier_governor_ = BarrierGovernorMode::kClassicAdaptive;
    PresetGateExpectFieldsMatch(classic, fresh_as_fields);
}

// Instrument 2: THE OFF PROOF. The whole struct, through dump_options(), which
// prints one line per field in declaration order. The pre-event default is
// reconstructed as ipm_preset("classic") + max_iters = 500 and diffed against
// the live default: exactly three lines differ, they are the three the event
// moved, and every other line -- every other field of the options value -- is
// byte-identical.
TEST(InteriorPointSolverPresetsTest,
     PresetGateClassicPlusFiveHundredReconstructsThePreEventDefault) {
    const std::vector<std::string> before = lines_of(dump_to_string(pre_d1_default()));
    const std::vector<std::string> after = lines_of(dump_to_string(IpmOptions{}));

    ASSERT_EQ(before.size(), after.size()) << "dump_options() must print the same fields for both";

    std::vector<std::string> differing;
    for (std::size_t i = 0; i < before.size(); ++i) {
        if (before[i] != after[i]) {
            differing.push_back(before[i] + "  ->  " + after[i]);
        }
    }

    std::string joined;
    for (const std::string &d : differing) {
        joined += "\n  " + d;
    }

    ASSERT_EQ(differing.size(), 3u)
        << "W2 D1 moved three fields and no others; diff was:" << joined;
    EXPECT_EQ(differing[0], "common.max_iterations = 500  ->  common.max_iterations = 1000")
        << joined;
    EXPECT_EQ(differing[1],
              "advanced.acceptance_mode = kClassicMerit  ->  advanced.acceptance_mode = kFilter")
        << joined;
    EXPECT_EQ(differing[2], "advanced.barrier_governor_mode = kClassicAdaptive  ->  "
                            "advanced.barrier_governor_mode = kMonitored")
        << joined;
}

// The reconstruction is VALID, not merely printable: a consumer that wants the
// old behaviour back can hand this value to the solver and it is accepted.
// (classic_merit over classic_adaptive trips neither combination guard, which
// is exactly why the pre-event default needed no opt-in of its own.)
TEST(InteriorPointSolverPresetsTest, PresetGateThePreEventDefaultStillValidates) {
    EXPECT_NO_THROW(hven::validate(pre_d1_default()));
    EXPECT_NO_THROW(hven::validate(IpmOptions{}));
}
