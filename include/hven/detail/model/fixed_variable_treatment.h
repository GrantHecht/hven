// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The fixed-variable vocabulary's internals: the bound-relaxation limits the
// interior-point options validation reads, and the treatments' snake-case names
// for diagnostics. The enum itself is public (drivers/ipm_options.h).

#include "hven/drivers/ipm_enums.h"

namespace hven::detail {

/// Snake-case name for a treatment, for diagnostics and error messages.
inline const char *fixed_variable_treatment_name(IpmFixedVariableTreatment treatment) {
    switch (treatment) {
    case IpmFixedVariableTreatment::kMakeParameter:
        return "make_parameter";
    case IpmFixedVariableTreatment::kMakeConstraint:
        return "make_constraint";
    case IpmFixedVariableTreatment::kRelaxBounds:
        return "relax_bounds";
    }
    return "unknown";
}

/// Default widening of every finite, non-fixing variable bound: b moves outward
/// by this factor times max(1, |b|). Ipopt's bound_relax_factor default. Equal
/// to `IpmOptions{}.bound_relax_factor` (pinned in src/drivers/ipm_options.cpp).
inline constexpr double kDefaultBoundRelaxFactor = 1.0e-8;

/// Largest widening the options accept. The relaxed box is the one every barrier
/// term divides by, so a large factor solves a measurably different problem than
/// the one declared; a hundredth of max(1, |b|) is already far past any tolerance
/// a caller would set.
inline constexpr double kMaxBoundRelaxFactor = 1.0e-2;

} // namespace hven::detail
