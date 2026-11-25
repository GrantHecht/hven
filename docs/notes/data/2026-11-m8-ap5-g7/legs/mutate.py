#!/usr/bin/env python3
"""M8 AP-5 G7 (../DECLARATION.md): applies ONE named mutation to a scratch copy of the code head.
usage: mutate.py <copy-root> <id>      Each mutation is an exact text replacement that must match exactly once."""
import sys

M = {
    # Q7: IpmConvergence::feasibility not written.
    'q1': ('src/drivers/ipm_solver.cpp',
           '    this->result_.convergence.feasibility =\n',
           '    [[maybe_unused]] const double mut_q1_ =\n'),
    # Q7: the dump prints the retired key.
    'q2': ('src/drivers/ipm_options.cpp',
           'kv_dbl(os, "termination.acceptable_stationarity_tol"',
           'kv_dbl(os, "termination.acceptable_kkt_tol"'),
    # R4: SqpTiming::engine_seconds not written.
    'r4': ('src/drivers/sqp_solver.cpp',
           '    out.timing.engine_seconds = wall_seconds;\n',
           '    (void)wall_seconds;\n'),
    # E4: the SQP worker preset keeps the caller's threads.
    'e4': ('src/drivers/sqp_options.cpp',
           'SqpOptions worker_options(SqpOptions base) {\n    base.common.threads = 1;\n',
           'SqpOptions worker_options(SqpOptions base) {\n'),
    # E3: the moved SSN introspection reads the IPQP tier.
    'e3': ('include/hven/detail/drivers/sqp_solver_impl.h',
           '        return solver.impl().ssn_tier_num_threads();\n',
           '        return solver.impl().ipqp_tier_num_threads();\n'),
    # R7 (SQP): payload_ignored never set.
    'r7s': ('src/drivers/sqp_solver.cpp',
            '        (warm.valid && opts_.common.start_level == StartLevel::kCold) ? 1 : 0;\n',
            '        0;\n'),
    # R7 (IPM): start_level_used never kWarm.
    'r7i': ('src/drivers/ipm_solver.cpp',
            '        this->result_.counters.start_level_used = StartLevel::kWarm;\n',
            '        (void)0;\n'),
    # R7 (IPM): start_level_used never kSeeded.
    'r7j': ('src/drivers/ipm_solver.cpp',
            '                this->result_.counters.start_level_used = StartLevel::kSeeded;\n',
            '                (void)0;\n'),
    # B: the main ladder's count not added.
    'b1': ('src/drivers/ipm_solver.cpp',
           '        this->result_.counters.inertia_corrections += h_facs;\n',
           ''),
    # B: the dense guard's re-run count not added (REPORTED).
    'b2': ('src/drivers/ipm_solver.cpp',
           '                    this->result_.counters.inertia_corrections += extra;\n',
           ''),
    # E2: the throw path's z not filled.
    'e2': ('src/drivers/sqp_solver.cpp',
           '                if (k.finite) {\n                    out.z = k.z;\n                }\n',
           '                (void)k;\n'),
    # E2: the lagging-evaluation guard ignored (REPORTED: no test reaches the window).
    'e2b': ('src/drivers/sqp_solver.cpp',
            '        if (!st.ev_lags_x && out.x.size() == n',
            '        if (out.x.size() == n'),
    # C (IPM): a callback stop in restoration takes no stash (G2's rule).
    'c1': ('src/drivers/ipm_solver.cpp',
           '            if (!stop_best_applied && this->resto_stash_.valid && this->restoration_ &&\n',
           '            if (!callback_stop && this->resto_stash_.valid && this->restoration_ &&\n'),
    # C (IPM): the stash armed by the token only (G2's arming).
    'c1b': ('src/drivers/ipm_solver.cpp',
            '    if (this->stop_token_.stop_possible() || this->iteration_callback_) {\n',
            '    if (this->stop_token_.stop_possible()) {\n'),
    # C (SQP, D-h): a callback answer on the sub-solve's token-stopped row relabels the parent.
    'c2': ('src/drivers/sqp_solver.cpp',
           '        (!st.interrupted || sub_reason == StopReason::kStopRequested)) {\n',
           '        !st.interrupted) {\n'),
    # Table pins: an X() entry deleted (BUILD_FAIL is the kill, by the sizeof/arity static_asserts).
    'm1': ('include/hven/core/solver_counters.h',
           '    X(inertia_corrections, counter_never_absent)                                                   \\\n',
           ''),
    'm2': ('include/hven/core/solver_counters.h',
           '    X(optimal_row_gray_exits, counter_never_absent)                                                \\\n    X(payload_ignored, counter_never_absent)\n',
           '    X(optimal_row_gray_exits, counter_never_absent)\n'),
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
