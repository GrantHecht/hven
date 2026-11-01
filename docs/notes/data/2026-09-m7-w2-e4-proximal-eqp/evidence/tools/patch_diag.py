#!/usr/bin/env python3
"""E4 DIAGNOSTIC-ONLY scratch patch (apply|revert): env E4_OFF bitmask disables
one of E4's four new decision points so a failing fixture can be attributed.
  1 = the iterate-currency runaway guard
  2 = the step-currency runaway guard
  4 = the EQP-site residual REFUSAL exit
  8 = the stationarity disjunct of the KKT-point test
NEVER SHIPPED: it reads getenv from library code. Reverted before any measurement
that is quoted, and before the commit.
"""
import sys
P = 'src/qp/qp_engine.cpp'
PAIRS = [
 # (shipped, diagnostic)
 ("""        if (is_runaway(qp, x, ws, eff_opts)) {
            status = QpStatus::kNumericalError;""",
  """        if (is_runaway(qp, x, ws, eff_opts) && (e4_off_mask() & 1) == 0) {
            status = QpStatus::kNumericalError;"""),
 ("""        if (!(dx_inf > step_tol) &&
            p_inf > detail::unbounded_artifact_scale(eff_opts) * std::max(1.0, x_inf)) {""",
  """        if (!(dx_inf > step_tol) && (e4_off_mask() & 2) == 0 &&
            p_inf > detail::unbounded_artifact_scale(eff_opts) * std::max(1.0, x_inf)) {"""),
 ("""        if (eqp.residual_refused) {
            status = QpStatus::kNumericalError;""",
  """        if (eqp.residual_refused && (e4_off_mask() & 4) == 0) {
            status = QpStatus::kNumericalError;"""),
 ("""        const bool stationary_on_ws =
            !zero_motion &&""",
  """        const bool stationary_on_ws =
            !zero_motion && (e4_off_mask() & 8) == 0 &&"""),
 ("""namespace hven::solvers {""",
  """namespace hven::solvers {

// E4 DIAGNOSTIC SCRATCH ONLY -- see .scratch/e4/tools/patch_diag.py.
static int e4_off_mask() {
    static const int m = [] {
        const char *v = std::getenv("E4_OFF");
        return v != nullptr ? std::atoi(v) : 0;
    }();
    return m;
}"""),
]
mode = sys.argv[1]
s = open(P).read()
for a, b in PAIRS:
    src, dst = (a, b) if mode == 'apply' else (b, a)
    assert s.count(src) == 1, (mode, src[:60])
    s = s.replace(src, dst)
open(P, 'w').write(s)
print(f"{mode} ok")
