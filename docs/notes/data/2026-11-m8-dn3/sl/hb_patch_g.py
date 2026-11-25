#!/usr/bin/env python3
# M8 DN-3a r2 (../DECLARATION-r2.txt, FA): the r1 patch (../sl/hb_patch_f.py) with the interim tally replaced by the
# per-result counter. Applied to DN-1d r3's commit E (branch dn1d-legs-r3: six SQP side columns),
# lanes/hven/hven_bench_cell.cpp. ONE SQP side column appended after dn1d_dense_shifted_terminations; nothing else moves:
#   side.dn3a_dense_solve_residual_refusals   SqpCounters::dense_solve_residual_refusals (provisional name): SSN and
#                                             IPQP step solves the dense solve check refused, summed over the solve.
# Also, as r1: the DN-1c column dn1c_dense_condition_fallbacks keeps its name and reads SqpCounters::dense_platform_retakes
# (the same counter, renamed by the DN-1d riders, hven 9c6059593); without it commit E does not compile against a pin
# after the rename.
# usage: hb_patch_g.py <lane cpp>   Fail closed: each anchor once.
import sys

p = sys.argv[1]
s = open(p).read()
ren = 'static_cast<long long>(c.dense_condition_fallbacks)'
if s.count(ren) != 1:
    sys.exit(f'HB_PATCH_G_RENAME {s.count(ren)}x')
s = s.replace(ren, 'static_cast<long long>(c.dense_platform_retakes)')
old = '''    add_side(report, "dn1d_dense_shifted_terminations",
             static_cast<long long>(c.dense_shifted_terminations));
'''
if s.count(old) != 1:
    sys.exit(f'HB_PATCH_G_ANCHOR {s.count(old)}x')
s = s.replace(old, old + '''    add_side(report, "dn3a_dense_solve_residual_refusals",
             static_cast<long long>(c.dense_solve_residual_refusals));
''')
open(p, 'w').write(s)
print('HB_PATCH_G_OK')
