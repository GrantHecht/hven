#!/usr/bin/env python3
# M8 DN-3 (../DECLARATION-r1.md): applied AFTER hb_patch_g.py and hb_patch_h.py (DN-3a r2's and DN-3b's, byte copies
# beside this file) to DN-1d r3's commit E (branch dn1d-legs-r3), lanes/hven/hven_bench_cell.cpp. Nothing else moves:
#   label prefix `auto-<base>`           linear.dense_factorization = kAuto on both engines (peeled beside `dense-`,
#                                        which sets kAlways); `auto-default` is the adoption leg's kAuto arm
#   side.dn3_dense_factorizations (SQP)  SqpCounters::dense_factorizations
#   side.dn3_dense_factorizations (IPM)  IpmResult::dense_factorizations
# usage: hb_patch_i.py <lane cpp>   Fail closed: each anchor once.
import sys

p = sys.argv[1]
s = open(p).read()


def once(old, new, tag):
    global s
    if s.count(old) != 1:
        sys.exit(f'HB_PATCH_I_{tag} {s.count(old)}x')
    s = s.replace(old, new)


once('constexpr const char *kDn1T0Prefix = "t0-";\n',
     'constexpr const char *kDn1T0Prefix = "t0-";\n'
     '/// M8 DN-3, LOCAL EVIDENCE CLONE ONLY: `auto-<base>` sets linear.dense_factorization = kAuto.\n'
     'constexpr const char *kDn3AutoPrefix = "auto-";\n', 'PREFIX')
once('''    if (label.rfind(kDn1DensePrefix, 0) == 0) {
        auto t = ipm_options(label.substr(std::strlen(kDn1DensePrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kAlways;
        return t;
    }
''', '''    if (label.rfind(kDn1DensePrefix, 0) == 0) {
        auto t = ipm_options(label.substr(std::strlen(kDn1DensePrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kAlways;
        return t;
    }
    if (label.rfind(kDn3AutoPrefix, 0) == 0) {
        auto t = ipm_options(label.substr(std::strlen(kDn3AutoPrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kAuto;
        return t;
    }
''', 'IPM_LABEL')
once('''    if (label.rfind(kDn1DensePrefix, 0) == 0) {
        auto t = sqp_options(label.substr(std::strlen(kDn1DensePrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kAlways;
        return t;
    }
''', '''    if (label.rfind(kDn1DensePrefix, 0) == 0) {
        auto t = sqp_options(label.substr(std::strlen(kDn1DensePrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kAlways;
        return t;
    }
    if (label.rfind(kDn3AutoPrefix, 0) == 0) {
        auto t = sqp_options(label.substr(std::strlen(kDn3AutoPrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kAuto;
        return t;
    }
''', 'SQP_LABEL')
once('''    add_side(report, "dn3a_dense_solve_residual_refusals",
             static_cast<long long>(c.dense_solve_residual_refusals));
''', '''    add_side(report, "dn3a_dense_solve_residual_refusals",
             static_cast<long long>(c.dense_solve_residual_refusals));
    add_side(report, "dn3_dense_factorizations", static_cast<long long>(c.dense_factorizations));
''', 'SQP_COLUMN')
once('''    add_side(report, "dn3b_resto_entries",
             static_cast<long long>(ipm_result.last_feasibility_restoration_entries.value_or(-1)));
''', '''    add_side(report, "dn3b_resto_entries",
             static_cast<long long>(ipm_result.last_feasibility_restoration_entries.value_or(-1)));
    add_side(report, "dn3_dense_factorizations",
             static_cast<long long>(ipm_result.dense_factorizations));
''', 'IPM_COLUMN')
open(p, 'w').write(s)
print('HB_PATCH_I_OK')
