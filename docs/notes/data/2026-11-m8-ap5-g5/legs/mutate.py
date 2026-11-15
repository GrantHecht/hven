#!/usr/bin/env python3
# M8 AP-5 G5 (../DECLARATION.md): applies mutation <id> to the scratch source copy (structure from G4 legs-r4/mutate.py).
# Each old string must occur exactly once (fail closed otherwise). usage: mutate.py <src-root> <id>
import sys

root, mid = sys.argv[1], sys.argv[2]
SQP = 'src/drivers/sqp_solver.cpp'
IPM = 'src/drivers/ipm_solver.cpp'
PROB = 'src/model/problem.cpp'
ROWS = 'src/drivers/callback_rows.cpp'
SNAP = 'src/drivers/solve_result.cpp'
HDR = 'include/hven/drivers/solve_result.h'
M = {
    # interior-point event
    'a': (IPM, '''    const bool rows_out = want_constraints && !restoration_contaminated;''',
               '''    const bool rows_out = false && want_constraints;'''),
    'b': (IPM, '''    const bool sides_out = want_sides && !restoration_contaminated;''',
               '''    const bool sides_out = want_sides;'''),
    'c': (IPM, '''                lo[this->bounds_->lower_idx_[k]] += duals.z_lower_[k];
            for (int k = 0; k < nu; k++)
                up[this->bounds_->upper_idx_[k]] += duals.z_upper_[k];''',
               '''                up[this->bounds_->lower_idx_[k]] += duals.z_lower_[k];
            for (int k = 0; k < nu; k++)
                lo[this->bounds_->upper_idx_[k]] += duals.z_upper_[k];'''),
    'd': (IPM, '''    this->reached_step_info_ = IpmIterationInfo{};

    Runtimer.start();''', '''
    Runtimer.start();'''),
    'e': (IPM, '''            reached.alpha_primal = stepped.alpha_p;''', '''            reached.alpha_primal = 1.0;'''),
    'f': (IPM, '''        this->best_info_scratch_ = this->reached_step_info_;''', ''''''),
    'g': (IPM, '''    detail::compose_event_rows(this->callback_rows_, pt.ce, pt.ci, snap.g, snap.violation);''', ''''''),
    'h': (IPM, '''            reached.line_search_trials = static_cast<Index>(stepped.ls_iters);''', ''''''),
    # SQP event
    'i': (SQP, '''    const bool want_rows = payload_requests(iteration_payload_, IterationPayload::kConstraints) &&''',
               '''    const bool want_rows = false &&'''),
    'j': (SQP, '''        .z = want_vectors ? mj.event_z : no_vector,''', '''        .z = mj.event_z,'''),
    'k': (SQP, '''                        info->depth = sub_depth + 1;''', '''                        info->depth = sub_depth;'''),
    'l': (SQP, '''            st.mb.snap.sqp = sqp_iteration_info(exported, 0);''', ''''''),
    'm': (SQP, '''    info.violation_l1 = row.violation_l1;''', '''    info.violation_l1 = row.feasibility;'''),
    'n': (SQP, '''    detail::split_bound_multipliers(best.z, best.z_lower, best.z_upper);''', ''''''),
    # shared maps
    'o': (ROWS, '''        z_lower[k] = v > 0.0 ? v : (v <= 0.0 ? 0.0 : v);
        z_upper[k] = v < 0.0 ? -v : (v >= 0.0 ? 0.0 : v);''',
                '''        z_upper[k] = v > 0.0 ? v : (v <= 0.0 ? 0.0 : v);
        z_lower[k] = v < 0.0 ? -v : (v >= 0.0 ? 0.0 : v);'''),
    'p': (PROB, '''    static double positive_part(double v) { return std::isnan(v) ? v : std::max(v, 0.0); }''',
                '''    static double positive_part(double v) { return v; }'''),
    'q': (PROB, '''                    violation[r] = std::abs(ce[rows.eq_row[r]]);''',
                '''                    violation[r] = ce[rows.eq_row[r]];'''),
    'r': (PROB, '''                case NlpRowKind::kFree:
                    violation[r] = 0.0;''', '''                case NlpRowKind::kFree:
                    violation[r] = std::numeric_limits<double>::quiet_NaN();'''),
    's': (SNAP, '''    out.g = event.g;\n''', ''''''),
    't': (SNAP, '''    out.ipm = event.ipm;\n''', ''''''),
    'u': (HDR, '''inline constexpr IterationPayload kDefaultIterationPayload =
    IterationPayload::kVectors | IterationPayload::kDiagnostics;''',
               '''inline constexpr IterationPayload kDefaultIterationPayload = IterationPayload::kAll;'''),
    'v': (SQP, '''    sweep_negative_face_prices(lambda_i, discarded);''', ''''''),
}
if mid not in M:
    sys.exit('unknown mutation ' + mid)
path, old, new = M[mid]
p = root + '/' + path
s = open(p).read()
n = s.count(old)
if n != 1:
    sys.exit('mutation %s: old string occurs %d times in %s' % (mid, n, path))
open(p, 'w').write(s.replace(old, new, 1))
print('applied', mid, path)
