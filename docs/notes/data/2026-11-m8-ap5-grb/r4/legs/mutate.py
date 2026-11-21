#!/usr/bin/env python3
"""M8 AP-5 G-RB rider r4 (../DECLARATION-r4.md): applies ONE named mutation to a scratch copy of the code head.
usage: mutate.py <copy-root> <id>      Each mutation is an exact text replacement that must match exactly `count` times (default 1)."""
import sys

RB = 'include/hven/detail/drivers/return_best.h'
IP = 'src/drivers/ipm_solver.cpp'
M = {
    # o1: the order compares f before the violation.
    'o1': (RB, '    return v < bv || (v == bv && fe < bfe);\n', '    return fe < bfe || (fe == bfe && v < bv);\n'),
    # o2: non-strict improvement.
    'o2': (RB, '    return v < bv || (v == bv && fe < bfe);\n', '    return v < bv || (v == bv && fe <= bfe);\n'),
    # o3: no tolerance band.
    'o3': (RB, '    const double v = std::max(violation, feasibility_tol);\n    const double bv = std::max(best_violation, feasibility_tol);\n',
           '    const double v = violation;\n    const double bv = best_violation;\n'),
    # x1: the removed criterion path is reinstated in the IPM tracker (the old default: equality residual alone, `<=` ties).
    'x1': (IP, '    const double critval = (std::isnan(iter.econ_inf) || std::isnan(iter.icon_inf))\n                               ? std::numeric_limits<double>::quiet_NaN()\n                               : std::max(iter.econ_inf, iter.icon_inf);',
           '    const double critval = iter.econ_inf;', 1),
    'x1b': (IP, '    const bool beats =\n        detail::best_improves(critval, iter.prim_obj, BestCriteriaVal, this->best_prim_obj_scratch_,\n                              opts_.common.feasibility_tol);',
            '    const bool beats = critval <= BestCriteriaVal;', 1),
}

root, mid = sys.argv[1], sys.argv[2]
# x1 is two edits applied together (the measure and the comparison): the removed path in full.
steps = ['x1', 'x1b'] if mid == 'x1' else [mid]
for st in steps:
    ent = M[st]
    path, old, new = ent[0], ent[1], ent[2]
    want = ent[3] if len(ent) > 3 else 1
    p = root + '/' + path
    s = open(p).read()
    n = s.count(old)
    if n != want:
        print('MATCH_COUNT %s %d (want %d)' % (st, n, want))
        sys.exit(2)
    open(p, 'w').write(s.replace(old, new))
    print('APPLIED %s %s' % (st, path))
