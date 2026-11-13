// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The public SqpSolver handle: construction, transfer and forwarding.
//
// Everything the engine does lives in SqpSolverImpl (src/drivers/sqp_solver.cpp);
// the bodies here only reach it. They are a separate translation unit so the
// engine's own source stays free of the handle, and so the one boundary the
// private implementation draws is a single, named file.

#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "hven/detail/drivers/callback_rows.h"
#include "hven/detail/drivers/result_message.h"
#include "hven/detail/drivers/sqp_solver_impl.h"
#include "hven/detail/model/problem_access.h"
#include "hven/detail/warmstart/sqp_warm_access.h"

using namespace hven::detail;

namespace hven {

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

SqpSolver::SqpSolver(SqpOptions opts) : impl_(std::make_unique<SqpSolverImpl>(std::move(opts))) {}

SqpSolver::~SqpSolver() = default;
SqpSolver::SqpSolver(SqpSolver &&) noexcept = default;
SqpSolver &SqpSolver::operator=(SqpSolver &&) noexcept = default;

const SqpOptions &SqpSolver::options() const { return impl().options(); }

void SqpSolver::set_options(SqpOptions o) { impl().set_options(std::move(o)); }

int SqpSolver::ssn_tier_threads() const { return impl().ssn_tier_num_threads(); }

const SqpResult &SqpSolver::last_result() const { return impl().last_result(); }

int SqpSolver::ipqp_tier_threads() const { return impl().ipqp_tier_num_threads(); }

// The Problem is the solve's one argument: the SQP's form of it is refreshed
// first (or the solve refused), the engine's internal entry runs on that form
// unchanged, and the result's row blocks are mapped into the problem's row
// space last.
namespace {

bool reads_exact_hessian(const SqpOptions &options) {
    return options.hessian_mode == SqpHessianMode::kExact;
}

// What the handle adds to the engine's result: the row-space blocks, the work
// tally, the stop reason and the stop message.
void finish_result(const SqpSolverImpl &engine, const Problem &problem, const SolveBudget &budget,
                   SqpResult &result) {
    detail::ProblemAccess::report(problem, result);
    detail::ProblemAccess::report_work(problem, result);
    result.work.factorizations = result.counters.factorizations;
    result.stop_reason = engine.last_stop_reason();
    result.message = detail::sqp_result_message(
        result, result.stop_reason, engine.last_stop_detail(), engine.options(), budget);
}

// THE SAME FINISHING FOR A SOLVE AN EXCEPTION INTERRUPTED (M8 B5); see the
// interior-point handle's twin. Runs inside the catch clause and must not throw:
// a failure leaves the bare partial, with the status and the fixed message.
void finish_interrupted_result(SqpSolverImpl &engine, const Problem &problem,
                               const SolveBudget &budget, const Vec &x0, const std::string &what,
                               SqpResult &result) noexcept {
    result.status = SolveStatus::kInterrupted;
    // Set before the try: the report steps may throw, and a kept partial must
    // never read kNone after an escaped exception.
    result.stop_reason = StopReason::kException;
    try {
        result.message = "interrupted by an exception that escaped the solve";
        if (result.x.size() == 0) {
            result.x = x0;
        }
        engine.note_exception_stop(what);
        finish_result(engine, problem, budget, result);
    } catch (...) {
    }
}

template <class Entry>
SqpResult solve_on(SqpSolverImpl &engine, Problem &problem, const Vec &x0,
                   const SolveBudget &budget, Entry &&entry) {
    engine.set_last_result(SqpResult{});
    try {
        const detail::SqpTarget target = detail::ProblemAccess::sqp_target(
            problem, reads_exact_hessian(engine.options()), engine.options().common.infinite_bound);
        detail::ProblemAccess::reset_work(problem);
        const detail::CallbackRowsScope rows(engine, problem);
        SqpResult result =
            target.assembly != nullptr ? entry(*target.assembly) : entry(*target.model);
        finish_result(engine, problem, budget, result);
        // A returned solve leaves last_result() empty: the value is the caller's.
        return result;
    } catch (...) {
        // PROPAGATED UNCHANGED (M8 B5): the same exception object, after the
        // partial result is finished and kept. Nothing here wraps it.
        SqpResult partial = engine.take_interrupted_result();
        finish_interrupted_result(engine, problem, budget, x0,
                                  detail::describe_exception(std::current_exception()), partial);
        engine.set_last_result(std::move(partial));
        throw;
    }
}

} // namespace

// An empty WarmStart takes EXACTLY the cold entry (the no-payload engine route),
// so no payload counter moves for it.
SqpResult SqpSolver::solve(Problem &problem, ConstVecRef x0_in, const WarmStart &warm,
                           SolveBudget budget) {
    const Vec x0 = x0_in;
    SqpSolverImpl &engine = impl();
    if (warm.empty()) {
        return solve_on(engine, problem, x0, budget,
                        [&](auto &form) { return engine.solve(form, x0, budget); });
    }
    return solve_on(engine, problem, x0, budget,
                    [&](auto &form) { return engine.solve(form, x0, warm, budget); });
}

// The native route: the opaque handle is unwrapped to the engine's state here and
// nowhere deeper.
SqpResult SqpSolver::solve(Problem &problem, ConstVecRef x0_in, const SqpWarmStart &warm,
                           SolveBudget budget) {
    const Vec x0 = x0_in;
    SqpSolverImpl &engine = impl();
    const detail::SqpWarmState &state = detail::SqpWarmAccess::state(warm);
    return solve_on(engine, problem, x0, budget,
                    [&](auto &form) { return engine.solve(form, x0, state, budget); });
}

void SqpSolver::set_iteration_callback(IterationCallback cb, IterationPayload payload,
                                       CallbackScope scope) {
    impl().set_iteration_callback(std::move(cb), payload, scope);
}

IterationPayload SqpSolver::iteration_payload() const { return impl().iteration_payload(); }

CallbackScope SqpSolver::iteration_scope() const { return impl().iteration_scope(); }

void SqpSolver::clear_iteration_callback() { impl().clear_iteration_callback(); }

void SqpSolver::attach_trace(TraceSink *sink) { impl().attach_trace(sink); }

void SqpSolver::attach_ledger(Ledger *ledger, std::string label_prefix) {
    impl().attach_ledger(ledger, std::move(label_prefix));
}

} // namespace hven
