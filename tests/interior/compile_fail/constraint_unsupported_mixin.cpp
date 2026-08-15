// MUST NOT COMPILE.
//
// A type whose adapter declares it objective-only by inheriting
// ConstraintUnsupported. This is the sanctioned way to register one direction,
// so its refusal must read as a deliberate answer rather than an accident:
// the expected output is the mixin's own authored "hven constraint adapter:"
// static_assert and nothing raw.
//
// The mirror of objective_unsupported_mixin.cpp. Both halves of the
// half-policy pair are probed because they fail through DIFFERENT paths:
// ConstraintUnsupported::install_constraint IS present and callable, so the
// interface constructor's own backstop is satisfied and hands off, and the
// mixin's static_assert is what fires. A probe on only one direction would
// leave the other's authored message unable to regress loudly.

#include "adapter_fixture_functions.h"

hven::solvers::ConstraintInterface probe_constraint_unsupported_mixin() {
    const adapter_fixture::StubMixinObjectiveOnly f{};
    return hven::solvers::ConstraintInterface(f);
}
