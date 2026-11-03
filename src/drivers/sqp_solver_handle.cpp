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

#include "hven/detail/drivers/result_message.h"
#include "hven/detail/drivers/sqp_solver_impl.h"
#include "hven/detail/model/problem_access.h"
#include "hven/detail/warmstart/sqp_warm_access.h"

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

SqpSolver::SqpSolver(SqpOptions opts) : impl_(std::make_unique<SqpSolverImpl>(std::move(opts))) {}

SqpSolver::~SqpSolver() = default;
SqpSolver::SqpSolver(SqpSolver &&) noexcept = default;
SqpSolver &SqpSolver::operator=(SqpSolver &&) noexcept = default;

const SqpOptions &SqpSolver::options() const { return impl().options(); }

void SqpSolver::set_options(SqpOptions o) { impl().set_options(std::move(o)); }

int SqpSolver::ssn_tier_num_threads() const { return impl().ssn_tier_num_threads(); }

int SqpSolver::ipqp_tier_num_threads() const { return impl().ipqp_tier_num_threads(); }

// The Problem is the solve's one argument: the SQP's form of it is refreshed
// first (or the solve refused), the engine's internal entry runs on that form
// unchanged, and the result's row blocks are mapped into the problem's row
// space last.
namespace {

bool reads_exact_hessian(const SqpOptions &options) {
    return options.hessian_mode == SqpHessianMode::kExact;
}

template <class Entry>
SqpResult solve_on(SqpSolverImpl &engine, Problem &problem, const SolveBudget &budget,
                   Entry &&entry) {
    const detail::SqpTarget target =
        detail::ProblemAccess::sqp_target(problem, reads_exact_hessian(engine.options()));
    detail::ProblemAccess::reset_work(problem);
    SqpResult result = target.assembly != nullptr ? entry(*target.assembly) : entry(*target.model);
    detail::ProblemAccess::report(problem, result);
    detail::ProblemAccess::report_work(problem, result);
    result.work.factorizations = result.counters.factorizations;
    result.message = detail::sqp_result_message(result, engine.options(), budget);
    return result;
}

} // namespace

// An empty WarmStart takes EXACTLY the cold entry (the no-payload engine route),
// so no payload counter moves for it.
SqpResult SqpSolver::solve(Problem &problem, const Vec &x0, const WarmStart &warm,
                           SolveBudget budget) {
    SqpSolverImpl &engine = impl();
    if (warm.empty()) {
        return solve_on(engine, problem, budget,
                        [&](auto &form) { return engine.solve(form, x0, budget); });
    }
    return solve_on(engine, problem, budget,
                    [&](auto &form) { return engine.solve(form, x0, warm, budget); });
}

// The native route: the opaque handle is unwrapped to the engine's state here and
// nowhere deeper.
SqpResult SqpSolver::solve(Problem &problem, const Vec &x0, const SqpWarmStart &warm,
                           SolveBudget budget) {
    SqpSolverImpl &engine = impl();
    const detail::SqpWarmState &state = detail::SqpWarmAccess::state(warm);
    return solve_on(engine, problem, budget,
                    [&](auto &form) { return engine.solve(form, x0, state, budget); });
}

void SqpSolver::set_iteration_callback(IterationCallback cb) {
    impl().set_iteration_callback(std::move(cb));
}

void SqpSolver::set_iteration_callback(IterationCallback cb, IterationPayload payload) {
    impl().set_iteration_callback(std::move(cb), payload);
}

IterationPayload SqpSolver::iteration_payload() const { return impl().iteration_payload(); }

void SqpSolver::clear_iteration_callback() { impl().clear_iteration_callback(); }

void SqpSolver::attach_trace(TraceSink *sink) { impl().attach_trace(sink); }

void SqpSolver::attach_ledger(Ledger *ledger, std::string label_prefix) {
    impl().attach_ledger(ledger, std::move(label_prefix));
}

} // namespace hven::solvers
