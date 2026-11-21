#!/usr/bin/env python3
# M8 DN-1b S legs: the LOCAL hven-bench clone's commit C (on top of DN-1's commit B, hb_patch.py), applied to
# lanes/hven/hven_bench_cell.cpp. ONE side column appended after DN-1's IPM side columns; nothing else moves.
#   side.dn1b_dense_residual_ratio_triggers   (IPM: IpmResult::dense_residual_ratio_triggers)
# Fail closed: the anchor once.
import sys

p = sys.argv[1]
s = open(p).read()
old = '''    add_side(report, "dn1_kkt_dense_fallback_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.dense_fallback_count));
'''
if s.count(old) != 1:
    sys.exit(f'HB_PATCH_C_ANCHOR {s.count(old)}x')
s = s.replace(old, old + '''    add_side(report, "dn1b_dense_residual_ratio_triggers",
             static_cast<long long>(ipm_result.dense_residual_ratio_triggers));
''')
open(p, 'w').write(s)
print('HB_PATCH_C_OK')
