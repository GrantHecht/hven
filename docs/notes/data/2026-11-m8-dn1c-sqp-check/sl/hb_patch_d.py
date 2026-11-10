#!/usr/bin/env python3
# M8 DN-1c S legs: the LOCAL hven-bench clone's commit D (on top of DN-1b's commit C on branch dn1b-legs, which carries
# DN-1's commit B and its own IPM side column), applied to lanes/hven/hven_bench_cell.cpp. ONE side column appended after
# DN-1's SQP verdict columns; nothing else moves.
#   side.dn1c_dense_condition_fallbacks   (SQP: SqpCounters::dense_condition_fallbacks)
# Fail closed: the anchor once.
import sys

p = sys.argv[1]
s = open(p).read()
old = '''    add_side(report, "dn1_inertia_wrong_verdicts", static_cast<long long>(c.inertia_wrong_verdicts));
'''
if s.count(old) != 1:
    sys.exit(f'HB_PATCH_D_ANCHOR {s.count(old)}x')
s = s.replace(old, old + '''    add_side(report, "dn1c_dense_condition_fallbacks",
             static_cast<long long>(c.dense_condition_fallbacks));
''')
open(p, 'w').write(s)
print('HB_PATCH_D_OK')
