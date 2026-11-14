// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The Nlp callback boundary's counter.
//
// A CountingNlp sits between a user's Nlp and both engine transcriptions of a
// Problem, forwards every virtual unchanged and counts the five evaluation
// callbacks (eval_f, eval_grad_f, eval_g, eval_jacobian, eval_hessian). The
// structure, bounds, dimension and name queries are forwarded and not counted.
// SolveResult::work reads the tally; the public solver handle resets it at solve
// entry.

#include <memory>
#include <string>
#include <utility>

#include "hven/model/nlp.h"

namespace hven::detail {

class CountingNlp final : public Nlp {
  public:
    explicit CountingNlp(std::shared_ptr<Nlp> inner) : inner_(std::move(inner)) {}

    struct Tally {
        Index objective = 0;
        Index gradient = 0;
        Index constraints = 0;
        Index jacobian = 0;
        Index hessian = 0;
    };

    const Tally &tally() const noexcept { return tally_; }
    void reset_tally() noexcept { tally_ = Tally{}; }

    NlpDimensions dimensions() const override { return inner_->dimensions(); }
    void bounds(VecRef x_lower, VecRef x_upper, VecRef g_lower, VecRef g_upper) const override {
        inner_->bounds(x_lower, x_upper, g_lower, g_upper);
    }
    double eval_f(ConstVecRef x) const override {
        ++tally_.objective;
        return inner_->eval_f(x);
    }
    void eval_grad_f(ConstVecRef x, VecRef grad) const override {
        ++tally_.gradient;
        inner_->eval_grad_f(x, grad);
    }
    void eval_g(ConstVecRef x, VecRef g) const override {
        ++tally_.constraints;
        inner_->eval_g(x, g);
    }
    void jacobian_structure(IndexVecRef rows, IndexVecRef cols) const override {
        inner_->jacobian_structure(rows, cols);
    }
    void eval_jacobian(ConstVecRef x, VecRef values) const override {
        ++tally_.jacobian;
        inner_->eval_jacobian(x, values);
    }
    bool has_hessian() const override { return inner_->has_hessian(); }
    void hessian_structure(IndexVecRef rows, IndexVecRef cols) const override {
        inner_->hessian_structure(rows, cols);
    }
    void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef lambda,
                      VecRef values) const override {
        ++tally_.hessian;
        inner_->eval_hessian(x, obj_factor, lambda, values);
    }
    std::string name() const override { return inner_->name(); }

  private:
    std::shared_ptr<Nlp> inner_;
    mutable Tally tally_;
};

} // namespace hven::detail
