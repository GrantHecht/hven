#!/usr/bin/env python3
# M8 DN-1d (../DECLARATION.txt): structure from ../../2026-11-m8-dn1c-sqp-check/sl/hb_patch_d.py. Applied to DN-1b's
# commit C (branch dn1b-legs: DN-1's commit B and its IPM side column), lanes/hven/hven_bench_cell.cpp. FOUR SQP side
# columns appended after DN-1's SQP verdict columns; nothing else moves.
#   side.dn1c_dense_condition_fallbacks     (SqpCounters::dense_condition_fallbacks: the last-resort platform retakes)
#   side.dn1d_dense_flagged_factorizations  (SqpCounters::dense_flagged_factorizations)
#   side.dn1d_dense_singular_refactors      (SqpCounters::dense_singular_refactors)
#   side.dn1d_dense_shifted_solve_refusals  (SqpCounters::dense_shifted_solve_refusals)
# Fail closed: the anchor once.
import sys

p = sys.argv[1]
s = open(p).read()
old = '''    add_side(report, "dn1_inertia_wrong_verdicts", static_cast<long long>(c.inertia_wrong_verdicts));
'''
if s.count(old) != 1:
    sys.exit(f'HB_PATCH_E_ANCHOR {s.count(old)}x')
s = s.replace(old, old + '''    add_side(report, "dn1c_dense_condition_fallbacks",
             static_cast<long long>(c.dense_condition_fallbacks));
    add_side(report, "dn1d_dense_flagged_factorizations",
             static_cast<long long>(c.dense_flagged_factorizations));
    add_side(report, "dn1d_dense_singular_refactors",
             static_cast<long long>(c.dense_singular_refactors));
    add_side(report, "dn1d_dense_shifted_solve_refusals",
             static_cast<long long>(c.dense_shifted_solve_refusals));
''')
open(p, 'w').write(s)
print('HB_PATCH_E_OK')
