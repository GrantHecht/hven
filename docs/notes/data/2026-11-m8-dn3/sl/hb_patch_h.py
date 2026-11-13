#!/usr/bin/env python3
# M8 DN-3b (../DECLARATION.txt, FA): applied AFTER hb_patch_g.py (DN-3a r2's, byte copy beside this file) to DN-1d r3's
# commit E (branch dn1d-legs-r3), lanes/hven/hven_bench_cell.cpp. FIVE IPM side columns appended after
# dn1b_dense_residual_ratio_triggers, read from the PUBLIC IpmResult (kkt_factor_counters: SymmetricFactor::Counters;
# last_feasibility_restoration_entries); nothing else moves:
#   side.dn3b_kkt_dense_singular_count         exactly singular dense reports (kReport only)
#   side.dn3b_kkt_dense_flagged_count          dense factorizations taken and flagged numerically singular (kReport only)
#   side.dn3b_kkt_dense_retake_count           retake_on_platform() calls (the SQP's last resort; never the IPM's)
#   side.dn3b_kkt_dense_shift_refactor_count   factorize_equilibrated() calls (the SQP's sigma shift; never the IPM's)
#   side.dn3b_resto_entries                    feasibility restoration entries (-1 when no strategy is constructed)
# The existing dn1_kkt_dense_fallback_count column (SymmetricFactor::Counters::dense_fallback_count) is the sparse-fallback
# count the FA leg reads.
# usage: hb_patch_h.py <lane cpp>   Fail closed: the anchor once.
import sys

p = sys.argv[1]
s = open(p).read()
old = '''    add_side(report, "dn1b_dense_residual_ratio_triggers",
             static_cast<long long>(ipm_result.dense_residual_ratio_triggers));
'''
if s.count(old) != 1:
    sys.exit(f'HB_PATCH_H_ANCHOR {s.count(old)}x')
s = s.replace(old, old + '''    add_side(report, "dn3b_kkt_dense_singular_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.dense_singular_count));
    add_side(report, "dn3b_kkt_dense_flagged_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.dense_flagged_count));
    add_side(report, "dn3b_kkt_dense_retake_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.dense_retake_count));
    add_side(report, "dn3b_kkt_dense_shift_refactor_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.dense_shift_refactor_count));
    add_side(report, "dn3b_resto_entries",
             static_cast<long long>(ipm_result.last_feasibility_restoration_entries.value_or(-1)));
''')
open(p, 'w').write(s)
print('HB_PATCH_H_OK')
