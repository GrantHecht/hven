#!/usr/bin/env python3
"""M8 TR-D (../DECLARATION.md M): applies ONE named mutation to a scratch copy of the code head.
usage: mutate.py <copy-root> <id>      Each mutation is an exact text replacement that must match exactly once."""
import sys

LOOP = '    for (const IpmIterate &row : iters) {\n        this->result_.history.push_back(this->make_iteration_record(row));\n    }\n'
M = {
    # d1: a history row is dropped (the IPM drops the last record of every phase).
    'd1': ('src/drivers/ipm_solver.cpp', LOOP,
           '    for (std::size_t k = 0; k + 1 < iters.size(); ++k) {\n        this->result_.history.push_back(this->make_iteration_record(iters[k]));\n    }\n'),
    # d2: a history row is dropped (the SQP stops pushing after two rows; the trace still writes them).
    'd2': ('src/drivers/sqp_solver.cpp',
           '        st.out.history.push_back(std::move(record));\n        ++st.rows_pushed;\n',
           '        if (st.out.history.size() != 2) {\n            st.out.history.push_back(std::move(record));\n            ++st.rows_pushed;\n        }\n'),
    # d3: an IPM field miswired in the history row only (complementarity read from stationarity).
    'd3': ('src/drivers/ipm_solver.cpp', LOOP,
           '    for (const IpmIterate &row : iters) {\n        IterationRecord rec = this->make_iteration_record(row);\n        rec.complementarity = rec.stationarity;\n        this->result_.history.push_back(rec);\n    }\n'),
    # d4: an SQP field miswired in the history row only (trust_radius read from violation_l1), after the trace emit.
    'd4': ('src/drivers/sqp_solver.cpp',
           '        st.out.history.push_back(std::move(record));\n        ++st.rows_pushed;\n',
           '        record.sqp->trust_radius = record.sqp->violation_l1;\n        st.out.history.push_back(std::move(record));\n        ++st.rows_pushed;\n'),
    # d5: `factorizations` is not written on the SQP row.
    'd5': ('src/drivers/sqp_solver.cpp', '    rec.factorizations = factorizations;\n    SqpIterationRow row;',
           '    (void)factorizations;\n    SqpIterationRow row;'),
    # d6i: the IPM history is built with tracing on only.
    'd6i': ('src/drivers/ipm_solver.cpp', LOOP,
            '    if (this->trace_ != nullptr) {\n        for (const IpmIterate &row : iters) {\n            this->result_.history.push_back(this->make_iteration_record(row));\n        }\n    }\n'),
    # d6s: the SQP history rows are real only with a sink attached (an empty row otherwise).
    'd6s': ('src/drivers/sqp_solver.cpp', 'is_restoration_sub_driver_ ? 1 : 0, st.out.counters.factorizations);\n',
            'is_restoration_sub_driver_ ? 1 : 0, st.out.counters.factorizations);\n        if (ipqp_trace_ == nullptr) {\n            record = IterationRecord{};\n        }\n'),
    # d7: the IPM provenance stamp is omitted (rows never read recovering / objective_bearing).
    'd7': ('src/drivers/ipm_solver.cpp',
           '    row.recovering = this->cur_eval_prov_.restoration_active ||\n                     (this->restoration_ != nullptr && this->restoration_->is_active());\n    row.objective_bearing = this->cur_eval_prov_.objective_bearing;\n',
           '    row.recovering = false;\n    row.objective_bearing = false;\n'),
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
