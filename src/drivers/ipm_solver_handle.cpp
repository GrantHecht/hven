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

#include "hven/detail/drivers/callback_rows.h"
#include "hven/detail/drivers/ipm_solver_impl.h"
#include "hven/detail/drivers/result_message.h"
#include "hven/detail/model/problem_access.h"

namespace hven::solvers {

namespace {

[[noreturn]] void throw_ipm_moved_from() {
    throw std::logic_error("IpmSolver: the solver was moved from; it may only be destroyed or "
                           "assigned to");
}

// What the handle adds to the engine's result: the row-space blocks, the callback
// tally and the stop message.
void finish_result(const IpmSolverImpl &engine, const Problem &problem, const SolveBudget &budget,
                   IpmResult &result) {
    detail::ProblemAccess::report(problem, result);
    detail::ProblemAccess::report_work(problem, result);
    result.message =
        detail::ipm_result_message(result, engine.last_stop_reason(), engine.options(), budget);
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

bool IpmSolver::kkt_pattern_is_analyzed(const Problem &problem) const {
    const IpmSolverImpl &engine = impl();
    const NonLinearProgram *program = detail::ProblemAccess::built_ipm_program(problem);
    return program != nullptr && engine.kkt_pattern_is_analyzed(*program);
}

// The Problem is the solve's one argument: its interior-point transcription is
// refreshed (or built) first, the engine's internal entry runs on it unchanged,
// and the result's row blocks are mapped into the problem's row space last.
// An empty WarmStart takes EXACTLY the cold entry (the no-payload engine route),
// so no payload counter moves for it.
IpmResult IpmSolver::solve(Problem &problem, ConstVecRef x0_in, const WarmStart &warm,
                           SolveBudget budget) {
    const Vec x0 = x0_in;
    IpmSolverImpl &engine = impl();
    // The tally restarts BEFORE the program is built: the build of an Nlp
    // problem's program walks the three sparsity patterns with one Hessian and
    // one Jacobian evaluation, and a solve that pays for the build reports it.
    detail::ProblemAccess::reset_work(problem);
    NonLinearProgram &program = detail::ProblemAccess::ipm_program(problem);
    const detail::CallbackRowsScope rows(engine, problem);
    IpmResult result =
        warm.empty() ? engine.solve(program, x0, budget) : engine.solve(program, x0, warm, budget);
    finish_result(engine, problem, budget, result);
    return result;
}

void IpmSolver::set_iteration_callback(IterationCallback cb, IterationPayload payload,
                                       CallbackScope scope) {
    impl().set_iteration_callback(std::move(cb), payload, scope);
}

IterationPayload IpmSolver::iteration_payload() const { return impl().iteration_payload(); }

CallbackScope IpmSolver::iteration_scope() const { return impl().iteration_scope(); }

void IpmSolver::clear_iteration_callback() { impl().clear_iteration_callback(); }

void IpmSolver::set_kkt_hook(const KktHook &f) { impl().set_kkt_hook(f); }

void IpmSolver::clear_kkt_hook() { impl().clear_kkt_hook(); }

void IpmSolver::attach_trace(TraceSink *sink) { impl().attach_trace(sink); }

void IpmSolver::attach_ledger(Ledger *ledger, std::string label_prefix) {
    impl().attach_ledger(ledger, std::move(label_prefix));
}

} // namespace hven::solvers
