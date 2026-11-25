#!/usr/bin/env python3
"""M8 TR-A (../DECLARATION.md M): applies ONE named mutation to a scratch copy of the code head.
usage: mutate.py <copy-root> <id>      Each mutation is an exact text replacement that must match exactly once."""
import sys

M = {
    # w1: a record field dropped from the writer.
    'w1': ('src/drivers/trace_writer.cpp',
           '    key_bool(b, first, "rolled_back", r.rolled_back);\n',
           ''),
    # w2: a wrong key.
    'w2': ('src/drivers/trace_writer.cpp',
           '    key_double(b, first, "stationarity", r.stationarity);\n',
           '    key_double(b, first, "stationary", r.stationarity);\n'),
    # w3: sid never advances.
    'w3': ('src/drivers/trace_writer.cpp', '        ++sid_;\n', ''),
    # w4: the solver_initialized milliseconds leak onto the wire.
    'w4': ('src/drivers/trace_writer.cpp',
           '    if (event.kind == MessageKind::kSolverInitialized) {\n        key_null(b, first, "a");',
           '    if (false) {\n        key_null(b, first, "a");'),
    # c1: a ladder rung leaks: the rows print at kSummary.
    'c1': ('src/drivers/console_trace_sink.cpp',
           '    if (!at_least(Verbosity::kIterations)) {\n        return;\n    }\n    const IterationRecord &r = event.record;',
           '    if (!at_least(Verbosity::kSummary)) {\n        return;\n    }\n    const IterationRecord &r = event.record;'),
    # c2: colour is on under kNever.
    'c2': ('src/drivers/console_trace_sink.cpp',
           '    color_ = f.color == ConsoleColor::kAlways ||',
           '    color_ = f.color != ConsoleColor::kAlways ||'),
    # c3: a kDetail line prints at kIterations.
    'c3': ('src/drivers/console_trace_sink.cpp',
           '    if (!at_least(Verbosity::kDetail)) {\n        return;\n    }\n    fmt::print(out_, "{:>5} . qp.mode',
           '    if (!at_least(Verbosity::kIterations)) {\n        return;\n    }\n    fmt::print(out_, "{:>5} . qp.mode'),
    # o1: the off path does work: the message builder runs before the sink test.
    'o1': ('src/drivers/ipm_solver.cpp',
           '    if (TraceSink *const sink = this->trace_; sink != nullptr) {\n        sink->on_message(build());',
           '    auto built = build();\n    if (TraceSink *const sink = this->trace_; sink != nullptr) {\n        sink->on_message(built);'),
    # o2: the off path builds the SQP row outside the guard.
    'o2': ('src/drivers/sqp_solver.cpp',
           '    if (ipqp_trace_ != nullptr) {\n        const IterationRecord record = make_sqp_iteration_record(',
           '    {\n        const IterationRecord record = make_sqp_iteration_record('),
    # v2: the IPM step norm is not written.
    'v2': ('src/drivers/ipm_solver.cpp',
           '            Citer.step_norm = (alpha * DXSL.head(this->primal_vars_)).lpNorm<Eigen::Infinity>();\n',
           '            (void)alpha;\n'),
    # v3: the IPM soc flag is never set.
    'v3': ('src/drivers/ipm_solver.cpp',
           '            Citer.soc = resolved_depth == kRecoveryDepthSoc;\n', ''),
    # v4: the main site's factorization stamp is dropped.
    'v4': ('src/drivers/ipm_solver.cpp',
           '        Citer.factorizations = this->lifetime_factorize_count() - this->factorizations_at_entry_;\n        this->fill_iter_info(',
           '        this->fill_iter_info('),
    # p1a: the interior-point handle forgets to unwind the sink on an exceptional exit.
    'p1a': ('src/drivers/ipm_solver_handle.cpp', '        engine.abort_trace_solve();\n', ''),
    # p1b: the SQP handle forgets to unwind the sink on an exceptional exit.
    'p1b': ('src/drivers/sqp_solver_handle.cpp', '        engine.abort_trace_solve();\n', ''),
    # p1c: the restoration sub-driver forgets to unwind its own level.
    'p1c': ('src/drivers/sqp_solver.cpp', '            sink->on_solve_abort();\n', '            (void)sink;\n'),
    # p2: the non-finite abort accumulates the gradient block twice (the head the review found).
    'p2': ('src/drivers/ipm_solver.cpp',
           '            Citer.factorizations =\n                this->lifetime_factorize_count() - this->factorizations_at_entry_;\n            iters.push_back(Citer);\n            if (opts_.return_best && best_recorded) {',
           '            Citer.factorizations =\n                this->lifetime_factorize_count() - this->factorizations_at_entry_;\n            v_rhs.prim_grad() += PGX;\n            iters.push_back(Citer);\n            if (opts_.return_best && best_recorded) {'),
    # p3: the row's multiplier maxima are left in engine units.
    'p3': ('src/drivers/ipm_solver.cpp',
           '    row.max_multiplier_eq = r.max_e_mult / scale;\n    row.max_multiplier_ineq = r.max_i_mult / scale;',
           '    row.max_multiplier_eq = r.max_e_mult;\n    row.max_multiplier_ineq = r.max_i_mult;'),
    # p4: a row with no step carries a nonzero barrier objective.
    'p4': ('src/drivers/ipm_solver.cpp',
           '    if (stepped) {\n        row.barrier_objective = r.barr_obj;',
           '    if (stepped || r.barr_obj != 0.0) {\n        row.barrier_objective = r.barr_obj;'),
    # p5: the console's QP-detail lines are suppressed at nested depth again.
    'p5': ('src/drivers/console_trace_sink.cpp',
           '    if (!at_least(Verbosity::kDetail)) {\n        return;\n    }\n    fmt::print(out_, "{:>5} . qp.mode',
           '    if (depth_ > 0 || !at_least(Verbosity::kDetail)) {\n        return;\n    }\n    fmt::print(out_, "{:>5} . qp.mode'),
    # p11a: the SQP reads its census from the seam (no row-kind split) instead of the problem.
    'p11a': ('src/drivers/sqp_solver.cpp',
             '            trace_statistics_ = callback_rows_ != nullptr',
             '            trace_statistics_ = false'),
    # p11b: the IPM likewise.
    'p11b': ('src/drivers/ipm_solver.cpp',
             '        if (this->callback_rows_ != nullptr) {\n            begin_event.statistics = this->callback_rows_->statistics();',
             '        if (false) {\n            begin_event.statistics = this->callback_rows_->statistics();'),
    # t1: an SQP row without a step still reports one.
    't1': ('src/drivers/sqp_solver.cpp',
           '    const bool stepped = r.qp_solved && r.qp_status == QpStatus::kOptimal;',
           '    const bool stepped = true;'),
    # t2: the IPM row's alpha is the maximum, not the effective length.
    't2': ('src/drivers/ipm_solver.cpp',
           '        row.alpha_primal = r.alpha_t * r.alpha_p;', '        row.alpha_primal = r.alpha_p;'),
}

root, mid = sys.argv[1], sys.argv[2]
path, old, new = M[mid]
p = root + '/' + path
s = open(p).read()
n = s.count(old)
if n != 1:
    print('MATCH_COUNT %s %d' % (mid, n))
    sys.exit(2)
open(p, 'w').write(s.replace(old, new))
print('APPLIED %s %s' % (mid, path))
