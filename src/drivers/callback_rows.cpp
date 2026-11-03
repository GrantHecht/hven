// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/detail/drivers/callback_rows.h"

#include "hven/detail/model/problem_access.h"

namespace hven::solvers::detail {

void compose_event_lambda(const Problem *problem, const Vec &lambda_e, const Vec &lambda_i,
                          Vec &scratch) {
    if (problem != nullptr) {
        scratch = ProblemAccess::compose_rows(*problem, lambda_e, lambda_i);
        return;
    }
    scratch.resize(lambda_e.size() + lambda_i.size());
    scratch.head(lambda_e.size()) = lambda_e;
    scratch.tail(lambda_i.size()) = lambda_i;
}

} // namespace hven::solvers::detail
