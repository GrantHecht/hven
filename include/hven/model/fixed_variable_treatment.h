// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The interior-point engine's fixed-variable vocabulary: the treatment selector
// IpmOptions carries, and the bound-relaxation limits its validation reads.

namespace hven::solvers {

/// @brief How a primal variable whose declared lower and upper bounds are equal
///        is handed to the solver.
///
/// All three are implemented and all three reach the same solution on a
/// well-posed problem; they differ in the size of the system the solver
/// factorizes and in how exactly the variable sits at its value.
///
/// MakeParameter (the default) removes the variable from the optimization: it is
/// pinned at its bound value for every evaluation and the factorized system is
/// one row and column NARROWER per fixed variable, with its value exact in the
/// returned solution. MakeConstraint keeps it free and appends one internal
/// equality row per fixed variable AFTER every declared row, so the system is
/// WIDER and every user row keeps its index, and the variable reaches its value
/// to equality-constraint tolerance. RelaxBounds keeps it two-sided with its
/// bounds pushed apart by the relax factor, held near its value by the barrier,
/// and the system is the declared problem's size.
enum class FixedVariableTreatments { MakeParameter, MakeConstraint, RelaxBounds };

/// Human-readable name for a treatment, for diagnostics and error messages.
inline const char *fixed_variable_treatment_name(FixedVariableTreatments treatment) {
    switch (treatment) {
    case FixedVariableTreatments::MakeParameter:
        return "make_parameter";
    case FixedVariableTreatments::MakeConstraint:
        return "make_constraint";
    case FixedVariableTreatments::RelaxBounds:
        return "relax_bounds";
    }
    return "unknown";
}

/// Default widening applied to every finite, non-fixing variable bound before
/// it is recorded in the BoundSet: b is moved outward by this factor times
/// max(1, |b|). Matches Ipopt's bound_relax_factor default.
inline constexpr double kDefaultBoundRelaxFactor = 1.0e-8;

/// Largest widening the solver's settings accept. The relaxed box is the one
/// every barrier term actually divides by, so a large factor does not merely
/// loosen a bound -- it solves a measurably different problem than the one
/// declared, without saying so. A hundredth of max(1, |b|) is already far past
/// any tolerance a caller would set.
inline constexpr double kMaxBoundRelaxFactor = 1.0e-2;

} // namespace hven::solvers
