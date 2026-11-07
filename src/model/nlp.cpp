// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/model/nlp.h"

#include <stdexcept>

#include <fmt/format.h>

namespace hven {

namespace {

// The defaults serve an empty block. A non-empty destination means the problem
// declared entries this callback was not overridden to produce.
[[noreturn]] void throw_not_overridden(const Nlp &nlp, const char *callback, Index entries) {
    throw std::logic_error(fmt::format("Nlp '{}': {}() is not overridden, but the problem "
                                       "declares {} entries for it",
                                       nlp.name(), callback, entries));
}

} // namespace

void Nlp::eval_g(ConstVecRef /*x*/, VecRef g) const {
    if (g.size() > 0) {
        throw_not_overridden(*this, "eval_g", g.size());
    }
}

void Nlp::jacobian_structure(IndexVecRef rows, IndexVecRef /*cols*/) const {
    if (rows.size() > 0) {
        throw_not_overridden(*this, "jacobian_structure", rows.size());
    }
}

void Nlp::eval_jacobian(ConstVecRef /*x*/, VecRef values) const {
    if (values.size() > 0) {
        throw_not_overridden(*this, "eval_jacobian", values.size());
    }
}

void Nlp::hessian_structure(IndexVecRef rows, IndexVecRef /*cols*/) const {
    if (rows.size() > 0) {
        throw_not_overridden(*this, "hessian_structure", rows.size());
    }
}

void Nlp::eval_hessian(ConstVecRef /*x*/, double /*obj_factor*/, ConstVecRef /*lambda*/,
                       VecRef values) const {
    if (values.size() > 0) {
        throw_not_overridden(*this, "eval_hessian", values.size());
    }
}

} // namespace hven
