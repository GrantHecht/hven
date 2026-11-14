// M8 DN-1 A legs (../../DECLARATION-dn1legs.txt): DN-0's 5501 dumped KKT matrices through the DN-1
// build's SymmetricFactor, kNever and kAlways, plus two LAPACK side readings on the densified matrix.
// A consumer of the installed hven (find_package); adapted from DN-0's tools/dn0_replay.cpp (the dump
// reader, the Options rebuilt from the captured iparm, the residual).
//
//   never   SymmetricFactor(options_from(iparm)): analyze + factorize + solve(b = ones).
//   always  the same Options with dense_factorization = kAlways.
//   bk      LAPACKE_dsytrf_work (uplo 'U', column-major) on the densified upper triangle -- the call the
//           dense session makes -- read by the det/trace walk: negative count, exact singularity, and
//           min |D-block eigenvalue| / max |a_ij| (A7). Its negative count is checked against the arm's.
//   rook    LAPACKE_dsytrf_rook_work + LAPACKE_dsytrs_rook_work (A6): negative count, exact
//           singularity, max |U multiplier|, residual, user-space instructions per factorization and per
//           solve (perf_event_open, median of 3; densification outside the counter).
//
// MODE full   every column above;  MODE dense   only the always arm (A-thr's default-thread rerun).
// Usage: dn1_replay MODE OUT.csv LIST   (LIST: one "cell<TAB>path/to/mat_NNNN.bin" per line)
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <mkl_lapacke.h>

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

// DN-0's options_from, verbatim: the engine's Options rebuilt from the captured iparm.
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
    o.conditional_reproducibility_threads = ip[33] > 0 ? ip[33] : 0;
    o.collect_factor_mflops = ip[18] != 0;
    return o;
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
    long long stop() {
        ioctl(fd_, PERF_EVENT_IOC_DISABLE, 0);
        long long v = 0;
        if (::read(fd_, &v, sizeof(v)) != static_cast<ssize_t>(sizeof(v))) {
            throw std::runtime_error("perf counter read failed");
        }
        return v;
    }

  private:
    int fd_ = -1;
};

// DN-0's residual: infinity norm of the FULL symmetric matrix (upper CSR mirrored).
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

struct FactorRun {
    int threw = 0;
    int status = -1; // 0 kOk, 1 kBackendError
    int backend_code = 0;
    int arm = -1; // 0 kSparse, 1 kDense
    int state = -1, pos = -1, neg = -1, zero = -1, zero_is_derived = -1;
    long long perturbed = -1; // -1 absent
    long long f_nonzeros = -1, f_size_bytes = -1, f_dense_entries = -1;
    double f_mflops = -1.0;
    long long dense_factorize_count = 0, dense_fallback_count = 0, factorize_count = 0;
    double residual = NAN;
    hven::Vec x;
};

FactorRun run_factor(const hven::SpMatRM &A, const Options &opts, const hven::Vec &b) {
    FactorRun r;
    try {
        hven::linear::SymmetricFactor f(opts);
        f.analyze(A);
        const hven::linear::FactorizeOutcome fo = f.factorize(A);
        r.status = fo.status == hven::linear::FactorizeOutcome::Status::kOk ? 0 : 1;
        r.backend_code = fo.backend_code;
        r.arm = fo.arm == hven::linear::FactorArm::kDense ? 1 : 0;
        r.state = static_cast<int>(fo.inertia.state);
        r.pos = static_cast<int>(fo.inertia.positive);
        r.neg = static_cast<int>(fo.inertia.negative);
        r.zero = static_cast<int>(fo.inertia.zero);
        r.zero_is_derived = fo.inertia.zero_is_derived ? 1 : 0;
        if (fo.inertia.perturbed_pivots.has_value()) {
            r.perturbed = static_cast<long long>(*fo.inertia.perturbed_pivots);
        }
        if (fo.factor.factor_nonzeros.has_value()) {
            r.f_nonzeros = static_cast<long long>(*fo.factor.factor_nonzeros);
        }
        if (fo.factor.factor_mflops.has_value()) {
            r.f_mflops = static_cast<double>(*fo.factor.factor_mflops);
        }
        if (fo.factor.factor_size_bytes.has_value()) {
            r.f_size_bytes = static_cast<long long>(*fo.factor.factor_size_bytes);
        }
        if (fo.factor.dense_factor_entries.has_value()) {
            r.f_dense_entries = static_cast<long long>(*fo.factor.dense_factor_entries);
        }
        r.dense_factorize_count = static_cast<long long>(f.counters().dense_factorize_count);
        r.dense_fallback_count = static_cast<long long>(f.counters().dense_fallback_count);
        r.factorize_count = static_cast<long long>(f.counters().factorize_count);
        if (r.status == 0) {
            r.x = hven::Vec::Zero(A.rows());
            f.solve(b, r.x);
            r.residual = rel_residual(A, r.x, b);
        }
    } catch (const std::exception &e) {
        r.threw = 1;
        std::fprintf(stderr, "factor threw: %s\n", e.what());
    }
    return r;
}

// Column-major dense copy of the upper triangle (the entries dsytrf 'U' reads).
void densify_upper(const hven::SpMatRM &A, std::vector<double> &M) {
    const int n = static_cast<int>(A.rows());
    std::fill(M.begin(), M.end(), 0.0);
    for (int i = 0; i < n; ++i) {
        for (hven::SpMatRM::InnerIterator it(A, i); it; ++it) {
            const int j = static_cast<int>(it.col());
            M[static_cast<std::size_t>(j) * n + i] = it.value();
        }
    }
}

double max_abs_entry(const hven::SpMatRM &A) {
    double m = 0.0;
    for (int k = 0; k < A.nonZeros(); ++k) {
        m = std::max(m, std::abs(A.valuePtr()[k]));
    }
    return m;
}

struct Walk {
    int neg = -1;
    int singular = 0;     // 1: info > 0, a zero/non-finite 1x1, a 2x2 det of exactly 0.0 or non-finite
    double min_eig = NAN; // min |D-block eigenvalue|
    double max_mult = 0;  // max |stored U multiplier| (strict upper part outside the 2x2 off-diagonals)
};

// The det/trace walk over a completed dsytrf / dsytrf_rook factorization (uplo 'U'). A 2x2 block opens at
// k when ipiv[k] < 0 (both conventions mark both of its indices negative).
Walk walk_d(const std::vector<double> &M, int n, const std::vector<lapack_int> &ipiv, int info) {
    Walk w;
    if (info > 0) {
        w.singular = 1;
        return w;
    }
    int neg = 0;
    double min_eig = std::numeric_limits<double>::infinity();
    std::vector<char> offdiag(static_cast<std::size_t>(n), 0); // column k+1 holds D(k,k+1)
    int k = 0;
    while (k < n) {
        if (ipiv[static_cast<std::size_t>(k)] > 0) {
            const double d = M[static_cast<std::size_t>(k) * n + k];
            if (!std::isfinite(d) || d == 0.0) {
                w.singular = 1;
                return w;
            }
            neg += d < 0.0 ? 1 : 0;
            min_eig = std::min(min_eig, std::abs(d));
            k += 1;
        } else {
            if (k + 1 >= n) {
                throw std::runtime_error("malformed pivot data");
            }
            const double a = M[static_cast<std::size_t>(k) * n + k];
            const double b = M[static_cast<std::size_t>(k + 1) * n + k];
            const double c = M[static_cast<std::size_t>(k + 1) * n + (k + 1)];
            const double det = a * c - b * b;
            const double tr = a + c;
            if (!std::isfinite(det) || !std::isfinite(tr) || det == 0.0) {
                w.singular = 1;
                return w;
            }
            if (det < 0.0) {
                neg += 1;
            } else if (tr < 0.0) {
                neg += 2;
            }
            // |lambda_max| = |tr|/2 + sqrt(tr^2/4 - det); |lambda_min| = |det| / |lambda_max|.
            const double h = 0.5 * std::abs(tr);
            const double lmax = h + std::sqrt(std::max(0.0, h * h - det));
            min_eig = std::min(min_eig, std::abs(det) / lmax);
            offdiag[static_cast<std::size_t>(k + 1)] = 1;
            k += 2;
        }
    }
    double mm = 0.0;
    for (int j = 1; j < n; ++j) {
        for (int i = 0; i < j; ++i) {
            if (i == j - 1 && offdiag[static_cast<std::size_t>(j)]) {
                continue;
            }
            mm = std::max(mm, std::abs(M[static_cast<std::size_t>(j) * n + i]));
        }
    }
    w.neg = neg;
    w.min_eig = min_eig;
    w.max_mult = mm;
    return w;
}

long long med3(long long a, long long b, long long c) {
    return std::max(std::min(a, b), std::min(std::max(a, b), c));
}

std::string base_name(const std::string &p) {
    const auto k = p.find_last_of('/');
    return k == std::string::npos ? p : p.substr(k + 1);
}

std::string opt_ll(long long v) { return v < 0 ? std::string() : std::to_string(v); }

std::string fmt_g(double v) {
    if (std::isnan(v)) {
        return std::string();
    }
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.6e", v);
    return buf;
}

std::string run_cols(const FactorRun &r) {
    std::ostringstream s;
    s << r.threw << ',' << r.status << ',' << r.backend_code << ',' << r.arm << ',' << r.state << ','
      << r.pos << ',' << r.neg << ',' << r.zero << ',' << r.zero_is_derived << ','
      << opt_ll(r.perturbed) << ',' << opt_ll(r.f_nonzeros) << ','
      << (r.f_mflops < 0 ? std::string() : fmt_g(r.f_mflops)) << ',' << opt_ll(r.f_size_bytes)
      << ',' << opt_ll(r.f_dense_entries) << ',' << r.factorize_count << ','
      << r.dense_factorize_count << ',' << r.dense_fallback_count << ',' << fmt_g(r.residual);
    return s.str();
}

const char *kRunHeader = "threw,status,backend_code,arm,state,pos,neg,zero,zero_is_derived,perturbed,"
                         "factor_nonzeros,factor_mflops,factor_size_bytes,dense_factor_entries,"
                         "factorize_count,dense_factorize_count,dense_fallback_count,residual";

std::string prefixed(const std::string &prefix) {
    std::string out;
    std::string h = kRunHeader;
    std::size_t p = 0;
    while (p <= h.size()) {
        const auto q = h.find(',', p);
        const std::string tok = h.substr(p, q == std::string::npos ? std::string::npos : q - p);
        if (!out.empty()) {
            out += ',';
        }
        out += prefix + tok;
        if (q == std::string::npos) {
            break;
        }
        p = q + 1;
    }
    return out;
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 4) {
        std::fprintf(stderr, "usage: dn1_replay full|dense OUT.csv LIST\n");
        return 2;
    }
    const std::string mode = argv[1];
    if (mode != "full" && mode != "dense") {
        std::fprintf(stderr, "unknown mode %s\n", mode.c_str());
        return 2;
    }
    const bool full = mode == "full";
    std::FILE *out = std::fopen(argv[2], "w");
    if (out == nullptr) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    std::ifstream list(argv[3]);
    if (!list) {
        std::fprintf(stderr, "cannot open %s\n", argv[3]);
        return 2;
    }
    std::vector<std::pair<std::string, std::string>> items;
    for (std::string line; std::getline(list, line);) {
        const auto t = line.find('\t');
        if (t == std::string::npos) {
            continue;
        }
        items.emplace_back(line.substr(0, t), line.substr(t + 1));
    }
    std::optional<InstrCounter> counter;
    if (full) {
        counter.emplace();
        std::fprintf(out, "cell,matrix,n,nnz_upper,%s,%s,always_x_bitwise_equal_never,max_abs_a,"
                          "bk_info,bk_neg,bk_singular,bk_min_eig_rel,rook_info,rook_neg,rook_singular,"
                          "rook_max_mult,rook_residual,rook_factor_instr,rook_solve_instr\n",
                     prefixed("never_").c_str(), prefixed("always_").c_str());
    } else {
        std::fprintf(out, "cell,matrix,n,%s\n", prefixed("always_").c_str());
    }
    for (const auto &item : items) {
        const Dump d = load(item.second);
        const hven::SpMatRM A = to_csr(d);
        const hven::Vec b = hven::Vec::Ones(d.n);
        const Options never_opts = options_from(d.iparm);
        Options always_opts = never_opts;
        always_opts.dense_factorization = hven::linear::DensePolicy::kAlways;
        const FactorRun a = run_factor(A, always_opts, b);
        if (!full) {
            std::fprintf(out, "%s,%s,%d,%s\n", item.first.c_str(), base_name(d.path).c_str(), d.n,
                         run_cols(a).c_str());
            std::fflush(out);
            continue;
        }
        const FactorRun s = run_factor(A, never_opts, b);
        int xeq = -1;
        if (s.status == 0 && a.status == 0 && s.x.size() == a.x.size()) {
            xeq = std::memcmp(s.x.data(), a.x.data(), sizeof(double) * s.x.size()) == 0 ? 1 : 0;
        }
        const int n = d.n;
        const auto nn = static_cast<std::size_t>(n) * static_cast<std::size_t>(n);
        std::vector<double> M(nn);
        std::vector<lapack_int> ipiv(static_cast<std::size_t>(n));
        const double amax = max_abs_entry(A);

        // bk: the dense session's own call.
        densify_upper(A, M);
        double q = 0.0;
        LAPACKE_dsytrf_work(LAPACK_COL_MAJOR, 'U', n, M.data(), n, ipiv.data(), &q, -1);
        std::vector<double> work(std::max<std::size_t>(1, static_cast<std::size_t>(q)));
        const int bk_info =
            static_cast<int>(LAPACKE_dsytrf_work(LAPACK_COL_MAJOR, 'U', n, M.data(), n, ipiv.data(),
                                                 work.data(), static_cast<lapack_int>(work.size())));
        const Walk bk = walk_d(M, n, ipiv, bk_info);

        // rook.
        densify_upper(A, M);
        double qr = 0.0;
        LAPACKE_dsytrf_rook_work(LAPACK_COL_MAJOR, 'U', n, M.data(), n, ipiv.data(), &qr, -1);
        std::vector<double> rwork(std::max<std::size_t>(1, static_cast<std::size_t>(qr)));
        long long rf[3] = {0, 0, 0};
        long long rs[3] = {0, 0, 0};
        int rook_info = 0;
        hven::Vec xr = hven::Vec::Zero(n);
        for (int rep = 0; rep < 3; ++rep) {
            densify_upper(A, M);
            counter->start();
            rook_info = static_cast<int>(
                LAPACKE_dsytrf_rook_work(LAPACK_COL_MAJOR, 'U', n, M.data(), n, ipiv.data(),
                                         rwork.data(), static_cast<lapack_int>(rwork.size())));
            rf[rep] = counter->stop();
            if (rook_info == 0) {
                xr = b;
                counter->start();
                LAPACKE_dsytrs_rook_work(LAPACK_COL_MAJOR, 'U', n, 1, M.data(), n, ipiv.data(),
                                         xr.data(), n);
                rs[rep] = counter->stop();
            }
        }
        const Walk rook = walk_d(M, n, ipiv, rook_info);
        const double rook_res = rook_info == 0 ? rel_residual(A, xr, b) : NAN;

        std::fprintf(out, "%s,%s,%d,%d,%s,%s,%d,%s,%d,%d,%d,%s,%d,%d,%d,%s,%s,%lld,%lld\n",
                     item.first.c_str(), base_name(d.path).c_str(), n, d.nnz, run_cols(s).c_str(),
                     run_cols(a).c_str(), xeq, fmt_g(amax).c_str(), bk_info, bk.neg, bk.singular,
                     fmt_g(amax > 0 ? bk.min_eig / amax : NAN).c_str(), rook_info, rook.neg,
                     rook.singular, fmt_g(rook.singular ? NAN : rook.max_mult).c_str(),
                     fmt_g(rook_res).c_str(), med3(rf[0], rf[1], rf[2]),
                     rook_info == 0 ? med3(rs[0], rs[1], rs[2]) : -1LL);
        std::fflush(out);
    }
    std::fclose(out);
    return 0;
}
