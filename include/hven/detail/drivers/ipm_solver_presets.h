// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Named IpmSolver configuration presets: five mechanism-named
// globalization configurations, each a pure Settings field assignment (no
// algorithm code is touched). `classic` is THE PRE-2026-09-18 DEFAULT, PINNED
// HERE AS LITERALS -- not read off a default-constructed Settings -- so this
// table keeps its mechanism meaning independently of the struct's own defaults.
//
// That independence stopped being hypothetical at M7 W2 D1 (2026-09-18), the
// declared IPM-default event: IpmOptions{} moved to filter acceptance over the
// monitored governor with max_iters = 1000, and `classic` did NOT move with it.
// The table is therefore the thing that keeps the old default REACHABLE:
// ipm_preset("classic") with max_iters = 500 reconstructs it exactly, and
// tests/interior/test_ipm_solver_presets.cpp pins that reconstruction field by
// field and dump line by dump line.
//
// Every preset assigns exactly the same nine globalization fields (the full
// set IpmSolver::apply_preset() touches): acceptance_mode_,
// merit_penalty_rule_, barrier_governor_, never_monotone_, restoration_mode_,
// inertia_mode_, max_soc_, ls_extended_iters_, watchdog_. No other Settings
// field (tolerances, iteration caps, QP parameters, ...) is read or written by
// a preset.
//
// kInteriorPointSolverPresets drives both ipm_preset()'s
// dispatch and the valid-name list folded into its error message.
//
// The comparative figures below were measured on the consumer project's
// example suite, against `classic` as the baseline.

#pragma once

#include <array>
#include <string_view>

#include "hven/drivers/ipm_options.h"

namespace hven::detail {

// The nine globalization fields a preset assigns, exactly mirroring the
// IpmOptions members of the same names.
struct InteriorPointSolverPresetFields {
    IpmAcceptanceMode acceptance_mode_;
    IpmMeritPenaltyRule merit_penalty_rule_;
    IpmBarrierGovernorMode barrier_governor_;
    bool never_monotone_;
    IpmRestorationMode restoration_mode_;
    IpmInertiaMode inertia_mode_;
    Index max_soc_;
    Index ls_extended_iters_;
    bool watchdog_;
};

struct InteriorPointSolverPresetEntry {
    std::string_view name_;
    InteriorPointSolverPresetFields fields_;
};

// clang-format off
inline constexpr std::array<InteriorPointSolverPresetEntry, 5> kInteriorPointSolverPresets = {{
    // classic — the pre-2026-09-18 default, pinned as literals. It was the
    // stock Settings{} baseline until M7 W2 D1 moved the struct's own defaults;
    // it is pinned here (not read off Settings{}) so it keeps that mechanism
    // meaning, and with max_iters = 500 written on it, it reconstructs the old
    // default path bit-identically.
    {"classic", InteriorPointSolverPresetFields{
        /*acceptance_mode_=*/IpmAcceptanceMode::kClassicMerit,
        /*merit_penalty_rule_=*/IpmMeritPenaltyRule::kSinglePenalty,
        /*barrier_governor_=*/IpmBarrierGovernorMode::kClassicAdaptive,
        /*never_monotone_=*/false,
        /*restoration_mode_=*/IpmRestorationMode::kOff,
        /*inertia_mode_=*/IpmInertiaMode::kClassic,
        /*max_soc_=*/0,
        /*ls_extended_iters_=*/0,
        /*watchdog_=*/false,
    }},
    // filter_l1 — filter acceptance + monitored governor + nested-l1
    // restoration. Solved 12 of 17, at +31% aggregate iterations with median
    // parity; worst tail +609% (DionysusLowThrust).
    {"filter_l1", InteriorPointSolverPresetFields{
        /*acceptance_mode_=*/IpmAcceptanceMode::kFilter,
        /*merit_penalty_rule_=*/IpmMeritPenaltyRule::kSinglePenalty,
        /*barrier_governor_=*/IpmBarrierGovernorMode::kMonitored,
        /*never_monotone_=*/false,
        /*restoration_mode_=*/IpmRestorationMode::kL1Nested,
        /*inertia_mode_=*/IpmInertiaMode::kClassic,
        /*max_soc_=*/0,
        /*ls_extended_iters_=*/0,
        /*watchdog_=*/false,
    }},
    // soc_recovery_l1 — classic-merit acceptance + monitored governor +
    // proximal-regularization inertia + SOC(4) + recovery (extended
    // backtrack(2) + watchdog) + nested-l1 restoration. Solved 12 of 17, at
    // the highest aggregate cost (+42%) and the flattest worst-case tail
    // (+100%, OptimalDocking).
    {"soc_recovery_l1", InteriorPointSolverPresetFields{
        /*acceptance_mode_=*/IpmAcceptanceMode::kClassicMerit,
        /*merit_penalty_rule_=*/IpmMeritPenaltyRule::kSinglePenalty,
        /*barrier_governor_=*/IpmBarrierGovernorMode::kMonitored,
        /*never_monotone_=*/false,
        /*restoration_mode_=*/IpmRestorationMode::kL1Nested,
        /*inertia_mode_=*/IpmInertiaMode::kProximalRegularization,
        /*max_soc_=*/4,
        /*ls_extended_iters_=*/2,
        /*watchdog_=*/true,
    }},
    // soc_proximal — classic-merit acceptance + monitored governor +
    // proximal-regularization inertia + SOC(4) + proximal-switch restoration
    // (no l1 machinery). Solved 12 of 17, at the lowest aggregate cost (+27%);
    // worst tail +219% (MinimumTimeToClimb).
    {"soc_proximal", InteriorPointSolverPresetFields{
        /*acceptance_mode_=*/IpmAcceptanceMode::kClassicMerit,
        /*merit_penalty_rule_=*/IpmMeritPenaltyRule::kSinglePenalty,
        /*barrier_governor_=*/IpmBarrierGovernorMode::kMonitored,
        /*never_monotone_=*/false,
        /*restoration_mode_=*/IpmRestorationMode::kProximalSwitch,
        /*inertia_mode_=*/IpmInertiaMode::kProximalRegularization,
        /*max_soc_=*/4,
        /*ls_extended_iters_=*/0,
        /*watchdog_=*/false,
    }},
    // merit_l1 — modernized merit acceptance (classic barrier governor, NOT
    // monitored) + nested-l1 restoration. Solved 7+2 under the module call
    // shape and 8+2 under the matched (single-optimize()) shape: for zermelo's
    // wrong-basin guess it is the call shape, not the acceptance mechanism,
    // that decides the outcome.
    {"merit_l1", InteriorPointSolverPresetFields{
        /*acceptance_mode_=*/IpmAcceptanceMode::kMerit,
        /*merit_penalty_rule_=*/IpmMeritPenaltyRule::kSinglePenalty,
        /*barrier_governor_=*/IpmBarrierGovernorMode::kClassicAdaptive,
        /*never_monotone_=*/false,
        /*restoration_mode_=*/IpmRestorationMode::kL1Nested,
        /*inertia_mode_=*/IpmInertiaMode::kClassic,
        /*max_soc_=*/0,
        /*ls_extended_iters_=*/0,
        /*watchdog_=*/false,
    }},
}};
// clang-format on

} // namespace hven::detail
