#!/usr/bin/env python3
# M8 AP-5 G4 (../DECLARATION.md): applies mutation <id> to the scratch source copy. Each old string must occur
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
        return abandon_stopped_qp();
    }''', '''    if (false) {
        return abandon_stopped_qp();
    }'''),
    'h': (SQP, '''        if (detail::stop_flag_set(qp_stop_)) [[unlikely]] {
            return abandon_stopped_qp();
        }''', '''        if (false) {
            return abandon_stopped_qp();
        }'''),
    'i': (SQP, '        mj.row = *row_at_xk;\n', '        (void)row_at_xk;\n'),
    'j': (SQP, '        st.out.counters.major_iters = iter;\n', ''),
    'k': (SQP, '        engine_->set_stop_flag(nullptr); // the SOC re-solve is not a safe point (G4)\n', ''),
    'l': (SQP, '        ssn_engine_->set_stop_flag(qp_stop_);\n', ''),
    'm': (SQP, '        ipqp_engine_->set_stop_flag(qp_stop_);\n', ''),
    'n': (SQP, '''    // A STOP (M8 AP-5 G4): run_major abandons this subproblem; no walk is started.
    if (detail::stop_flag_set(qp_stop_)) [[unlikely]] {''', '''    if (false) {'''),
    'o': (SQP, '''    // A STOP (M8 AP-5 G4): run_major abandons this subproblem; no fallback runs.
    if (detail::stop_flag_set(qp_stop_)) [[unlikely]] {''', '''    if (false) {'''),
    'p': (SQP, '''    if (detail::stop_flag_set(engine.stop_flag())) [[unlikely]] {
        return QpSolution{};''', '''    if (false) {
        return QpSolution{};'''),
    'q': (SQP, '''        ~QpStopGuard() { self->arm_qp_stop(nullptr); }''', '''        ~QpStopGuard() {}'''),
    'r': (SQP, '''        mj.token_exit = !st.interrupted;
        push_history(mj.row);''', '''        push_history(mj.row);'''),
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
