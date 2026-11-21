#!/usr/bin/env python3
# M8 AP-5 G4 (../DECLARATION-r3.md): applies mutation <id> to the scratch source copy. Each old string must occur
# exactly once (fail closed otherwise). usage: mutate.py <src-root> <id>
import sys

root, mid = sys.argv[1], sys.argv[2]
SQP = 'src/drivers/sqp_solver.cpp'
IPM = 'src/drivers/ipm_solver.cpp'
WALK = 'src/qp/qp_engine.cpp'
SSN = 'src/qp/ssn_engine.cpp'
IPQP = 'src/qp/ipqp_engine.cpp'
M = {
    # IPM ladder
    'a': (IPM, 'hven::detail::stop_flag(this->stop_token_));', 'nullptr);'),
    'b': (IPM, '''            iters.push_back(Citer);
            honour_stop(false);''', '''            honour_stop(false);'''),
    'c': (IPM, '''            honour_stop(false);''', '''            honour_stop(true);'''),
    # kernels: the poll ticks but the exit is ignored
    'd': (WALK, '''            break; // status is still kIterationLimit: the cap's exit''', '''            (void)0;'''),
    'e': (SSN, '''            out->escape_reason = SsnEscape::kBudget;
            break;
        }
        const detail::SsnNorms nrm''', '''            out->escape_reason = SsnEscape::kBudget;
        }
        const detail::SsnNorms nrm'''),
    'f': (IPQP, '''            escape = IpqpEscape::kBudget; // the iteration budget's exit
            break;''', '''            escape = IpqpEscape::kBudget; // the iteration budget's exit'''),
    # driver
    'g': (SQP, '''    if (detail::stop_flag_set(qp_stop_)) [[unlikely]] {
        return abandon_stopped_qp(true);
    }''', '''    if (false) {
        return abandon_stopped_qp(true);
    }'''),
    'h': (SQP, '''        if (detail::stop_flag_set(qp_stop_)) [[unlikely]] {
            return abandon_stopped_qp(true);
        }''', '''        if (false) {
            return abandon_stopped_qp(true);
        }'''),
    'i': (SQP, '        mj.row = *row_at_xk;\n', '        (void)row_at_xk;\n'),
    'j': (SQP, '        st.out.counters.major_iters = iter;\n', ''),
    'k': (SQP, '        engine_->set_stop_flag(nullptr); // the SOC re-solve is not a safe point (G4)\n', ''),
    'l': (SQP, '        ssn_engine_->set_stop_flag(qp_stop_);\n', ''),
    'm': (SQP, '        ipqp_engine_->set_stop_flag(qp_stop_);\n', ''),
    'n': (SQP, '''    // factorizations are charged here, as a hand-off's are, since `qs` will not carry them.
    if (detail::stop_flag_set(qp_stop_)) [[unlikely]] {''', '''    if (false) {'''),
    'o': (SQP, '''    // factorizations are charged here, as an escape's are.
    if (detail::stop_flag_set(qp_stop_)) [[unlikely]] {''', '''    if (false) {'''),
    'p': (SQP, '''    if (detail::stop_flag_set(engine.stop_flag())) [[unlikely]] {
        return QpSolution{};''', '''    if (false) {
        return QpSolution{};'''),
    'q': (SQP, '''        ~QpStopGuard() { self->arm_qp_stop(nullptr); }''', '''        ~QpStopGuard() {}'''),
    'r': (SQP, '''        return stop_exit(!st.interrupted);''', '''        return stop_exit(false);'''),
    # round-2 riders (DECLARATION-r3)
    's': (SQP, '''        charge_ssn_subproblem_cost(out.counters, sres);
        return;
    }''', '''        return;
    }'''),
    't': (SQP, '''        charge_ipqp_subproblem_cost(out.counters, ires);
        return;
    }''', '''        return;
    }'''),
    'u': (SQP, '''    if (detail::stop_flag_set(qp_stop_)) [[unlikely]] {
        return abandon_stopped_qp(false);
    }''', '''    if (false) {
        return abandon_stopped_qp(false);
    }'''),
    'v': (SQP, '''    if (detail::stop_flag_set(qp_stop_)) [[unlikely]] {
        charge_ssn_subproblem_cost(counters, sres);
        return false;
    }''', '''    if (false) {
        charge_ssn_subproblem_cost(counters, sres);
        return false;
    }'''),
    'w': (SQP, '''        engine_->set_stop_flag(qp_stop_);
        // ONE LINE FOR THE CORRECTION''', '''        // ONE LINE FOR THE CORRECTION'''),
    'x': (SQP, '''        mj.row_qp_mode = trace_mode_of(opts_.qp_mode);
        return stop_exit''', '''        return stop_exit'''),
    'y': (SQP, '''        if (detail::stop_flag_set(qp_stop_)) [[unlikely]] {
            walk_owns_this_qp = false;
            return;
        }''', '''        if (false) {
            walk_owns_this_qp = false;
            return;
        }'''),
    # the G3 merge points (DECLARATION-r3)
    'z': (IPM, '''            if (this->best_tracking_armed() &&
                !(this->restoration_ && this->restoration_->is_active())) {
                this->track_best_iterate(iters.back(), i, XSL, RHS, mu, last_step_norm,
                                         BestCriteriaVal, BestIter, best_recorded);
            }
            // kStopReturnBest: the best iterate''', '''            // kStopReturnBest: the best iterate'''),
    'Z': (SQP, '''        const bool stop_returns_best =
            st.interrupted && st.stop_return_best && !mj.token_exit && st.mb.snap_valid;''', '''        const bool stop_returns_best =
            st.interrupted && st.stop_return_best && st.mb.snap_valid;'''),
}
if mid not in M:
    sys.exit('unknown mutation ' + mid)
path, old, new = M[mid]
p = root + '/' + path
s = open(p).read()
n = s.count(old)
if n != 1:
    sys.exit('mutation %s: old string occurs %d times in %s' % (mid, n, path))
open(p, 'w').write(s.replace(old, new, 1))
print('applied', mid, path)
