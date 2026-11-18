// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <hven/detail/model/problem_access.h>
#include <hven/detail/solver/callback_rows.h>

namespace hven::detail {

void compose_event_lambda(const Problem *problem, const Vec &lambda_e, const Vec &lambda_i,
                          Vec &scratch) {
    if (problem != nullptr) {
        ProblemAccess::compose_rows_into(*problem, lambda_e, lambda_i, scratch);
        return;
    }
    scratch.resize(lambda_e.size() + lambda_i.size());
    scratch.head(lambda_e.size()) = lambda_e;
    scratch.tail(lambda_i.size()) = lambda_i;
}

void compose_event_rows(const Problem *problem, const Vec &ce, const Vec &ci, Vec &g,
                        Vec &violation) {
    if (problem != nullptr) {
        ProblemAccess::compose_row_values_into(*problem, ce, ci, g, violation);
        return;
    }
    ProblemAccess::concatenated_row_values(ce, ci, g, violation);
}

void split_bound_multiplier(double z, double &z_lower, double &z_upper) {
    // NaN stays NaN on both sides: unmeasured, never zero.
    z_lower = z > 0.0 ? z : (z <= 0.0 ? 0.0 : z);
    z_upper = z < 0.0 ? -z : (z >= 0.0 ? 0.0 : z);
}

void split_bound_multipliers(const Vec &z, Vec &z_lower, Vec &z_upper) {
    z_lower.resize(z.size());
    z_upper.resize(z.size());
    for (Index k = 0; k < z.size(); ++k) {
        split_bound_multiplier(z[k], z_lower[k], z_upper[k]);
    }
}

} // namespace hven::detail
