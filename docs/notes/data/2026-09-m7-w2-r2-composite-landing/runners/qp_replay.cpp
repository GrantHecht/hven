// E2-D4 scratch replay tool: loads a D4QP dump and re-solves it on a fresh QpEngine.
// usage: qp_replay DUMP [--e2 0|1] [--algebra border|refactorize] [--cold] [--out FILE]
#include <cmath>
#include <cstdio>
#include <map>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/qp_problem.h>

using namespace hven;
using namespace hven::solvers;
using SpRM = Eigen::SparseMatrix<double, Eigen::RowMajor>;

struct Dump {
    long n = 0, me = 0, mi = 0;
    QpOptions opts;
    int threads = 0;
    SolveOverrides ov;
    int warm = 0, has_seed = 0;
    std::string result;
    std::map<std::string, Eigen::VectorXd> vec;
    std::map<std::string, SpRM> mat;
    std::map<std::string, std::vector<long>> ivec;
};

static Dump load(const std::string &path) {
    Dump d;
    std::ifstream f(path);
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); std::exit(2); }
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream is(line);
        std::string key;
        is >> key;
        if (key == "D4QP") continue;
        if (key == "dims") { is >> d.n >> d.me >> d.mi; continue; }
        if (key == "opts") {
            std::vector<std::string> t; std::string w; while (is >> w) t.push_back(w);
            if (t.size() != 11) { std::fprintf(stderr, "bad opts line\n"); std::exit(2); }
            auto D = [&](int i) { return std::strtod(t[i].c_str(), nullptr); };
            d.opts.primal_delta = D(0); d.opts.dual_mu = D(1); d.opts.feas_tol = D(2); d.opts.opt_tol = D(3);
            d.opts.max_iter = std::stol(t[4]); d.opts.schur_cap = std::stol(t[5]); d.opts.schur_cond_max = D(6);
            d.opts.ws_algebra = std::stoi(t[7]) == 0 ? WorkingSetLinearAlgebra::kRefactorize : WorkingSetLinearAlgebra::kSchurBorder;
            d.opts.tr_radius = D(8); d.opts.independent_working_set = std::stoi(t[9]) != 0; d.threads = std::stoi(t[10]);
            continue;
        }
        if (key == "overrides") { std::string a, b, c; is >> a >> b >> c; d.ov.tr_radius = std::strtod(a.c_str(), nullptr); d.ov.primal_delta = std::strtod(b.c_str(), nullptr); d.ov.dual_mu = std::strtod(c.c_str(), nullptr); continue; }
        if (key == "warm") { std::string t; is >> d.warm >> t >> d.has_seed; continue; }
        if (key == "result") { d.result = line; continue; }
        // vectors: "name k" ; matrices: "name r c nnz"; int vectors: name starts with seed_b/seed_i/final_
        std::vector<long> nums; long v;
        while (is >> v) nums.push_back(v);
        const bool isint = key.rfind("seed_bound", 0) == 0 || key.rfind("seed_ineq", 0) == 0 || key.rfind("final_", 0) == 0;
        if (nums.size() == 1 && isint) {
            std::vector<long> iv(nums[0]);
            for (long i = 0; i < nums[0]; ++i) { std::getline(f, line); iv[i] = std::stol(line); }
            d.ivec[key] = iv;
        } else if (nums.size() == 1) {
            Eigen::VectorXd x(nums[0]);
            for (long i = 0; i < nums[0]; ++i) { std::getline(f, line); x(i) = std::strtod(line.c_str(), nullptr); }
            d.vec[key] = x;
        } else if (nums.size() == 3) {
            std::vector<Eigen::Triplet<double>> t;
            for (long k = 0; k < nums[2]; ++k) {
                std::getline(f, line);
                std::istringstream ls(line); long r, c; std::string sv; ls >> r >> c >> sv;
                t.emplace_back((int)r, (int)c, std::strtod(sv.c_str(), nullptr));
            }
            SpRM A(nums[0], nums[1]);
            A.setFromTriplets(t.begin(), t.end());
            d.mat[key] = A;
        } else {
            std::fprintf(stderr, "bad line: %s\n", line.c_str()); std::exit(2);
        }
    }
    return d;
}

int main(int argc, char **argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: qp_replay DUMP [--e2 0|1] [--algebra border|refactorize] [--cold] [--out FILE]\n"); return 2; }
    Dump d = load(argv[1]);
    std::string out_path;
    bool cold = false;
    for (int a = 2; a < argc; ++a) {
        std::string s = argv[a];
        if (s == "--e2") d.opts.independent_working_set = std::atoi(argv[++a]) != 0;
        else if (s == "--algebra") d.opts.ws_algebra = std::string(argv[++a]) == "border" ? WorkingSetLinearAlgebra::kSchurBorder : WorkingSetLinearAlgebra::kRefactorize;
        else if (s == "--cold") cold = true;
        else if (s == "--out") out_path = argv[++a];
        else if (s == "--maxiter") d.opts.max_iter = std::atol(argv[++a]);
        else { std::fprintf(stderr, "unknown arg %s\n", s.c_str()); return 2; }
    }
    QpProblem qp;
    qp.H = d.mat["H"]; qp.g = d.vec["g"]; qp.Ae = d.mat["Ae"]; qp.be = d.vec["be"];
    qp.Ai = d.mat["Ai"]; qp.bi = d.vec["bi"]; qp.lower = d.vec["lower"]; qp.upper = d.vec["upper"];
    if (qp.Ae.rows() != d.me) qp.Ae.resize(d.me, d.n);
    if (qp.Ai.rows() != d.mi) qp.Ai.resize(d.mi, d.n);
    QpSolution seed;
    const bool use_seed = d.has_seed && !cold;
    if (use_seed) {
        seed.x = d.vec["seed_x"];
        for (long b : d.ivec["seed_bound_state"]) seed.bound_state.push_back((BoundState)b);
        for (long a : d.ivec["seed_ineq_active"]) seed.ineq_active.push_back(a != 0);
    }
    QpEngine eng(d.opts, d.threads);
    QpSolution r = use_seed ? eng.solve(qp, seed, d.ov) : eng.solve(qp, d.ov);
    const QpCounters &c = r.counters;
    const double f = qp.g.dot(r.x) + 0.5 * r.x.dot(qp.H.selfadjointView<Eigen::Upper>() * r.x);
    // primal feasibility of the returned x against the REAL box and the TR window about the start
    Eigen::VectorXd x0 = use_seed ? seed.x : Eigen::VectorXd::Zero(d.n);
    for (long i = 0; i < d.n; ++i) x0(i) = std::min(std::max(x0(i), qp.lower(i)), qp.upper(i));
    const double tr = std::isfinite(d.ov.tr_radius) ? d.ov.tr_radius : d.opts.tr_radius;
    double eqv = d.me ? (qp.Ae * r.x - qp.be).lpNorm<Eigen::Infinity>() : 0.0;
    double inv = 0.0;
    if (d.mi) { Eigen::VectorXd s = qp.Ai * r.x - qp.bi; inv = std::max(0.0, s.maxCoeff()); }
    double boxv = 0.0;
    for (long i = 0; i < d.n; ++i) {
        double lo = qp.lower(i), up = qp.upper(i);
        if (std::isfinite(tr)) { lo = std::max(lo, x0(i) - tr); up = std::min(up, x0(i) + tr); }
        boxv = std::max({boxv, lo - r.x(i), r.x(i) - up});
    }
    std::printf("REPLAY status=%d minors=%ld facts=%ld f=%.12g eqv=%.3e inv=%.3e boxv=%.3e blk_term=%ld cert=%ld cert_rej=%ld zmcap=%ld zmexh=%ld retreats=%ld exch=%ld deferred=%ld undec=%ld dep_tests=%ld entry_rm=%ld recheck_rm=%ld untested=%ld k0_reused=%d\n",
                (int)r.status, (long)c.minor_iters, (long)c.factorizations, f, eqv, inv, boxv, (long)c.dep_blocker_terminations,
                (long)c.dep_infeasible_certificates, (long)c.dep_certificate_rejections, (long)c.zero_motion_cap_hits, (long)c.zero_motion_exhausted,
                (long)c.dep_border_retreats, (long)c.ws_exchanges, (long)c.dep_deferred_adds, (long)c.dep_undecidable, (long)c.dep_tests,
                (long)c.entry_dependency_removals, (long)c.dep_recheck_removals, (long)c.dep_untested_growth, (int)c.k0_reused);
    std::printf("RIDE dep_ride_blockers=%ld ride_exchanges=%ld ride_declines_fallthrough_blocked=%ld ride_sign_vetoes=%ld ws_adds=%ld ws_drops=%ld\n", (long)c.dep_ride_blockers, (long)c.ride_exchanges, (long)c.ride_declines_fallthrough_blocked, (long)c.ride_sign_vetoes, (long)c.ws_adds, (long)c.ws_drops);
    std::printf("DUMPED %s\n", d.result.c_str());
    if (!out_path.empty()) {
        std::ofstream o(out_path);
        o.precision(17);
        o << "status " << (int)r.status << "\n";
        auto wv = [&](const char *nm, const Eigen::VectorXd &v) { o << nm << " " << v.size() << "\n"; for (long i = 0; i < v.size(); ++i) o << v(i) << "\n"; };
        wv("x", r.x); wv("lambda_e", r.lambda_e); wv("lambda_i", r.lambda_i); wv("z", r.z);
        o << "bound_state " << r.bound_state.size() << "\n"; for (auto b : r.bound_state) o << (int)b << "\n";
        o << "ineq_active " << r.ineq_active.size() << "\n"; for (bool b : r.ineq_active) o << (int)b << "\n";
    }
    return 0;
}
