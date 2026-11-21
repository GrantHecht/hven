// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The name tables of the interior-point option enums. Defined in
///        src/ipm/ipm_option_enum_names.cpp.

#include <hven/detail/core/options_registry.h>
#include <hven/ipm/ipm_enums.h>

namespace hven::detail {

HVEN_DECLARE_OPTION_ENUM(hven::IpmPhase, "IpmPhase")
HVEN_DECLARE_OPTION_ENUM(hven::IpmFixedVariableTreatment, "IpmFixedVariableTreatment")
HVEN_DECLARE_OPTION_ENUM(hven::IpmOrdering, "IpmOrdering")
HVEN_DECLARE_OPTION_ENUM(hven::IpmPivotStrategy, "IpmPivotStrategy")
HVEN_DECLARE_OPTION_ENUM(hven::IpmSolveParallelism, "IpmSolveParallelism")
HVEN_DECLARE_OPTION_ENUM(hven::IpmSolvePhaseMode, "IpmSolvePhaseMode")
HVEN_DECLARE_OPTION_ENUM(hven::IpmBarrierMode, "IpmBarrierMode")
HVEN_DECLARE_OPTION_ENUM(hven::IpmLineSearchMode, "IpmLineSearchMode")
HVEN_DECLARE_OPTION_ENUM(hven::IpmStepLengthRule, "IpmStepLengthRule")
HVEN_DECLARE_OPTION_ENUM(hven::IpmAcceptanceMode, "IpmAcceptanceMode")
HVEN_DECLARE_OPTION_ENUM(hven::IpmMeritPenaltyRule, "IpmMeritPenaltyRule")
HVEN_DECLARE_OPTION_ENUM(hven::IpmBarrierGovernorMode, "IpmBarrierGovernorMode")
HVEN_DECLARE_OPTION_ENUM(hven::IpmRestorationMode, "IpmRestorationMode")
HVEN_DECLARE_OPTION_ENUM(hven::IpmInertiaMode, "IpmInertiaMode")

} // namespace hven::detail
