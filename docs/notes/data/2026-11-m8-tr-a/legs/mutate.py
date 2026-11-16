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
           '    if (depth_ > 0 || !at_least(Verbosity::kDetail)) {\n        return;\n    }\n    fmt::print(out_, "{:>5} . qp.mode',
           '    if (depth_ > 0 || !at_least(Verbosity::kIterations)) {\n        return;\n    }\n    fmt::print(out_, "{:>5} . qp.mode'),
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
