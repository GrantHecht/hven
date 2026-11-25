#!/usr/bin/env bash
# I19-1 mutation must-kills (M1-M7 at r1; M8-M13 added at r2). Each mutant is a one-line edit to src/sqp/sqp_solver.cpp, built into the
# same Release tree, run against IterationTelemetry.* and CallerStepLimit*, then reverted from the saved copy.
# Run as: box-lane -- bash mutate.sh <repo-root> <release-build-dir> <out-dir>   (ONLY=<name> runs one mutant)
set -o pipefail
WT=${1:?repo root}; B=${2:?build dir}; OUT=${3:?out dir}
F=$WT/src/sqp/sqp_solver.cpp
mkdir -p "$OUT"
cp "$F" "$OUT/sqp_solver.cpp.orig"
run() { # name, sed-expr
  local name=$1 expr=$2
  if [ -n "${ONLY:-}" ] && [ "$name" != "$ONLY" ]; then return; fi
  cp "$OUT/sqp_solver.cpp.orig" "$F"
  sed -i "$expr" "$F"
  if cmp -s "$F" "$OUT/sqp_solver.cpp.orig"; then echo "$name: MUTATION DID NOT APPLY"; return; fi
  diff "$OUT/sqp_solver.cpp.orig" "$F" > "$OUT/$name.diff"
  ( cd "$B" && ninja -j"${BOX_LANE_JOBS:-4}" hven_sqp_tests > "$OUT/$name.build.log" 2>&1 ) || { echo "$name: BUILD FAILED"; return; }
  "$B/tests/sqp/hven_sqp_tests" --gtest_filter='IterationTelemetry.*:CallerStepLimit*' > "$OUT/$name.test.log" 2>&1
  local rc=$?
  echo "$name: test rc=$rc ($(grep -c '^\[  FAILED  \]' "$OUT/$name.test.log") failure lines)"
  grep '^\[  FAILED  \]' "$OUT/$name.test.log" | sort -u | sed 's/^/    /'
}
run M1_tr_binding_exports_merged   's/row.tr_binding = r.radius_binding;/row.tr_binding = r.region_limited;/'
run M2_cap_binding_never_set       's/row.cap_binding = r.cap_binding;/row.cap_binding = false;/'
run M3_growth_reads_radius_only    's/mj.row.region_limited, std::isfinite(st.delta), mj.ctx.pred_df,/mj.row.radius_binding, std::isfinite(st.delta), mj.ctx.pred_df,/'
run M4_total_is_cumulative         's/exported.qp_factorizations_total = st.out.counters.factorizations - st.row_work_base_fact;/exported.qp_factorizations_total = st.out.counters.factorizations;/'
run M5_base_never_advances         's/^        st.row_work_base_fact = st.out.counters.factorizations;$/        (void)0;/'
run M6_ladder_total_is_last_rung   's/report.total_factorizations = ladder_fact_total;/report.total_factorizations = qs_e.counters.factorizations;/'
run M7_restoration_work_not_excluded 's/^        st.row_work_base_fact += phase.counters.factorizations;.*$/        (void)0;/'
run M8_pred_df_not_in_caller_units   's|mj.row.pred_df = sc.active ? mj.ctx.pred_df / sc.obj : mj.ctx.pred_df;|mj.row.pred_df = mj.ctx.pred_df;|'
run M9_f_trial_not_in_caller_units   's|mj.row.f_trial = sc.active ? mj.ev_trial.f / sc.obj : mj.ev_trial.f;|mj.row.f_trial = mj.ev_trial.f;|'
run M10_eq_row_factor_dropped        's|h += std::abs(sc.active ? ce(i) / sc.eq_rows(i) : ce(i));|h += std::abs(ce(i));|'
run M11_ineq_row_factor_dropped      's|const double v = sc.active ? ci(j) / sc.ineq_rows(j) : ci(j);|const double v = ci(j);|'
# M12, M14, M15 are the m6 reads: expected to SURVIVE (see RESULTS r2: no fixture reaches a cap pin under the
# no-motion tolerance, and ctx.tr_active has no reader), so a survivor here is the recorded outcome, not a failure.
run M12_zero_step_reads_radius_only  's|!mj.row.region_limited \&\&|!mj.row.radius_binding \&\&|'
run M14_main_judge_reads_radius_only 's|mj.ctx.tr_active = mj.row.region_limited;|mj.ctx.tr_active = mj.row.radius_binding;|'
run M15_soc_judge_reads_radius_only  's|soc_ctx.tr_active = mj.row.region_limited;|soc_ctx.tr_active = mj.row.radius_binding;|'
run M13_minor_base_not_excluded      's/^        st.row_work_base_minor += phase.counters.qp_minor_iters;.*$/        (void)0;/'
cp "$OUT/sqp_solver.cpp.orig" "$F"
( cd "$B" && ninja -j"${BOX_LANE_JOBS:-4}" hven_sqp_tests > "$OUT/restore.build.log" 2>&1 )
git -C "$WT" diff --quiet -- src/sqp/sqp_solver.cpp && echo "restored: tree clean of mutations" || echo "restored: source differs from HEAD (expected until committed)"
