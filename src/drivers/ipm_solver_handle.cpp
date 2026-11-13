// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The public IpmSolver handle: construction, transfer and forwarding.
//
// Everything the engine does lives in IpmSolverImpl (src/drivers/ipm_solver.cpp);
// the bodies here only reach it. They are a separate translation unit so the
// engine's own source stays free of the handle, and so the one boundary the
// private implementation draws is a single, named file.

#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "hven/detail/drivers/callback_rows.h"
#include "hven/detail/drivers/ipm_solver_impl.h"
#include "hven/detail/drivers/result_message.h"
#include "hven/detail/model/problem_access.h"

using namespace hven::detail;

namespace hven {

namespace {

[[noreturn]] void throw_ipm_moved_from() {
    throw std::logic_error("IpmSolver: the solver was moved from; it may only be destroyed or "
                           "assigned to");
}

// What the handle adds to the engine's result: the row-space blocks, the callback
// tally, the stop reason and the stop message.
void finish_result(const IpmSolverImpl &engine, const Problem &problem, const SolveBudget &budget,
                   IpmResult &result) {
    detail::ProblemAccess::report(problem, result);
    detail::ProblemAccess::report_work(problem, result);
    result.stop_reason = detail::to_stop_reason(engine.last_stop_reason());
    result.message = detail::ipm_result_message(
        result, result.stop_reason, engine.last_stop_detail(), engine.options(), budget);
}

// THE SAME FINISHING FOR A SOLVE AN EXCEPTION INTERRUPTED (M8 B5): what the
// engine reached -- or, when it reached nothing, the start point with no
// iterations -- mapped into the problem's row space, with the work the solve
// spent and a message naming the exception. Runs inside the handle's catch
// clause, so it must not throw: a failure leaves the bare partial, which still
// carries the status and the fixed message set first.
void finish_interrupted_result(IpmSolverImpl &engine, const Problem &problem,
                               const SolveBudget &budget, const Vec &x0, const std::string &what,
                               IpmResult &result) noexcept {
    result.status = SolveStatus::kInterrupted;
    try {
        result.message = "interrupted by an exception that escaped the solve";
        if (result.x.size() == 0) {
            result.x = x0;
        }
        engine.note_exception_stop(what);
        detail::ProblemAccess::report(problem, result);
        detail::ProblemAccess::report_work(problem, result);
        result.stop_reason = detail::to_stop_reason(engine.last_stop_reason());
        result.message = detail::ipm_result_message(
            result, result.stop_reason, engine.last_stop_detail(), engine.options(), budget);
    } catch (...) {
    }
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


const IpmResult &IpmSolver::last_result() const { return impl().last_result(); }

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
    engine.set_last_result(IpmResult{});
    // The tally restarts BEFORE the program is built: the build of an Nlp
    // problem's program walks the three sparsity patterns with one Hessian and
    // one Jacobian evaluation, and a solve that pays for the build reports it.
    detail::ProblemAccess::reset_work(problem);
    try {
        NonLinearProgram &program =
            detail::ProblemAccess::ipm_program(problem, engine.options().common.infinite_bound);
        const detail::CallbackRowsScope rows(engine, problem);
        IpmResult result = warm.empty() ? engine.solve(program, x0, budget)
                                        : engine.solve(program, x0, warm, budget);
        finish_result(engine, problem, budget, result);
        // A returned solve leaves last_result() empty: the value is the caller's.
        return result;
    } catch (...) {
        // PROPAGATED UNCHANGED (M8 B5): the same exception object, after the
        // partial result is finished and kept. Nothing here wraps it.
        IpmResult partial = engine.take_interrupted_result();
        finish_interrupted_result(engine, problem, budget, x0,
                                  detail::describe_exception(std::current_exception()), partial);
        engine.set_last_result(std::move(partial));
        throw;
    }
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

} // namespace hven
