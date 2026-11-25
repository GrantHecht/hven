#!/usr/bin/env python3
"""M8 AP-5 G-RB (../DECLARATION.md): applies ONE named mutation to a scratch copy of the code head.
usage: mutate.py <copy-root> <id>      Each mutation is an exact text replacement that must match exactly `count` times (default 1)."""
import sys

RB = 'include/hven/detail/drivers/return_best.h'
SQ = 'src/drivers/sqp_solver.cpp'
IP = 'src/drivers/ipm_solver.cpp'
M = {
    # s1: a kInfeasible exit substitutes (the shared rule says yes under kOnAnyNonOptimal).
    's1': (RB, '    case SolveStatus::kOptimal:\n    case SolveStatus::kInfeasible:\n    case SolveStatus::kInterrupted:\n        return false;\n',
           '    case SolveStatus::kOptimal:\n    case SolveStatus::kInterrupted:\n        return false;\n    case SolveStatus::kInfeasible:\n        return mode == ReturnBest::kOnAnyNonOptimal;\n'),
    # s2: the SQP restoration exits substitute whatever the status (the rule is bypassed).
    's2': (SQ, 'best_applies(st.resto.status)', 'st.mb.snap_valid', 2),
    # l1: kOnLimit does not cover the work and time limits (the rule).
    'l1': (RB, '    case SolveStatus::kIterationLimit:\n    case SolveStatus::kWorkLimit:\n    case SolveStatus::kTimeLimit:\n        return mode != ReturnBest::kNever;\n',
           '    case SolveStatus::kIterationLimit:\n        return mode != ReturnBest::kNever;\n    case SolveStatus::kWorkLimit:\n    case SolveStatus::kTimeLimit:\n        return mode == ReturnBest::kOnAnyNonOptimal;\n'),
    # l2: the SQP limit exit is narrowed back to the iteration cap.
    'l2': (SQ, '        if (!mj.converged && !probe_exhausted &&\n            detail::return_best_applies(',
           '        if (!mj.converged && !probe_exhausted && st.limit_exit == SolveStatus::kOptimal &&\n            detail::return_best_applies('),
    # o1: the order compares f before the violation.
    'o1': (RB, '    return v < bv || (v == bv && fe < bfe);\n', '    return fe < bfe || (fe == bfe && v < bv);\n'),
    # o2: non-strict improvement.
    'o2': (RB, '    return v < bv || (v == bv && fe < bfe);\n', '    return v < bv || (v == bv && fe <= bfe);\n'),
    # o3: no tolerance band.
    'o3': (RB, '    const double v = std::max(violation, feasibility_tol);\n    const double bv = std::max(best_violation, feasibility_tol);\n',
           '    const double v = violation;\n    const double bv = best_violation;\n'),
    # n1: the IPM non-finite abort substitutes under every mode but kNever.
    'n1': (IP, 'if (rb_applies(SolveStatus::kNumericalError) && best_recorded) {',
           'if (opts_.common.return_best != ReturnBest::kNever && best_recorded) {'),
    # n2: the SQP non-limit exits never substitute.
    'n2': (SQ, '            return st.mb.snap_valid &&\n                   detail::return_best_applies(opts_.common.return_best, status);\n',
           '            return false;\n'),
    # p1: the SQP probe-budget stop is no longer excluded.
    'p1': (SQ, '        if (!mj.converged && !probe_exhausted &&\n            detail::return_best_applies(',
           '        if (!mj.converged &&\n            detail::return_best_applies('),
    # t1: the IPM work/time limit door does not substitute the pair (claims the best, returns the last).
    't1': (IP, 'if (rb_applies(ExitCode) && best_recorded) {\n                XSL = BestXSL;', 'if (rb_applies(ExitCode) && best_recorded && false) {\n                XSL = BestXSL;'),
    # r1 (REPORTED): the shared IPM measure reads the equality residual only.
    'r1': (IP, '        critval = (std::isnan(iter.econ_inf) || std::isnan(iter.icon_inf))\n                      ? std::numeric_limits<double>::quiet_NaN()\n                      : std::max(iter.econ_inf, iter.icon_inf);\n',
           '        critval = iter.econ_inf;\n'),
}

root, mid = sys.argv[1], sys.argv[2]
ent = M[mid]
path, old, new = ent[0], ent[1], ent[2]
want = ent[3] if len(ent) > 3 else 1
p = root + '/' + path
s = open(p).read()
n = s.count(old)
if n != want:
    print('MATCH_COUNT %s %d (want %d)' % (mid, n, want))
    sys.exit(2)
open(p, 'w').write(s.replace(old, new))
print('APPLIED %s %s' % (mid, path))
