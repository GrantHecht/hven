// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Named InteriorPointSolver configuration presets: five mechanism-named
// globalization configurations, each a pure Settings field assignment (no
// algorithm code is touched). `classic` is the stock Settings{} baseline,
// pinned here as literals rather than read off a default-constructed Settings
// so this table keeps its mechanism meaning even if a future change moves the
// struct's own defaults.
//
// Every preset assigns exactly the same nine globalization fields (the full
// set InteriorPointSolver::apply_preset() touches): acceptance_strategy_,
// merit_penalty_rule_, barrier_governor_, never_monotone_, restoration_mode_,
// inertia_mode_, max_soc_, ls_extended_iters_, watchdog_. No other Settings
// field (tolerances, iteration caps, QP parameters, ...) is read or written by
// a preset.
//
// kInteriorPointSolverPresets drives both InteriorPointSolver::apply_preset()'s
// dispatch and the valid-name list folded into its error message.
//
// The comparative figures below were measured on the consumer project's
// example suite, against `classic` as the baseline.

#pragma once

#include <array>
#include <string_view>

#include "hven/detail/drivers/interior_point_solver_fwd.h"

namespace hven::solvers {

// The nine globalization fields a preset assigns, exactly mirroring the
// InteriorPointSolver::Settings members of the same names.
struct InteriorPointSolverPresetFields {
    AcceptanceStrategies acceptance_strategy_;
    MeritPenaltyRules merit_penalty_rule_;
    BarrierGovernors barrier_governor_;
    bool never_monotone_;
    RestorationModes restoration_mode_;
    InertiaModes inertia_mode_;
    int max_soc_;
    int ls_extended_iters_;
    bool watchdog_;
};

struct InteriorPointSolverPresetEntry {
    std::string_view name_;
    InteriorPointSolverPresetFields fields_;
};

// clang-format off
inline constexpr std::array<InteriorPointSolverPresetEntry, 5> kInteriorPointSolverPresets = {{
    // classic — the stock Settings{} baseline (bit-identical default path).
    // Pinned as literals (not read off Settings{}) so this preset keeps its
    // mechanism meaning independent of the struct's own defaults.
    {"classic", InteriorPointSolverPresetFields{
        /*acceptance_strategy_=*/AcceptanceStrategies::classic_merit,
        /*merit_penalty_rule_=*/MeritPenaltyRules::wmno,
        /*barrier_governor_=*/BarrierGovernors::classic_adaptive,
        /*never_monotone_=*/false,
        /*restoration_mode_=*/RestorationModes::off,
        /*inertia_mode_=*/InertiaModes::classic,
        /*max_soc_=*/0,
        /*ls_extended_iters_=*/0,
        /*watchdog_=*/false,
    }},
    // filter_l1 — filter acceptance + monitored governor + nested-l1
    // restoration. Solved 12 of 17, at +31% aggregate iterations with median
    // parity; worst tail +609% (DionysusLowThrust).
    {"filter_l1", InteriorPointSolverPresetFields{
        /*acceptance_strategy_=*/AcceptanceStrategies::filter,
        /*merit_penalty_rule_=*/MeritPenaltyRules::wmno,
        /*barrier_governor_=*/BarrierGovernors::monitored,
        /*never_monotone_=*/false,
        /*restoration_mode_=*/RestorationModes::l1_nested,
        /*inertia_mode_=*/InertiaModes::classic,
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
        /*acceptance_strategy_=*/AcceptanceStrategies::classic_merit,
        /*merit_penalty_rule_=*/MeritPenaltyRules::wmno,
        /*barrier_governor_=*/BarrierGovernors::monitored,
        /*never_monotone_=*/false,
        /*restoration_mode_=*/RestorationModes::l1_nested,
        /*inertia_mode_=*/InertiaModes::proximal_regularization,
        /*max_soc_=*/4,
        /*ls_extended_iters_=*/2,
        /*watchdog_=*/true,
    }},
    // soc_proximal — classic-merit acceptance + monitored governor +
    // proximal-regularization inertia + SOC(4) + proximal-switch restoration
    // (no l1 machinery). Solved 12 of 17, at the lowest aggregate cost (+27%);
    // worst tail +219% (MinimumTimeToClimb).
    {"soc_proximal", InteriorPointSolverPresetFields{
        /*acceptance_strategy_=*/AcceptanceStrategies::classic_merit,
        /*merit_penalty_rule_=*/MeritPenaltyRules::wmno,
        /*barrier_governor_=*/BarrierGovernors::monitored,
        /*never_monotone_=*/false,
        /*restoration_mode_=*/RestorationModes::proximal_switch,
        /*inertia_mode_=*/InertiaModes::proximal_regularization,
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
        /*acceptance_strategy_=*/AcceptanceStrategies::merit,
        /*merit_penalty_rule_=*/MeritPenaltyRules::wmno,
        /*barrier_governor_=*/BarrierGovernors::classic_adaptive,
        /*never_monotone_=*/false,
        /*restoration_mode_=*/RestorationModes::l1_nested,
        /*inertia_mode_=*/InertiaModes::classic,
        /*max_soc_=*/0,
        /*ls_extended_iters_=*/0,
        /*watchdog_=*/false,
    }},
}};
// clang-format on

} // namespace hven::solvers
