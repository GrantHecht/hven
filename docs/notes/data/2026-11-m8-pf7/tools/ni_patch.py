#!/usr/bin/env python3
"""Variant NI: compile the instrumentation emission guards out.  usage: ni_patch.py <source root>  (an untouched copy of the base; edits in place)."""
import re, sys
root = sys.argv[1]
files = ['src/ipm/ipm_solver.cpp', 'src/qp/ipqp/ipqp_engine.cpp', 'src/qp/walk/walk_engine.cpp', 'src/sqp/sqp_solver.cpp']
for f in files:
    p = f"{root}/{f}"; s = open(p).read()
    s = re.sub(r'\bthis->user_trace_ != nullptr', 'false', s)
    s = re.sub(r'\buser_trace_ != nullptr', 'false', s)
    s = re.sub(r'\bthis->ipqp_trace_ != nullptr', 'false', s)
    s = re.sub(r'\bipqp_trace_ != nullptr', 'false', s)
    s = re.sub(r'\bthis->trace_ != nullptr', 'false', s)
    s = re.sub(r'\btrace_ != nullptr', 'false', s)
    s = s.replace('TraceSink *const sink = this->trace_; sink != nullptr', 'TraceSink *const sink = nullptr; sink != nullptr')
    s = re.sub(r'\bthis->ledger_ == nullptr', 'true', s)
    s = re.sub(r'\bledger_ == nullptr', 'true', s)
    s = re.sub(r'\bthis->ledger_ != nullptr', 'false', s)
    s = re.sub(r'\bledger_ != nullptr', 'false', s)
    s = re.sub(r'\btrace_solve_open_\b(?=\))', 'false', s)
    s = s.replace('this->false', 'false')
    open(p, 'w').write(s)
