// PF-1b scratch replay (NOT FOR MERGE): replay one D4QP dump on a fresh QpEngine with the walk observer
// armed, print counters, an action census, per-minor CSV, the recomputed KKT residuals of the returned
// point, and (--exact) the unregularized EQP on the returned working set solved by SparseLU.
// usage: pf1b_replay DUMP [--maxiter N] [--csv FILE] [--mu M] [--delta D] [--indep 0|1] [--exact] [--sv]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

#include <Eigen/SparseLU>
#include <Eigen/SVD>
#include <Eigen/SparseCholesky>

#include "support/qp_dump.h"
#include <hven/detail/qp/qp_walk_trace.h>

using namespace hven;
namespace T = hven::detail::testing;
using SpMat = Eigen::SparseMatrix<double>;

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage\n");
        return 2;
    }
    const std::string path = argv[1];
    long maxiter = -1;
    int indep = -1;
    double mu = -1.0, delta = -1.0;
    bool exact = false, sv = false, hpd = false;
    std::string csv;
    for (int a = 2; a < argc; ++a) {
        std::string s = argv[a];
        if (s == "--maxiter") maxiter = std::atol(argv[++a]);
        else if (s == "--csv") csv = argv[++a];
        else if (s == "--indep") indep = std::atoi(argv[++a]);
        else if (s == "--mu") mu = std::atof(argv[++a]);
        else if (s == "--delta") delta = std::atof(argv[++a]);
        else if (s == "--exact") exact = true;
        else if (s == "--sv") sv = true;
        else if (s == "--hpd") hpd = true;
    }
    test_support::QpDump d = test_support::load_qp_dump(path);
    QpOptions o = d.opts;
    if (maxiter > 0) o.max_iterations = maxiter;
    if (indep >= 0) o.independent_working_set = indep != 0;
    if (mu >= 0.0) o.dual_regularization = mu;
    if (delta >= 0.0) o.primal_regularization = delta;
    const Index n = d.qp.n(), me = d.qp.me(), mi = d.qp.mi();
    if (hpd) {
        SpMat H = d.qp.H.selfadjointView<Eigen::Upper>();
        Eigen::SimplicialLDLT<SpMat> ldlt(H);
        const Vec D = ldlt.vectorD();
        std::printf("PF1H info=%d Dmin=%.6e Dmax=%.6e nneg=%ld\n", (int)ldlt.info(), D.minCoeff(), D.maxCoeff(),
                    (long)(D.array() <= 0.0).count());
    }
    T::QpWalkObserver::reset();
    T::QpWalkObserver::active = true;
    T::QpWalkObserver::max_records = 4000000;
    const auto t0 = std::chrono::steady_clock::now();
    const detail::QpSolution s = test_support::solve_qp_dump(d, o);
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const auto &c = s.counters;
    long pins = 0, trs = 0;
    for (auto b : s.bound_state)
        if (b != BoundState::kFree) ++pins;
    for (bool t : s.tr_active)
        if (t) ++trs;
    std::printf("PF1R status=%d minors=%ld facts=%ld rungs=%ld suspect=%ld resid_ref=%ld drift_ref=%ld polish=%ld "
                "stag_drops=%ld stag_stops=%ld shift_adds=%ld wall=%.3f q=%.15e pins=%ld tr=%ld active=%ld mu=%.3g\n",
                (int)s.status, (long)c.minor_iters, (long)c.factorizations, (long)c.dual_regularization_decreases,
                (long)c.suspect_escalations, (long)c.eqp_residual_refusals, (long)c.eqp_row_drift_refusals,
                (long)c.eqp_polish_steps, (long)c.stagnation_drops, (long)c.stagnation_stops, (long)c.shift_adds,
                wall, test_support::qp_dump_objective(d, s.x), pins, trs,
                (long)std::count(s.ineq_active.begin(), s.ineq_active.end(), true), o.dual_regularization);
    std::printf("PF1D %s\n", d.result.c_str());
    const auto &R = T::QpWalkObserver::records;
    std::map<int, long> act_n;
    long verd[4] = {0, 0, 0, 0}, pert_tot = 0, pert_passes = 0;
    for (const auto &r : R) {
        ++act_n[static_cast<int>(r.action)];
        if (r.inertia_verdict >= 0 && r.inertia_verdict <= 2) ++verd[r.inertia_verdict];
        else ++verd[3];
        if (r.inertia_perturbed > 0) {
            pert_tot += r.inertia_perturbed;
            ++pert_passes;
        }
    }
    std::printf("PF1A records=%zu truncated=%d", R.size(), (int)T::QpWalkObserver::truncated);
    for (auto &[k, v] : act_n) std::printf(" a%d=%ld", k, v);
    std::printf(" | verdict ok=%ld suspect=%ld wrong=%ld none=%ld perturbed_passes=%ld perturbed_total=%ld\n", verd[0],
                verd[1], verd[2], verd[3], pert_passes, pert_tot);
    if (s.x.size() == n && s.lambda_i.size() == mi) {
        const test_support::QpDumpKkt k = test_support::qp_dump_kkt(d, s);
        std::printf("PF1K eq=%.3e ineq=%.3e box=%.3e stat=%.3e sign=%.3e compl=%.3e scale=%.3e\n", k.eq, k.ineq,
                    k.box, k.stationarity, k.sign, k.complementarity, k.scale);
    }
    if (exact && s.x.size() == n) {
        // free columns: real-bound free and not TR-held
        std::vector<Index> fcol, colmap(n, -1);
        for (Index i = 0; i < n; ++i) {
            const bool held = s.bound_state[i] != BoundState::kFree ||
                              (static_cast<std::size_t>(i) < s.tr_active.size() && s.tr_active[i]);
            if (!held) {
                colmap[i] = (Index)fcol.size();
                fcol.push_back(i);
            }
        }
        std::vector<Index> arows;
        for (Index j = 0; j < mi; ++j)
            if (s.ineq_active[j]) arows.push_back(j);
        const Index nf = (Index)fcol.size(), mw = me + (Index)arows.size();
        // A_f (mw x nf), rhs pieces
        std::vector<Eigen::Triplet<double>> ta;
        Vec b(mw);
        Vec xp = s.x;
        for (Index i = 0; i < nf; ++i) xp(fcol[i]) = 0.0;
        const Vec Aex = me > 0 ? Vec(d.qp.Ae * xp) : Vec();
        const Vec Aix = mi > 0 ? Vec(d.qp.Ai * xp) : Vec();
        for (Index r = 0; r < me; ++r) {
            for (SpMatRM::InnerIterator it(d.qp.Ae, r); it; ++it)
                if (colmap[it.col()] >= 0) ta.emplace_back(r, colmap[it.col()], it.value());
            b(r) = d.qp.be(r) - Aex(r);
        }
        for (Index k = 0; k < (Index)arows.size(); ++k) {
            const Index r = arows[k];
            for (SpMatRM::InnerIterator it(d.qp.Ai, r); it; ++it)
                if (colmap[it.col()] >= 0) ta.emplace_back(me + k, colmap[it.col()], it.value());
            b(me + k) = d.qp.bi(r) - Aix(r);
        }
        SpMat Af(mw, nf);
        Af.setFromTriplets(ta.begin(), ta.end());
        SpMat Hfull = d.qp.H.selfadjointView<Eigen::Upper>();
        const Vec Hxp = Hfull * xp;
        std::vector<Eigen::Triplet<double>> tk;
        for (Index cc = 0; cc < Hfull.outerSize(); ++cc)
            for (SpMat::InnerIterator it(Hfull, cc); it; ++it)
                if (colmap[it.row()] >= 0 && colmap[it.col()] >= 0)
                    tk.emplace_back(colmap[it.row()], colmap[it.col()], it.value());
        for (Index cc = 0; cc < Af.outerSize(); ++cc)
            for (SpMat::InnerIterator it(Af, cc); it; ++it) {
                tk.emplace_back(nf + it.row(), it.col(), it.value());
                tk.emplace_back(it.col(), nf + it.row(), it.value());
            }
        SpMat K(nf + mw, nf + mw);
        K.setFromTriplets(tk.begin(), tk.end());
        Vec rhs(nf + mw);
        for (Index i = 0; i < nf; ++i) rhs(i) = -(d.qp.g(fcol[i]) + Hxp(fcol[i]));
        rhs.tail(mw) = b;
        Eigen::SparseLU<SpMat> lu;
        lu.compute(K);
        if (lu.info() != Eigen::Success) {
            std::printf("PF1E lu_fail\n");
        } else {
            Vec sol = lu.solve(rhs);
            for (int it = 0; it < 3; ++it) sol += lu.solve(Vec(rhs - K * sol));
            const double res = (K * sol - rhs).lpNorm<Eigen::Infinity>() / std::max(1.0, rhs.lpNorm<Eigen::Infinity>());
            Vec xe = s.x;
            for (Index i = 0; i < nf; ++i) xe(fcol[i]) = sol(i);
            double dl = 0.0, lmax = 0.0;
            long negrows = 0;
            for (Index r = 0; r < me; ++r) {
                dl = std::max(dl, std::abs(sol(nf + r) - s.lambda_e(r)));
                lmax = std::max(lmax, std::abs(s.lambda_e(r)));
            }
            for (Index k = 0; k < (Index)arows.size(); ++k) {
                const double le = sol(nf + me + k);
                dl = std::max(dl, std::abs(le - s.lambda_i(arows[k])));
                lmax = std::max(lmax, std::abs(s.lambda_i(arows[k])));
                if (le < 0.0) ++negrows;
            }
            // inactive-row feasibility of the exact face point
            double inact = 0.0;
            if (mi > 0) {
                const Vec sl = d.qp.bi - d.qp.Ai * xe;
                for (Index j = 0; j < mi; ++j)
                    if (!s.ineq_active[j]) inact = std::max(inact, -sl(j));
            }
            std::printf("PF1E nf=%ld mw=%ld kkt_rel_res=%.3e |x_exact-x|inf=%.3e |lam_exact-lam|inf=%.3e "
                        "|lam|inf=%.3e exact_neg_rows=%ld exact_inactive_viol=%.3e q_exact=%.15e\n",
                        (long)nf, (long)mw, res, (xe - s.x).lpNorm<Eigen::Infinity>(), dl, lmax, negrows, inact,
                        test_support::qp_dump_objective(d, xe));
        }
        if (sv) {
            Eigen::MatrixXd Ad = Eigen::MatrixXd(Af);
            Eigen::BDCSVD<Eigen::MatrixXd> svd(Ad);
            const Vec sigma = svd.singularValues();
            const Index ns = sigma.size();
            std::printf("PF1S rows=%ld cols=%ld sigma_max=%.6e", (long)Ad.rows(), (long)Ad.cols(), sigma(0));
            for (Index k = std::max<Index>(0, ns - 6); k < ns; ++k) std::printf(" s%ld=%.6e", (long)k, sigma(k));
            std::printf("\n");
        }
    }
    if (!csv.empty()) {
        FILE *f = std::fopen(csv.c_str(), "w");
        std::fprintf(f, "minor,action,block_kind,block,alpha,p_inf,degen,ws_in,ws_out,obj_in,pf,verdict,perturbed,"
                        "delta,mu,post_drop,released,shift_adds,decision_mult,best_mult,eqp_row_res,eqp_ratio,"
                        "eqp_iters,eqp_refused,stat_free,refine_steps,border_dim,path\n");
        for (const auto &r : R) {
            std::fprintf(f,
                         "%ld,%d,%d,%ld,%.6e,%.6e,%d,%ld,%ld,%.15e,%ld,%d,%ld,%.3e,%.3e,%d,%ld,%ld,%.6e,%.6e,%.6e,%.6e,"
                         "%ld,%ld,%.6e,%ld,%ld,%d\n",
                         (long)r.minor, (int)r.action, (int)r.block, (long)r.block_index, r.alpha, r.p_inf,
                         (int)r.degenerate, (long)r.ws_size_in, (long)r.ws_size_out, r.obj_in,
                         (long)r.pass_factorizations, (int)r.inertia_verdict, (long)r.inertia_perturbed,
                         r.primal_delta, r.dual_mu, (int)r.post_drop, (long)r.released_index, (long)r.shift_adds,
                         r.decision_multiplier, r.best_multiplier, r.eqp_row_residual, r.eqp_residual_ratio,
                         (long)r.eqp_residual_iters, (long)r.eqp_residual_refused, r.stat_free_inf,
                         (long)r.eqp_call_refine_steps, (long)r.border_dim, (int)r.eqp_path);
        }
        std::fclose(f);
    }
    return 0;
}
