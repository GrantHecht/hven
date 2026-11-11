// M8 DN-1d (../DECLARATION.txt D): copied from the scratch tool build-dn1d/det/det.cpp (the dev re-measure).
// DN-1d detector re-measure: the production kReport detector (SymmetricFactor, DensePolicy::kAlways,
// DenseSingularPolicy::kReport) on E0 dump files. usage: det <out.csv> < manifest (one dump path per line)
// Output per dump: path,n,status,flagged,exact,estimates
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <Eigen/SparseCore>
#include <hven/linear/symmetric_factor.h>
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *o = std::fopen(argv[1], "w");
    std::fprintf(o, "path,n,status,flagged,exact,estimates\n");
    std::string path;
    while (std::getline(std::cin, path)) {
        if (path.empty()) continue;
        FILE *f = std::fopen(path.c_str(), "rb");
        if (!f) { std::fprintf(stderr, "MISSING %s\n", path.c_str()); return 3; }
        long hdr[6]; double dh[2]; char site[16];
        if (std::fread(hdr, sizeof hdr, 1, f) != 1 || std::fread(dh, sizeof dh, 1, f) != 1 || std::fread(site, 16, 1, f) != 1) return 4;
        const long n = hdr[0];
        std::vector<double> A((size_t)n * n);
        if (std::fread(A.data(), sizeof(double), A.size(), f) != A.size()) return 5;
        std::fclose(f);
        std::vector<Eigen::Triplet<double>> t;
        for (long i = 0; i < n; ++i)
            for (long j = i; j < n; ++j) {
                const double v = A[j * n + i];
                if (i == j || v != 0.0) t.emplace_back((int)i, (int)j, v);
            }
        hven::SpMatRM M(n, n);
        M.setFromTriplets(t.begin(), t.end());
        M.makeCompressed();
        hven::linear::SymmetricFactor::Options opt;
        opt.dense_factorization = hven::linear::DensePolicy::kAlways;
        opt.dense_singular_policy = hven::linear::DenseSingularPolicy::kReport;
        opt.threads = 1;
        hven::linear::SymmetricFactor fac(opt);
        fac.analyze(M);
        const auto oc = fac.factorize(M);
        const bool exact = oc.status != hven::linear::FactorizeOutcome::Status::kOk;
        const bool flagged = oc.inertia.numerically_singular;
        std::fprintf(o, "%s,%ld,%d,%d,%d,%ld\n", path.c_str(), n, (int)oc.status, flagged ? 1 : 0, exact ? 1 : 0,
                     (long)fac.counters().dense_condition_estimate_count);
    }
    std::fclose(o);
    return 0;
}
