// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// MUST NOT COMPILE.
//
// M6 W5 T3 deprecated NlpModel's six by-value evaluations in favour of the
// in-place forms. The attribute IS the migration notice, so it is pinned as a
// compile-time fact: under -Werror=deprecated-declarations the by-value call
// below is the one error, and the in-place replacement beside it is not one --
// which also proves nlp_model.h's own scoped suppression covers the defaults
// that still delegate to the deprecated form.

#include <hven/core/types.h>
#include <hven/model/nlp_model.h>

hven::SpMatRM probe_deprecated_eval_hess(const hven::solvers::NlpModel &model, const hven::Vec &x,
                                         const hven::Vec &le, const hven::Vec &li) {
    hven::SpMatRM out;
    model.eval_hess_in_place(x, 1.0, le, li, out); // the replacement: not deprecated
    return model.eval_hess(x, 1.0, le, li);        // the deprecated form: THE error
}
