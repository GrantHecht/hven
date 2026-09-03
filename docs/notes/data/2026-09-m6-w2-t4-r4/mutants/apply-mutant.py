import sys, shutil, subprocess
name=sys.argv[1]
# The pristine source is the measured code head a14ad96, read from git so this stays runnable.
# M8 was added in W2 T5's fix round 1 and needs THAT tree (a14ad96 predates T5's test file):
# pass its ref (8f4f062, the fix-round driver head) as the optional second argument.
ref=sys.argv[2] if len(sys.argv)>2 else 'a14ad96'
dst='src/drivers/sqp_driver.cpp'
s=subprocess.run(['git','show',ref+':src/drivers/sqp_driver.cpp'],capture_output=True,text=True,check=True).stdout
GATE="            if (cand.x != nullptr && cand.x->size() == x.size() && cand.x->allFinite()) {"
def rep(a,b):
    global s
    assert s.count(a)==1, ("missing", a[:70])
    s=s.replace(a,b)
if name=='M1_qp_mode_gate':
    rep(GATE, "            if (opts_.qp_mode == QpMode::kWalk && cand.x != nullptr &&\n                cand.x->size() == x.size() && cand.x->allFinite()) {")
elif name=='M2_no_reclassification':
    rep("""                        seam.refresh_derivatives(*cand.values_ev, *cand.x);
                        ++out.counters.evals_full;
                        --out.counters.evals_values;
""","""                        seam.refresh_derivatives(*cand.values_ev, *cand.x);
""")
elif name=='M3_fresh_full_query':
    rep("                        seam.refresh_derivatives(*cand.values_ev, *cand.x);",
        "                        *cand.values_ev = seam.eval_nlp(*cand.x, lambda_e, lambda_i);")
elif name=='M4_no_zero_step_check':
    rep("""                const bool have_p = report.p_elastic.size() == n &&
                                    report.p_elastic.lpNorm<Eigen::Infinity>() > 0.0;""",
        """                const bool have_p = report.p_elastic.size() == n;""")
elif name=='M5_no_candidate_anywhere':
    rep(GATE, "            if (false && cand.x != nullptr && cand.x->size() == x.size() &&\n                cand.x->allFinite()) {")
elif name=='M6_engine_scale_guard':
    rep("                const double h_entry = constraint_violation_l1(resto_ev);",
        "                const double h_entry = constraint_violation_l1(ev);")
    rep("""                    ++out.counters.evals_full;
                    seam.to_caller_scale(ev_cand);
                    take = ev_cand.all_finite && jacobian_values_finite(ev_cand) &&
                           constraint_violation_l1(ev_cand) < h_entry;""",
        """                    ++out.counters.evals_full;
                    take = ev_cand.all_finite && jacobian_values_finite(ev_cand) &&
                           constraint_violation_l1(ev_cand) < h_entry;
                    seam.to_caller_scale(ev_cand);""")
    rep("""                    NlpEval probe = *cand.values_ev;
                    seam.to_caller_scale(probe);
                    if (constraint_violation_l1(probe) < h_entry) {""",
        """                    NlpEval probe = *cand.values_ev;
                    if (constraint_violation_l1(probe) < h_entry) {""")
elif name=='M7_no_jacobian_screen':
    rep("take = ev_cand.all_finite && jacobian_values_finite(ev_cand) &&",
        "take = ev_cand.all_finite &&")
    rep("if (cand.values_ev->all_finite && jacobian_values_finite(*cand.values_ev)) {",
        "if (cand.values_ev->all_finite) {")
elif name=='M8_nan_h_discriminator':
    rep("                if (cand.values_ev == nullptr) {",
        "                if (cand.values_ev == nullptr ||\n"
        "                    !std::isfinite(constraint_violation_l1(*cand.values_ev))) {")
elif name=='CLEAN':
    pass
else:
    raise SystemExit("unknown mutant "+name)
open(dst,'w').write(s)
print("applied", name)
