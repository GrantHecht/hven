import sys, shutil
name=sys.argv[1]
src='.scratch/w2t4fix1-r4/mutants/sqp_driver.cpp.orig'
dst='src/drivers/sqp_driver.cpp'
s=open(src).read()
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
elif name=='CLEAN':
    pass
else:
    raise SystemExit("unknown mutant "+name)
open(dst,'w').write(s)
print("applied", name)
