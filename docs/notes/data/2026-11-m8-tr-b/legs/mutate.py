#!/usr/bin/env python3
"""M8 TR-B (../DECLARATION.md M): applies ONE named mutation to a scratch copy of the code head.
usage: mutate.py <copy-root> <id>      Each mutation is an exact text replacement that must match exactly once."""
import sys

M = {
    # i1: the IPM drops restoration_exited on the resumed path.
    'i1': ('src/drivers/ipm_solver.cpp',
           '    if (exit_s != nullptr) {\n        this->emit_message([&] {',
           '    if (exit_s != nullptr && std::string_view(exit_s) != "resumed") {\n        this->emit_message([&] {'),
    # i2: the IPM never writes restoration_entered (the entry writes a different kind).
    'i2': ('src/drivers/ipm_solver.cpp',
           '        msg.kind = MessageKind::kRestorationEntered;',
           '        msg.kind = MessageKind::kRankDeficiency;'),
    # i3: the IPM stop_at_iteration names the wrong s.
    'i3': ('src/drivers/ipm_solver.cpp',
           'msg.s = callback_stop ? "callback" : "token";',
           'msg.s = callback_stop ? "token" : "callback";'),
    # i4: the IPM message builder runs before the sink test (with no sink attached).
    'i4': ('src/drivers/ipm_solver.cpp',
           '    if (TraceSink *const sink = this->trace_; sink != nullptr) {\n        sink->on_message(build());',
           '    auto built = build();\n    if (TraceSink *const sink = this->trace_; sink != nullptr) {\n        sink->on_message(built);'),
    # s1: the SQP drops subproblem_failure.
    's1': ('src/drivers/sqp_solver.cpp',
           '            if (failure_s == nullptr) {\n                return;',
           '            if (true) {\n                return;'),
    # s2: the SQP never writes restoration_entered (the entry writes a refusal kind).
    's2': ('src/drivers/sqp_solver.cpp',
           'MessageTraceEvent m = make_sqp_message(MessageKind::kRestorationEntered, iter);',
           'MessageTraceEvent m = make_sqp_message(MessageKind::kRestorationRefused, iter);'),
    # s3: the SQP stop_at_iteration names the wrong s.
    's3': ('src/drivers/sqp_solver.cpp',
           'm.s = reason == StopReason::kStopRequested ? "token" : "callback";',
           'm.s = reason == StopReason::kStopRequested ? "callback" : "token";'),
    # s4: the SQP message builder runs before the sink test (with no sink attached).
    's4': ('src/drivers/sqp_solver.cpp',
           '    if (ipqp_trace_ != nullptr) {\n        ipqp_trace_->on_message(build());',
           '    auto built = build();\n    if (ipqp_trace_ != nullptr) {\n        ipqp_trace_->on_message(built);'),
    # s5: the SQP drops restoration_exited on the resumed path.
    's5': ('src/drivers/sqp_solver.cpp', '        note_exit("resumed", h_r);\n', ''),
    # s6: the SQP drops scaled_convergence_refused at the row-scaling guard.
    's6': ('src/drivers/sqp_solver.cpp',
           '        ++st.out.counters.scaled_convergence_refusals;\n        emit_note(MessageKind::kScaledConvergenceRefused, iter);\n    }\n    // THE CALLER-UNIT STATIONARITY CEILING',
           '        ++st.out.counters.scaled_convergence_refusals;\n    }\n    // THE CALLER-UNIT STATIONARITY CEILING'),
    # a1: M7-19 -- the abort door emits its callback event but no iter row.
    'a1': ('src/drivers/ipm_solver.cpp',
           'substitution, so the event describes the point the phase returns.\n            if (TraceSink *const sink = this->trace_; sink != nullptr) {',
           'substitution, so the event describes the point the phase returns.\n            if (TraceSink *const sink = this->trace_; sink != nullptr && false) {'),
    # a2: M7-19 -- the abort door emits its iter row but no callback event.
    'a2': ('src/drivers/ipm_solver.cpp',
           '            if (this->iteration_callback_) {\n                CBtimer.start();\n                this->fire_iteration_event(iters.back(), XSL, RHS, mu, last_step_norm,\n                                           best_substituted ? this->best_prim_obj_scratch_\n                                                            : iters.back().prim_obj);\n                CBtimer.stop();\n            }\n            ExitCode = SolveStatus::kNumericalError;',
           '            if (false) {\n                CBtimer.start();\n                this->fire_iteration_event(iters.back(), XSL, RHS, mu, last_step_norm,\n                                           best_substituted ? this->best_prim_obj_scratch_\n                                                            : iters.back().prim_obj);\n                CBtimer.stop();\n            }\n            ExitCode = SolveStatus::kNumericalError;'),
    # g1: an SQP message event is constructed outside any sink guard (the structural scan must kill it).
    'g1': ('src/drivers/sqp_solver.cpp',
           '        const char *failure_s = nullptr;\n',
           '        const char *failure_s = nullptr;\n        MessageTraceEvent unguarded_probe = make_sqp_message(MessageKind::kSubproblemFailure, iter);\n        (void)unguarded_probe;\n'),
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
