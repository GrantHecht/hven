// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The public SqpSolver handle: construction, transfer and forwarding.
//
// Everything the engine does lives in SqpSolverImpl (src/drivers/sqp_solver.cpp);
// the bodies here only reach it. They are a separate translation unit so the
// engine's own source stays free of the handle, and so the one boundary the
// private implementation draws is a single, named file.

#include <memory>
#include <stdexcept>
#include <utility>

#include "hven/detail/drivers/sqp_solver_impl.h"

namespace hven::solvers {

namespace {

[[noreturn]] void throw_sqp_moved_from() {
    throw std::logic_error("SqpSolver: the solver was moved from; it may only be destroyed or "
                           "assigned to");
}

} // namespace

SqpSolverImpl &SqpSolver::impl() {
    if (impl_ == nullptr) [[unlikely]] {
        throw_sqp_moved_from();
    }
    return *impl_;
}

const SqpSolverImpl &SqpSolver::impl() const {
    if (impl_ == nullptr) [[unlikely]] {
        throw_sqp_moved_from();
    }
    return *impl_;
}

SqpSolver::SqpSolver(const SqpOptions &opts) : impl_(std::make_unique<SqpSolverImpl>(opts)) {}

SqpSolver::~SqpSolver() = default;
SqpSolver::SqpSolver(SqpSolver &&) noexcept = default;
SqpSolver &SqpSolver::operator=(SqpSolver &&) noexcept = default;

const SqpOptions &SqpSolver::options() const noexcept { return impl_->options(); }

void SqpSolver::set_options(SqpOptions o) { impl().set_options(std::move(o)); }

int SqpSolver::ssn_tier_num_threads() const { return impl().ssn_tier_num_threads(); }

int SqpSolver::ipqp_tier_num_threads() const { return impl().ipqp_tier_num_threads(); }

SqpResult SqpSolver::solve(const NlpModel &model) { return impl().solve(model); }

SqpResult SqpSolver::solve(const NlpModel &model, const Vec &x0) { return impl().solve(model, x0); }

SqpResult SqpSolver::solve(const NlpModel &model, const Vec &x0, SolveBudget budget) {
    return impl().solve(model, x0, budget);
}

SqpResult SqpSolver::solve(NlpModelAssembly &bridge, const Vec &x0) {
    return impl().solve(bridge, x0);
}

SqpResult SqpSolver::solve(NlpModelAssembly &bridge, const Vec &x0, SolveBudget budget) {
    return impl().solve(bridge, x0, budget);
}

SqpResult SqpSolver::solve(const NlpModel &model, const Vec &x0, const SqpWarmStart &warm,
                           SolveBudget budget) {
    return impl().solve(model, x0, warm, budget);
}

SqpResult SqpSolver::solve(NlpModelAssembly &bridge, const Vec &x0, const SqpWarmStart &warm,
                           SolveBudget budget) {
    return impl().solve(bridge, x0, warm, budget);
}

SqpResult SqpSolver::solve(const NlpModel &model, const Vec &x0, const WarmStartData &warm,
                           SolveBudget budget) {
    return impl().solve(model, x0, warm, budget);
}

SqpResult SqpSolver::solve(NlpModelAssembly &bridge, const Vec &x0, const WarmStartData &warm,
                           SolveBudget budget) {
    return impl().solve(bridge, x0, warm, budget);
}

WarmStartData SqpSolver::export_warm_start() const { return impl().export_warm_start(); }

void SqpSolver::set_iteration_callback(IterationCallback cb) {
    impl().set_iteration_callback(std::move(cb));
}

void SqpSolver::set_iteration_callback(IterationCallback cb, IterationPayload payload) {
    impl().set_iteration_callback(std::move(cb), payload);
}

IterationPayload SqpSolver::iteration_payload() const noexcept {
    return impl_->iteration_payload();
}

void SqpSolver::clear_iteration_callback() { impl().clear_iteration_callback(); }

void SqpSolver::attach_trace(TraceSink *sink) { impl().attach_trace(sink); }

void SqpSolver::attach_ledger(Ledger *ledger, std::string label_prefix) {
    impl().attach_ledger(ledger, std::move(label_prefix));
}

} // namespace hven::solvers
