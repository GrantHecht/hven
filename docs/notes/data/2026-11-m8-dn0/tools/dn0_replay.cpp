// M8 DN-0 dense replay: each dumped KKT matrix factored and solved both ways, in isolation.
//
//   sparse arm: hven::linear::SymmetricFactor, construct + analyze + factorize + solve
//               (destruction excluded), Options rebuilt from the captured iparm;
//   dense arm:  hven::linear::DenseSymmetricFactor, construct + try_factorize + solve on the
//               densified upper triangle (densification counted separately, not in the arm).
//
// Instructions are user-space `instructions` from perf_event_open around each arm, three
// repetitions with the arm order alternated (S,D / D,S / S,D), per-arm median. One untimed
// warm-up of both arms on the process's first matrix. Built against the installed hven
// (find_package), so it is a consumer of the public surface only.
//
// DN0_VERIFY build: linked with -Wl,--wrap=pardiso; runs each sparse arm once and checks that
// the input iparm of the replay's phase-22 call equals the captured one on the input fields.
//
// Usage: dn0_replay OUT.csv MAT_FILE...
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <hven/linear/dense_symmetric_factor.h>
#include <hven/linear/symmetric_factor.h>

namespace {

struct Dump {
    std::string path;
    int n = 0, nnz = 0, phase = 0, mtype = 0, error = 0, iparm34 = 0;
    int neg = -1, pos = -1, perturbed = -1;
    std::array<int32_t, 64> iparm{};
    std::vector<int32_t> ia, ja;
    std::vector<double> a;
};

Dump load(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot open " + path);
    }
    char magic[8];
    f.read(magic, 8);
    if (std::memcmp(magic, "DN0MAT1", 8) != 0) {
        throw std::runtime_error("bad magic in " + path);
    }
    int32_t hdr[9];
    f.read(reinterpret_cast<char *>(hdr), sizeof(hdr));
    Dump d;
    d.path = path;
    d.n = hdr[0];
    d.nnz = hdr[1];
    d.phase = hdr[2];
    d.mtype = hdr[3];
    d.error = hdr[4];
    d.iparm34 = hdr[5];
    d.neg = hdr[6];
    d.pos = hdr[7];
    d.perturbed = hdr[8];
    f.read(reinterpret_cast<char *>(d.iparm.data()), 64 * sizeof(int32_t));
    d.ia.resize(static_cast<std::size_t>(d.n) + 1);
    d.ja.resize(static_cast<std::size_t>(d.nnz));
    d.a.resize(static_cast<std::size_t>(d.nnz));
    f.read(reinterpret_cast<char *>(d.ia.data()), d.ia.size() * sizeof(int32_t));
    f.read(reinterpret_cast<char *>(d.ja.data()), d.ja.size() * sizeof(int32_t));
    f.read(reinterpret_cast<char *>(d.a.data()), d.a.size() * sizeof(double));
    if (!f) {
        throw std::runtime_error("short read in " + path);
    }
    return d;
}

hven::SpMatRM to_csr(const Dump &d) {
    const int base = d.iparm34 ? 0 : 1;
    hven::SpMatRM A(d.n, d.n);
    A.reserve(d.nnz);
    A.makeCompressed();
    A.resizeNonZeros(d.nnz);
    for (int i = 0; i <= d.n; ++i) {
        A.outerIndexPtr()[i] = d.ia[static_cast<std::size_t>(i)] - base;
    }
    for (int k = 0; k < d.nnz; ++k) {
        A.innerIndexPtr()[k] = d.ja[static_cast<std::size_t>(k)] - base;
        A.valuePtr()[k] = d.a[static_cast<std::size_t>(k)];
    }
    return A;
}

using Options = hven::linear::SymmetricFactor::Options;

// Rebuild the engine's SymmetricFactor::Options from the iparm the call was made with
// (the inverse of symmetric_factor_mkl.cpp's config_from / pardiso_session.cpp's writes).
Options options_from(const std::array<int32_t, 64> &ip) {
    Options o;
    o.threads = 0;
    o.pivot_perturbation_exponent = ip[9];
    o.max_refinement_steps = ip[7];
    switch (ip[1]) {
    case 0:
        o.ordering = Options::Ordering::kMinimumDegree;
        break;
    case 2:
        o.ordering = Options::Ordering::kNestedDissection;
        break;
    case 3:
        o.ordering = Options::Ordering::kParallelNestedDissection;
        break;
    default:
        throw std::runtime_error("unmapped iparm[1] " + std::to_string(ip[1]));
    }
    o.weighted_matching = ip[12] != 0;
    o.matrix_scaling = ip[10] != 0;
    switch (ip[20]) {
    case 0:
        o.pivot_strategy = Options::PivotStrategy::kOneByOne;
        break;
    case 1:
        o.pivot_strategy = Options::PivotStrategy::kTwoByTwo;
        break;
    case 2:
        o.pivot_strategy = Options::PivotStrategy::kOneByOneNoAutoRefine;
        break;
    case 3:
        o.pivot_strategy = Options::PivotStrategy::kTwoByTwoNoAutoRefine;
        break;
    default:
        throw std::runtime_error("unmapped iparm[20] " + std::to_string(ip[20]));
    }
    switch (ip[23]) {
    case 0:
        o.factorization_algorithm = Options::FactorizationAlgorithm::kClassic;
        break;
    case 1:
        o.factorization_algorithm = Options::FactorizationAlgorithm::kTwoLevel;
        break;
    default:
        throw std::runtime_error("unmapped iparm[23] " + std::to_string(ip[23]));
    }
    switch (ip[24]) {
    case 0:
        o.solve_parallelism = Options::SolveParallelism::kAdaptivePartitioning;
        break;
    case 1:
        o.solve_parallelism = Options::SolveParallelism::kSequential;
        break;
    case 2:
        o.solve_parallelism = Options::SolveParallelism::kMatrixPartitionParallel;
        break;
    default:
        throw std::runtime_error("unmapped iparm[24] " + std::to_string(ip[24]));
    }
    o.conditional_reproducibility_threads = ip[33] > 0 ? ip[33] : 0; // pardisoinit leaves -1 (off)
    o.collect_factor_mflops = ip[18] != 0;
    return o;
}

std::string options_signature(const std::array<int32_t, 64> &ip) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "pp%d_rf%d_ord%d_wm%d_sc%d_piv%d_alg%d_sol%d_cnr%d_mf%d", ip[9],
                  ip[7], ip[1], ip[12], ip[10], ip[20], ip[23], ip[24], ip[33],
                  ip[18] != 0 ? 1 : 0);
    return buf;
}

class InstrCounter {
  public:
    InstrCounter() {
        perf_event_attr attr{};
        attr.type = PERF_TYPE_HARDWARE;
        attr.size = sizeof(attr);
        attr.config = PERF_COUNT_HW_INSTRUCTIONS;
        attr.disabled = 1;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;
        fd_ = static_cast<int>(syscall(SYS_perf_event_open, &attr, 0, -1, -1, 0));
        if (fd_ < 0) {
            throw std::runtime_error("perf_event_open(instructions:u) failed");
        }
    }
    ~InstrCounter() { close(fd_); }
    void start() {
        ioctl(fd_, PERF_EVENT_IOC_RESET, 0);
        ioctl(fd_, PERF_EVENT_IOC_ENABLE, 0);
    }
    long long read_now() const {
        long long v = 0;
        if (::read(fd_, &v, sizeof(v)) != static_cast<ssize_t>(sizeof(v))) {
            throw std::runtime_error("perf counter read failed");
        }
        return v;
    }
    long long stop() {
        ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0);
        return read_now();
    }

  private:
    int fd_ = -1;
};

// Infinity-norm residual of the FULL symmetric matrix (upper CSR mirrored).
double rel_residual(const hven::SpMatRM &A, const hven::Vec &x, const hven::Vec &b) {
    const int n = static_cast<int>(A.rows());
    hven::Vec r = -b;
    hven::Vec rowabs = hven::Vec::Zero(n);
    for (int i = 0; i < n; ++i) {
        for (hven::SpMatRM::InnerIterator it(A, i); it; ++it) {
            const int j = static_cast<int>(it.col());
            r[i] += it.value() * x[j];
            rowabs[i] += std::abs(it.value());
            if (j != i) {
                r[j] += it.value() * x[i];
                rowabs[j] += std::abs(it.value());
            }
        }
    }
    const double den =
        rowabs.maxCoeff() * x.lpNorm<Eigen::Infinity>() + b.lpNorm<Eigen::Infinity>();
    return den > 0 ? r.lpNorm<Eigen::Infinity>() / den : r.lpNorm<Eigen::Infinity>();
}

struct SparseRun {
    long long analyze = 0, factorize = 0, solve = 0, total = 0;
    int status = -1; // 0 ok, 1 backend error, 2 threw
    int neg = -1, pos = -1, zero = -1, perturbed = -1, inertia_state = -1;
    hven::Vec x;
};

struct DenseRun {
    long long densify = 0, factorize = 0, solve = 0, total = 0;
    int status = -1; // 0 ok, 1 exactly singular, 2 threw
    int neg = -1;
    hven::Vec x;
};

SparseRun run_sparse(InstrCounter &c, const hven::SpMatRM &A, const Options &opts,
                     const hven::Vec &b) {
    SparseRun r;
    r.x = hven::Vec::Zero(A.rows());
    std::optional<hven::linear::SymmetricFactor> f;
    try {
        c.start();
        f.emplace(opts);
        f->analyze(A);
        const long long t1 = c.read_now();
        const hven::linear::FactorizeOutcome fo = f->factorize(A);
        const long long t2 = c.read_now();
        long long t3 = t2;
        if (fo.status == hven::linear::FactorizeOutcome::Status::kOk) {
            f->solve(b, r.x);
            t3 = c.stop();
        } else {
            c.stop();
        }
        r.analyze = t1;
        r.factorize = t2 - t1;
        r.solve = t3 - t2;
        r.total = t3;
        r.status = fo.status == hven::linear::FactorizeOutcome::Status::kOk ? 0 : 1;
        r.inertia_state = static_cast<int>(fo.inertia.state);
        if (fo.inertia.state == hven::linear::InertiaEvidence::State::kObserved) {
            r.neg = static_cast<int>(fo.inertia.negative);
            r.pos = static_cast<int>(fo.inertia.positive);
            r.zero = static_cast<int>(fo.inertia.zero);
        }
        if (fo.inertia.perturbed_pivots.has_value()) {
            r.perturbed = static_cast<int>(*fo.inertia.perturbed_pivots);
        }
    } catch (const std::exception &e) {
        c.stop();
        r.status = 2;
        std::fprintf(stderr, "sparse arm threw: %s\n", e.what());
    }
    f.reset();
    return r;
}

DenseRun run_dense(InstrCounter &c, const hven::SpMatRM &A, const hven::Vec &b) {
    DenseRun r;
    const int n = static_cast<int>(A.rows());
    r.x = hven::Vec::Zero(n);
    c.start();
    hven::Mat M = hven::Mat::Zero(n, n);
    for (int i = 0; i < n; ++i) {
        for (hven::SpMatRM::InnerIterator it(A, i); it; ++it) {
            M(i, it.col()) = it.value();
        }
    }
    r.densify = c.stop();
    std::optional<hven::linear::DenseSymmetricFactor> f;
    try {
        c.start();
        f.emplace();
        const auto out = f->try_factorize(M, hven::linear::Triangle::kUpper);
        const long long t1 = c.read_now();
        long long t2 = t1;
        if (out == hven::linear::DenseFactorizeOutcome::kOk) {
            f->solve(b, r.x);
            t2 = c.stop();
        } else {
            c.stop();
        }
        r.factorize = t1;
        r.solve = t2 - t1;
        r.total = t2;
        r.status = out == hven::linear::DenseFactorizeOutcome::kOk ? 0 : 1;
        if (r.status == 0) {
            const auto ev = f->block_evidence();
            if (ev.has_value()) {
                r.neg = static_cast<int>(ev->neg_eigs);
            }
        }
    } catch (const std::exception &e) {
        c.stop();
        r.status = 2;
        std::fprintf(stderr, "dense arm threw: %s\n", e.what());
    }
    f.reset();
    return r;
}

long long med3(long long a, long long b, long long c) {
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

std::string base_name(const std::string &p) {
    const auto k = p.find_last_of('/');
    return k == std::string::npos ? p : p.substr(k + 1);
}

} // namespace

#ifdef DN0_VERIFY
// The input fields of iparm (MKL reference: inputs, or in/out set by this session at analyze).
static const int kInputFields[] = {0,  1,  3,  4,  5,  7,  9,  10, 11, 12, 20,
                                   23, 24, 26, 27, 30, 33, 34, 35, 36, 55, 59};
static const int32_t *g_expected = nullptr;
static int g_match = -1;
static std::string g_mismatch;
extern "C" void __real_pardiso(void *, const int *, const int *, const int *, const int *,
                               const int *, const void *, const int *, const int *, int *,
                               const int *, int *, const int *, void *, void *, int *);
extern "C" void __wrap_pardiso(void *pt, const int *maxfct, const int *mnum, const int *mtype,
                               const int *phase, const int *n, const void *a, const int *ia,
                               const int *ja, int *perm, const int *nrhs, int *iparm,
                               const int *msglvl, void *b, void *x, int *error) {
    if (g_expected != nullptr && *phase == 22) {
        g_match = 1;
        for (int k : kInputFields) {
            if (iparm[k] != g_expected[k]) {
                g_match = 0;
                g_mismatch += "[" + std::to_string(k) + "]" + std::to_string(iparm[k]) +
                              "!=" + std::to_string(g_expected[k]) + ";";
            }
        }
    }
    __real_pardiso(pt, maxfct, mnum, mtype, phase, n, a, ia, ja, perm, nrhs, iparm, msglvl, b, x,
                   error);
}
#endif

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: dn0_replay OUT.csv MAT_FILE...\n");
        return 2;
    }
    std::FILE *out = std::fopen(argv[1], "w");
    if (out == nullptr) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    InstrCounter counter;
#ifdef DN0_VERIFY
    std::fprintf(out, "matrix,n,options,iparm_match,mismatch\n");
    for (int k = 2; k < argc; ++k) {
        const Dump d = load(argv[k]);
        const hven::SpMatRM A = to_csr(d);
        const hven::Vec b = hven::Vec::Ones(d.n);
        g_expected = d.iparm.data();
        g_match = -1;
        g_mismatch.clear();
        (void)run_sparse(counter, A, options_from(d.iparm), b);
        g_expected = nullptr;
        std::fprintf(out, "%s,%d,%s,%d,%s\n", base_name(d.path).c_str(), d.n,
                     options_signature(d.iparm).c_str(), g_match, g_mismatch.c_str());
    }
#else
    std::fprintf(out, "matrix,n,nnz_upper,density_full,options,engine_error,engine_pos,engine_neg,"
                      "engine_perturbed,sparse_status,sparse_inertia_state,sparse_pos,sparse_neg,"
                      "sparse_zero,sparse_perturbed,dense_status,dense_neg,neg_agree,"
                      "sparse_analyze,sparse_factorize,sparse_solve,sparse_total,"
                      "dense_densify,dense_factorize,dense_solve,dense_total,"
                      "sparse_total_r1,sparse_total_r2,sparse_total_r3,"
                      "dense_total_r1,dense_total_r2,dense_total_r3,"
                      "sparse_residual,dense_residual\n");
    bool warmed = false;
    for (int k = 2; k < argc; ++k) {
        const Dump d = load(argv[k]);
        const hven::SpMatRM A = to_csr(d);
        const hven::Vec b = hven::Vec::Ones(d.n);
        const Options opts = options_from(d.iparm);
        if (!warmed) {
            (void)run_sparse(counter, A, opts, b);
            (void)run_dense(counter, A, b);
            warmed = true;
        }
        SparseRun s[3];
        DenseRun dd[3];
        s[0] = run_sparse(counter, A, opts, b);
        dd[0] = run_dense(counter, A, b);
        dd[1] = run_dense(counter, A, b);
        s[1] = run_sparse(counter, A, opts, b);
        s[2] = run_sparse(counter, A, opts, b);
        dd[2] = run_dense(counter, A, b);
        const long long full_nnz = 2LL * d.nnz - d.n; // every diagonal is stored (hven contract)
        const double density = static_cast<double>(full_nnz) / (static_cast<double>(d.n) * d.n);
        const int agree = (s[0].neg >= 0 && dd[0].neg >= 0 && s[0].neg == dd[0].neg) ? 1 : 0;
        const double sres = s[0].status == 0 ? rel_residual(A, s[0].x, b) : NAN;
        const double dres = dd[0].status == 0 ? rel_residual(A, dd[0].x, b) : NAN;
        std::fprintf(out,
                     "%s,%d,%d,%.6g,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
                     "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%lld,"
                     "%lld,%lld,%lld,%lld,%lld,%lld,%.3e,%.3e\n",
                     base_name(d.path).c_str(), d.n, d.nnz, density,
                     options_signature(d.iparm).c_str(), d.error, d.pos, d.neg, d.perturbed,
                     s[0].status, s[0].inertia_state, s[0].pos, s[0].neg, s[0].zero, s[0].perturbed,
                     dd[0].status, dd[0].neg, agree, med3(s[0].analyze, s[1].analyze, s[2].analyze),
                     med3(s[0].factorize, s[1].factorize, s[2].factorize),
                     med3(s[0].solve, s[1].solve, s[2].solve),
                     med3(s[0].total, s[1].total, s[2].total),
                     med3(dd[0].densify, dd[1].densify, dd[2].densify),
                     med3(dd[0].factorize, dd[1].factorize, dd[2].factorize),
                     med3(dd[0].solve, dd[1].solve, dd[2].solve),
                     med3(dd[0].total, dd[1].total, dd[2].total), s[0].total, s[1].total,
                     s[2].total, dd[0].total, dd[1].total, dd[2].total, sres, dres);
        std::fflush(out);
    }
#endif
    std::fclose(out);
    return 0;
}
