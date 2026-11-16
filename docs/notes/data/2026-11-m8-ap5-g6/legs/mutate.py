#!/usr/bin/env python3
# M8 AP-5 G6 (../DECLARATION.md): applies mutation <id> to the scratch source copy. Pattern of ../../2026-11-m8-ap5-g5/legs-r3/mutate.py.
# Each old string must occur the stated number of times (fail closed otherwise). usage: mutate.py <src-root> <id>
import re, sys

root, mid = sys.argv[1], sys.argv[2]
IPM = 'src/drivers/ipm_solver.cpp'
GLOB = 'src/drivers/ipm_solver_globalization.cpp'
REST = 'include/hven/detail/globalization/restoration.h'
CNT = 'include/hven/core/solver_counters.h'
TYPES = 'include/hven/drivers/ipm_solver_types.h'
TRACEW = 'src/drivers/trace_writer.cpp'
# (file, old, new, expected count)
M = {
    # the X-macro wiring
    'm1': (CNT, '''    X(dense_factorizations, counter_never_absent)

/// @brief `HVEN_IPM_COUNTERS_FIELDS`''', '''
/// @brief `HVEN_IPM_COUNTERS_FIELDS`''', 1),
    'm2': (CNT, '''    X(soc_steps, counter_never_absent)                                                             \\
    X(nonfinite_step_rollbacks, counter_never_absent)                                              \\
    X(polish_ignored, counter_never_absent)                                                        \\
    X(payload_ignored, counter_never_absent)                                                       \\
    X(watchdog_activations, counter_never_absent)                                                  \\
    X(recovery_depth_histogram, counter_never_absent)                                              \\
    X(dense_solve_residual_refusals, counter_never_absent)                                         \\
    X(dense_factorizations, counter_never_absent)''', '''    X(dense_factorizations, counter_never_absent)                                                  \\
    X(nonfinite_step_rollbacks, counter_never_absent)                                              \\
    X(polish_ignored, counter_never_absent)                                                        \\
    X(payload_ignored, counter_never_absent)                                                       \\
    X(watchdog_activations, counter_never_absent)                                                  \\
    X(recovery_depth_histogram, counter_never_absent)                                              \\
    X(dense_solve_residual_refusals, counter_never_absent)                                         \\
    X(soc_steps, counter_never_absent)''', 1),
    # the counters group
    'c1': (IPM, '    result.counters.factorizations = result.work.factorizations;\n', '', 2),
    'c1b': (IPM, '    out.counters.factorizations = out.work.factorizations;\n', '', 1),
    'c2': (IPM, '    this->result_.counters.symbolic_analyses = this->kkt_analysis_count_ - analyses_before;\n', '', 1),
    'c3': (IPM, '''                this->result_.counters.soc_steps, resolved_depth,
                this->result_.counters.watchdog_activations);''', '''                this->result_.counters.watchdog_activations, resolved_depth,
                this->result_.counters.soc_steps);''', 1),
    'c4': (IPM, '''                this->result_.counters.soc_steps, resolved_depth,
                this->result_.counters.watchdog_activations);''', '''                this->result_.counters.soc_steps, resolved_depth,
                *std::make_unique<Index>(0));''', 1),
    'c5': (IPM, '            this->result_.counters.recovery_depth_histogram[resolved_depth]++;\n', '', 2),
    'c6': (IPM, '            ++this->result_.counters.nonfinite_step_rollbacks;\n', '', 1),
    'c7': (IPM, '                this->result_.counters.polish_ignored = 1;\n', '', 1),
    'c8': (IPM, '            this->result_.counters.payload_ignored = 1;\n', '', 1),
    'c9': (IPM, '++this->result_.counters.dense_solve_residual_refusals;', ';', 2),
    'c10': (IPM, r'RE:(result|out)\.counters\.dense_factorizations =\s*this->lifetime_dense_factorize_count\(\) - this->dense_factorizations_at_entry_;', 'RE=\\1.counters.dense_factorizations = 0;', 3),
    'c11': (IPM, '        result.counters.restoration_entries += phase.restoration_entries.value_or(0);\n', '', 1),
    'c12': (IPM, '        result.counters.restoration_iters += phase.restoration_iters.value_or(0);\n', '', 1),
    'c13': (IPM, '''        result.counters.restoration_entries += phase.restoration_entries.value_or(0);''',
                 '''        result.counters.restoration_entries = phase.restoration_entries.value_or(0);''', 1),
    # the per-phase fields
    'p1': (GLOB, '    report.funnel_width =\n', '    (void)\n', 1),
    'p2': (GLOB, '    report.filter_size = static_cast<Index>(filter_.size());\n', '', 1),
    'p3': (GLOB, '    report.filter_resets = n_filter_resets_;\n', '', 1),
    'p4': (GLOB, '    report.monotone_switches = last_monotone_switches_;\n', '', 1),
    'p5': (GLOB, '    report.monotone_iterations = last_monotone_iters_;\n', '', 1),
    'p6': (REST, '        report.restoration_entries = entries_;\n', '', 1),
    'p7': (REST, '        report.restoration_iters = iterations_in_mode_;\n', '', 1),
    'p8': (IPM, '            report.proximal_regularization_primal = this->phase_prox_primal_;\n', '', 1),
    'p9': (IPM, '            report.proximal_regularization_dual = this->phase_prox_dual_;\n', '', 1),
    'p10': (IPM, '            report.kkt_factor_status = ipm_kkt_factor_status(this->phase_kkt_info_);\n', '', 1),
    'p11': (IPM, '''        this->acceptance_->append_diagnostics(
            this->result_.phases[static_cast<std::size_t>(current_phase_idx)]);''', '', 1),
    'p12': (IPM, '''        this->governor_->append_diagnostics(
            this->result_.phases[static_cast<std::size_t>(current_phase_idx)]);''', '', 1),
    'p13': (IPM, '''            this->restoration_->append_diagnostics(
                this->result_.phases[static_cast<std::size_t>(current_phase_idx)]);''', '            (void)0;', 1),
    'p14': (TYPES, '''        if (it->ran) {
            return &*it;
        }''', '''        return &*it;''', 1),
    'p15': (IPM, '    rec.counters = result.counters;\n', '', 1),
    'p16': (TRACEW, 'key_enum(b, first, "kkt_factor_status", to_json(event.kkt_factor_status));', 'key_enum(b, first, "last_kkt_info", to_json(event.kkt_factor_status));', 1),
}
if mid not in M:
    sys.exit(f'unknown mutation {mid}')
f, old, new, cnt = M[mid]
path = f'{root}/{f}'
s = open(path).read()
if old.startswith('RE:'):
    pat = old[3:]
    n = len(re.findall(pat, s))
    if n != cnt:
        sys.exit(f'APPLY_FAIL {mid}: pattern occurs {n}, expected {cnt}')
    s = re.sub(pat, new[3:], s)
else:
    n = s.count(old)
    if n != cnt:
        sys.exit(f'APPLY_FAIL {mid}: old string occurs {n}, expected {cnt}')
    s = s.replace(old, new)
open(path, 'w').write(s)
print(f'applied {mid} to {f}')
