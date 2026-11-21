#!/usr/bin/env python3
"""M8 GL-0 (../DECLARATION.md M): applies ONE named mutation to a scratch copy of the code head.
usage: mutate.py <copy-root> <id>      Each mutation is an exact text replacement that must match exactly once."""
import sys

SQP = 'src/sqp/sqp_solver.cpp'
FUN = 'src/sqp/globalization/funnel.cpp'
TRW = 'src/trace/trace_writer.cpp'
HDR = 'include/hven/detail/sqp/sqp_major_row.h'
M = {
    # c1: a funnel-ceiling rejection is counted as an Armijo failure.
    'c1': (SQP, '    case detail::RejectCause::kFunnelCeiling:\n        ++c.reject_funnel_ceiling;',
           '    case detail::RejectCause::kFunnelCeiling:\n        ++c.reject_f_armijo;'),
    # c2: an Armijo failure is counted as an h-type funnel failure.
    'c2': (SQP, '    case detail::RejectCause::kArmijo:\n        ++c.reject_f_armijo;',
           '    case detail::RejectCause::kArmijo:\n        ++c.reject_h_funnel;'),
    # c3: an h-type funnel failure is counted as a ceiling violation.
    'c3': (SQP, '    case detail::RejectCause::kFunnelDecrease:\n        ++c.reject_h_funnel;',
           '    case detail::RejectCause::kFunnelDecrease:\n        ++c.reject_funnel_ceiling;'),
    # c4: a routed QP failure is counted as "other".
    'c4': (SQP, '    case detail::RejectCause::kNone: // the QP-failure routes pass kNone\n        ++c.reject_qp_failed;',
           '    case detail::RejectCause::kNone: // the QP-failure routes pass kNone\n        ++c.reject_other;'),
    # c5: the strategy swaps its Armijo and h-type causes (the classification itself).
    'c5': (FUN, '        last_cause_ = RejectCause::kArmijo;', '        last_cause_ = RejectCause::kFunnelDecrease;'),
    # c6: the strategy reports no cause for a ceiling rejection.
    'c6': (FUN, '            last_cause_ = RejectCause::kFunnelCeiling;', '            last_cause_ = RejectCause::kNone;'),
    # f1: an h-type acceptance is counted as f-type.
    'f1': (SQP, '            } else {\n                ++st.out.counters.accepted_h_type;',
           '            } else {\n                ++st.out.counters.accepted_f_type;'),
    # r1: the switching ratio is unwired (never stored on the row).
    'r1': (SQP, '    mj.row.switching_ratio = detail::switching_ratio_value(\n        mj.ctx.pred_df, detail::kFunnelDelta * mj.ctx.h_old * mj.ctx.h_old);\n', ''),
    # r2: the ratio divides by the trial's h instead of the iterate's.
    'r2': (SQP, 'mj.ctx.pred_df, detail::kFunnelDelta * mj.ctx.h_old * mj.ctx.h_old);',
           'mj.ctx.pred_df, detail::kFunnelDelta * mj.ctx.h_new * mj.ctx.h_new);'),
    # r3: the ratio drops the delta factor.
    'r3': (SQP, 'mj.ctx.pred_df, detail::kFunnelDelta * mj.ctx.h_old * mj.ctx.h_old);',
           'mj.ctx.pred_df, mj.ctx.h_old * mj.ctx.h_old);'),
    # r4: the trace writer drops the key (the row still carries it).
    'r4': (TRW, '    key_opt_double(b, f, "switching_ratio", r.switching_ratio);\n', ''),
    # r5: the row builder drops the ratio (the driver's row has it; the record does not).
    'r5': (SQP, '    row.switching_ratio = r.switching_ratio;\n', ''),
    # h1: the histogram is off by one (position p lands in bucket p, the last bucket absorbing).
    'h1': (SQP, 'std::max<Index>(run_position, 1)) - 1, last)]', 'std::max<Index>(run_position, 1)), last)]'),
    # h2: the histogram's last bucket is not the "5 or more" bucket (it saturates one early).
    'h2': (SQP, 'const std::size_t last = c.rejection_run_histogram.size() - 1;',
           'const std::size_t last = c.rejection_run_histogram.size() - 2;'),
    # h3: the routed-failure site does not advance the run (always position 1).
    'h3': (SQP, '            count_rejection(st.out.counters, detail::RejectCause::kNone, st.counted_run_at_iterate);',
           '            count_rejection(st.out.counters, detail::RejectCause::kNone, 1);'),
    # n1 (r4): the ratio's call site restored to the plain division (the original defect: FE_DIVBYZERO at h == 0).
    'n1': (SQP, 'detail::switching_ratio_value(\n        mj.ctx.pred_df, detail::kFunnelDelta * mj.ctx.h_old * mj.ctx.h_old);',
           'mj.ctx.pred_df / (detail::kFunnelDelta * mj.ctx.h_old * mj.ctx.h_old);'),
    # n1b (r4): the helper's zero-denominator branch removed (plain division inside the helper).
    'n1b': (HDR, '    const double quotient = pred_df / (zero_denominator ? 1.0 : denominator);', '    const double quotient = pred_df / denominator;'),
    # n2 (r4): the non-finite rollback advances the histogram's run (the pre-r4 behaviour of the shared counter).
    'n2': (SQP, '        st.counted_run_at_iterate = st.rollback.counted_run_at_iterate;\n',
           '        st.counted_run_at_iterate = st.rollback.counted_run_at_iterate + 1;\n'),
    # n1c (r4d): the helper reduced to the plain division (what the pre-r4 call site computed), no zero-denominator case.
    'n1c': (HDR, '    const bool zero_denominator = denominator == 0.0;\n    const double quotient = pred_df / (zero_denominator ? 1.0 : denominator);\n    if (!zero_denominator) {\n        return quotient;\n    }\n    if (pred_df == 0.0 || std::isnan(pred_df)) {\n        return std::numeric_limits<double>::quiet_NaN();\n    }\n    return std::copysign(std::numeric_limits<double>::infinity(), pred_df);\n',
            '    return pred_df / denominator;\n'),
    # x1 (REPORTED, not must-kill): the cause is read after the second-order correction's own judgement.
    'x1': (SQP, '    const detail::RejectCause reject_cause = verdict == StepVerdict::kReject\n                                                 ? st.strategy->last_reject_cause()\n                                                 : detail::RejectCause::kNone;\n',
           '    detail::RejectCause reject_cause = detail::RejectCause::kNone;\n'),
}

root, mid = sys.argv[1], sys.argv[2]
path, old, new = M[mid]
p = root + '/' + path
s = open(p).read()
n = s.count(old)
if n != 1:
    print('MATCH_COUNT %s %d' % (mid, n))
    sys.exit(2)
s = s.replace(old, new)
if mid == 'x1':  # second half: read it where the rejection is counted, i.e. after SOC
    old2 = '        ++st.rejections_at_iterate;\n        ++st.counted_run_at_iterate;\n        ++st.out.counters.rejected_steps;\n        count_rejection(st.out.counters,\n                        reject_cause == detail::RejectCause::kNone'
    new2 = '        reject_cause = st.strategy->last_reject_cause();\n        ++st.rejections_at_iterate;\n        ++st.counted_run_at_iterate;\n        ++st.out.counters.rejected_steps;\n        count_rejection(st.out.counters,\n                        reject_cause == detail::RejectCause::kNone'
    if s.count(old2) != 1:
        print('MATCH_COUNT x1b %d' % s.count(old2))
        sys.exit(2)
    s = s.replace(old2, new2)
open(p, 'w').write(s)
print('APPLIED %s %s' % (mid, path))
