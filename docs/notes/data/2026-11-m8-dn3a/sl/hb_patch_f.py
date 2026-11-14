#!/usr/bin/env python3
# M8 DN-3a: applied to DN-1d r3's commit E (branch dn1d-legs-r3: six SQP side columns), lanes/hven/hven_bench_cell.cpp.
# THREE SQP side columns appended after dn1d_dense_shifted_terminations; nothing else moves. They read the
# library's INTERIM detail-only process tally (include/hven/detail/kkt/kkt_calls.h, dense_solve_check_tally();
# one cell per process, so the process tally is the cell's):
#   side.dn3a_ssn_dense_solve_refusals    (SSN Newton steps the unshifted dense solve check refused)
#   side.dn3a_ipqp_dense_solve_refusals   (IPQP step solves the check refused)
#   side.dn3a_gram_dense_refusals         (rank-repair Gram factorizations refused on the dense arm)
# Also: the DN-1c column dn1c_dense_condition_fallbacks keeps its name and reads SqpCounters::dense_platform_retakes,
# the same counter renamed by the DN-1d riders (hven 9c6059593, C286/C287); without it commit E does not compile
# against a pin after the rename.
# usage: hb_patch_f.py <lane cpp> [rename-only]   (rename-only: the BASE pin, which has no tally; no column added)
# Fail closed: each anchor once.
import sys

p = sys.argv[1]
rename_only = len(sys.argv) > 2 and sys.argv[2] == 'rename-only'
s = open(p).read()
ren = 'static_cast<long long>(c.dense_condition_fallbacks)'
if s.count(ren) != 1:
    sys.exit(f'HB_PATCH_F_RENAME {s.count(ren)}x')
s = s.replace(ren, 'static_cast<long long>(c.dense_platform_retakes)')
if not rename_only:
    inc = '#include <hven/hven.h>\n'
    if s.count(inc) != 1:
        sys.exit(f'HB_PATCH_F_INCLUDE {s.count(inc)}x')
    s = s.replace(inc, inc + '#include <hven/detail/kkt/kkt_calls.h>\n')
    old = '''    add_side(report, "dn1d_dense_shifted_terminations",
             static_cast<long long>(c.dense_shifted_terminations));
'''
    if s.count(old) != 1:
        sys.exit(f'HB_PATCH_F_ANCHOR {s.count(old)}x')
    s = s.replace(old, old + '''    add_side(report, "dn3a_ssn_dense_solve_refusals",
             static_cast<long long>(hven::detail::dense_solve_check_tally().ssn_refusals.load()));
    add_side(report, "dn3a_ipqp_dense_solve_refusals",
             static_cast<long long>(hven::detail::dense_solve_check_tally().ipqp_refusals.load()));
    add_side(report, "dn3a_gram_dense_refusals",
             static_cast<long long>(hven::detail::dense_solve_check_tally().gram_refusals.load()));
''')
open(p, 'w').write(s)
print('HB_PATCH_F_OK' + (' (rename-only)' if rename_only else ''))
