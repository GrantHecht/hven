#!/usr/bin/env python3
# M8 DN-1 S and M legs (../DECLARATION-dn1legs.txt): the LOCAL hven-bench clone's commit B, applied to
# lanes/hven/hven_bench_cell.cpp. Two label PREFIXES and four side columns; nothing else moves (the `default`
# label path, every existing column and every other label are untouched). Fail closed: every anchor once.
#   dense-<base>  <base> with linear.dense_factorization = DensePolicy::kAlways (both engines)
#   t0-<base>     <base> with common.threads = 0 (the engines' default: MKL's own thread count)
#   side.dn1_kkt_dense_factorize_count / side.dn1_kkt_dense_fallback_count   (IPM: IpmResult::kkt_factor_counters)
#   side.dn1_inertia_suspect_verdicts / side.dn1_inertia_wrong_verdicts         (SQP: SqpCounters)
import sys

p = sys.argv[1]
s = open(p).read()


def once(old, new):
    global s
    if s.count(old) != 1:
        sys.exit(f'HB_PATCH_ANCHOR {s.count(old)}x: {old[:60]!r}')
    s = s.replace(old, new)


once('''hven::IpmOptions ipm_options(const std::string &label) {
''', '''/// M8 DN-1, LOCAL EVIDENCE CLONE ONLY (never pushed): hven's DN-1 dense-arm legs' two label prefixes, peeled
/// before everything else. `dense-<base>` sets linear.dense_factorization = kAlways; `t0-<base>` sets
/// common.threads = 0 (the engines' default thread setting).
constexpr const char *kDn1DensePrefix = "dense-";
constexpr const char *kDn1T0Prefix = "t0-";

hven::IpmOptions ipm_options(const std::string &label) {
    if (label.rfind(kDn1T0Prefix, 0) == 0) {
        auto t = ipm_options(label.substr(std::strlen(kDn1T0Prefix)));
        t.common.threads = 0;
        return t;
    }
    if (label.rfind(kDn1DensePrefix, 0) == 0) {
        auto t = ipm_options(label.substr(std::strlen(kDn1DensePrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kAlways;
        return t;
    }
''')
once('''hven::SqpOptions sqp_options(const std::string &label) {
''', '''hven::SqpOptions sqp_options(const std::string &label) {
    if (label.rfind(kDn1T0Prefix, 0) == 0) {
        auto t = sqp_options(label.substr(std::strlen(kDn1T0Prefix)));
        t.common.threads = 0;
        return t;
    }
    if (label.rfind(kDn1DensePrefix, 0) == 0) {
        auto t = sqp_options(label.substr(std::strlen(kDn1DensePrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kAlways;
        return t;
    }
''')
once('''    add_side(report, "ipm_kkt_inf", ipm_result.convergence.kkt);
''', '''    add_side(report, "ipm_kkt_inf", ipm_result.convergence.kkt);
    add_side(report, "dn1_kkt_dense_factorize_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.dense_factorize_count));
    add_side(report, "dn1_kkt_dense_fallback_count",
             static_cast<long long>(ipm_result.kkt_factor_counters.dense_fallback_count));
''')
once('''    add_side(report, "sqp_kkt_residual", sqp_result.convergence.kkt_residual);
''', '''    add_side(report, "sqp_kkt_residual", sqp_result.convergence.kkt_residual);
    add_side(report, "dn1_inertia_suspect_verdicts", static_cast<long long>(c.inertia_suspect_verdicts));
    add_side(report, "dn1_inertia_wrong_verdicts", static_cast<long long>(c.inertia_wrong_verdicts));
''')
open(p, 'w').write(s)
print('HB_PATCH_OK')
