// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The public IpmSolver handle: construction, transfer and forwarding.
//
// Everything the engine does lives in IpmSolverImpl (src/drivers/ipm_solver.cpp);
// the bodies here only reach it. They are a separate translation unit so the
// engine's own source stays free of the handle, and so the one boundary the
// private implementation draws is a single, named file.

#include <memory>
#include <stdexcept>
#include <utility>

#include "hven/detail/drivers/ipm_solver_impl.h"

namespace hven::solvers {

namespace {

[[noreturn]] void throw_ipm_moved_from() {
    throw std::logic_error("IpmSolver: the solver was moved from; it may only be destroyed or "
                           "assigned to");
}

} // namespace

IpmSolverImpl &IpmSolver::impl() {
    if (impl_ == nullptr) [[unlikely]] {
        throw_ipm_moved_from();
    }
    return *impl_;
}

const IpmSolverImpl &IpmSolver::impl() const {
    if (impl_ == nullptr) [[unlikely]] {
        throw_ipm_moved_from();
    }
    return *impl_;
}

IpmSolver::IpmSolver(IpmOptions opts) : impl_(std::make_unique<IpmSolverImpl>(std::move(opts))) {}

IpmSolver::~IpmSolver() = default;
IpmSolver::IpmSolver(IpmSolver &&) noexcept = default;
IpmSolver &IpmSolver::operator=(IpmSolver &&) noexcept = default;

const IpmOptions &IpmSolver::options() const { return impl().options(); }

void IpmSolver::set_options(IpmOptions o) { impl().set_options(std::move(o)); }

IpmStopReason IpmSolver::last_stop_reason() const { return impl().last_stop_reason(); }

bool IpmSolver::kkt_pattern_is_analyzed(const NonLinearProgram &model) const {
    return impl().kkt_pattern_is_analyzed(model);
}

IpmResult IpmSolver::solve(NonLinearProgram &model, const Eigen::VectorXd &x0, SolveBudget budget) {
    return impl().solve(model, x0, budget);
}

IpmResult IpmSolver::solve(NonLinearProgram &model, const Eigen::VectorXd &x0,
                           const WarmStartData &warm, SolveBudget budget) {
    return impl().solve(model, x0, warm, budget);
}

void IpmSolver::set_iteration_callback(IterationCallback cb) {
    impl().set_iteration_callback(std::move(cb));
}

void IpmSolver::set_iteration_callback(IterationCallback cb, IterationPayload payload) {
    impl().set_iteration_callback(std::move(cb), payload);
}

IterationPayload IpmSolver::iteration_payload() const { return impl().iteration_payload(); }

void IpmSolver::clear_iteration_callback() { impl().clear_iteration_callback(); }

void IpmSolver::set_kkt_hook(const KktHook &f) { impl().set_kkt_hook(f); }

void IpmSolver::clear_kkt_hook() { impl().clear_kkt_hook(); }

void IpmSolver::attach_trace(TraceSink *sink) { impl().attach_trace(sink); }

void IpmSolver::attach_ledger(Ledger *ledger, std::string label_prefix) {
    impl().attach_ledger(ledger, std::move(label_prefix));
}

} // namespace hven::solvers
