// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).
//
// M8 AP-2 B3-a (IPARM-SURFACE evidence): dump the full 64-entry Pardiso iparm array handed
// to every pardiso() call of an HS071 interior-point solve, for the default options and for
// each non-default value of every IpmOptions field that reaches the linear layer.
//
// B3 RIDERS COPY of ../../iparm/dump_iparm.cpp: built once, -DHVEN_B3_ARM=1, against the
// installed riders-head library, linked with -Wl,--wrap=pardiso. The two configurations whose
// option no longer exists (algorithm=two_level: the enumerator is gone; backend_print=on: the
// field is gone) are dropped; skip_unperturbed=off is spelled advanced.skip_unperturbed_attempts.
// The pin (HVEN_B3_ARM=0) branch is deleted. The criterion (riders/DECLARATION.txt) is that
// this dump equals the B3 pin dump (../../iparm/iparm-pin.txt) with those two configs removed.
// The array is captured BEFORE each call (what hven configured; the backend writes output
// entries during the call). A configuration refused by std::invalid_argument -- at
// construction, set_options or solve entry -- prints one REFUSED line, without the message
// text (the field is named differently on the two arms).
//
// B8 COPY of ../../2026-11-m8-ap2-b7-riders/iparm/dump_iparm.cpp: one change, `using namespace
// hven::solvers` -> `using namespace hven` (the namespace rename); no configuration changes.
//
// M8 DN-3 r4 COPY of ../../../2026-11-m8-ap2-b8-riders/iparm/dump_iparm.cpp (I3a): silence() sets
// linear.dense_factorization = kNever; nothing else changes, so the dump compares byte for byte with iparm-b8r.txt.
//
// B8 RIDERS COPY of ../../2026-11-m8-ap2-b8/iparm/dump_iparm.cpp: the option spellings the riders
// renamed (manifest/ident_manifest.tsv types; IpmLinearOptions' members per manifest/member_manifest.tsv).
// The configuration LABELS are unchanged, so the dump compares byte for byte with the pin.

#include <cstdio>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <mkl_pardiso.h>
#include <mkl_types.h>

#include "hven/drivers/ipm_solver.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"

namespace {

struct Call {
    int phase = 0, mtype = 0, n = 0, nrhs = 0;
    MKL_INT iparm[64] = {};
};
std::vector<Call> g_calls;
bool g_recording = false;

} // namespace

extern "C" {
void __real_pardiso(_MKL_DSS_HANDLE_t pt, const MKL_INT *maxfct, const MKL_INT *mnum,
                    const MKL_INT *mtype, const MKL_INT *phase, const MKL_INT *n, const void *a,
                    const MKL_INT *ia, const MKL_INT *ja, MKL_INT *perm, const MKL_INT *nrhs,
                    MKL_INT *iparm, const MKL_INT *msglvl, void *b, void *x, MKL_INT *error);

void __wrap_pardiso(_MKL_DSS_HANDLE_t pt, const MKL_INT *maxfct, const MKL_INT *mnum,
                    const MKL_INT *mtype, const MKL_INT *phase, const MKL_INT *n, const void *a,
                    const MKL_INT *ia, const MKL_INT *ja, MKL_INT *perm, const MKL_INT *nrhs,
                    MKL_INT *iparm, const MKL_INT *msglvl, void *b, void *x, MKL_INT *error) {
    if (g_recording) {
        Call c;
        c.phase = static_cast<int>(*phase);
        c.mtype = static_cast<int>(*mtype);
        c.n = static_cast<int>(*n);
        c.nrhs = static_cast<int>(*nrhs);
        for (int i = 0; i < 64; ++i) {
            c.iparm[i] = iparm[i];
        }
        g_calls.push_back(c);
    }
    __real_pardiso(pt, maxfct, mnum, mtype, phase, n, a, ia, ja, perm, nrhs, iparm, msglvl, b, x,
                   error);
}
} // extern "C"

namespace {

using namespace hven;
using hven::ConstVecRef;
using hven::IndexVecRef;
using hven::kInfinity;
using hven::VecRef;

struct Hs071 : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 4, .m = 2, .jacobian_nonzeros = 8, .hessian_nonzeros = 10};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(1.0);
        xu.setConstant(5.0);
        gl << 25.0, 40.0;
        gu << kInfinity, 40.0;
    }
    double eval_f(ConstVecRef x) const override {
        return x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g << x[3] * (2 * x[0] + x[1] + x[2]), x[0] * x[3], x[0] * x[3] + 1,
            x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(ConstVecRef x, VecRef g) const override {
        g << x[0] * x[1] * x[2] * x[3], x.squaredNorm();
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override {
        v << x[1] * x[2] * x[3], x[0] * x[2] * x[3], x[0] * x[1] * x[3], x[0] * x[1] * x[2],
            2 * x[0], 2 * x[1], 2 * x[2], 2 * x[3];
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3;
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_hessian(ConstVecRef x, double of, ConstVecRef l, VecRef v) const override {
        v << of * 2 * x[3] + l[1] * 2, of * x[3] + l[0] * x[2] * x[3], l[1] * 2,
            of * x[3] + l[0] * x[1] * x[3], l[0] * x[0] * x[3], l[1] * 2,
            of * (2 * x[0] + x[1] + x[2]) + l[0] * x[1] * x[2], of * x[0] + l[0] * x[0] * x[2],
            of * x[0] + l[0] * x[0] * x[1], l[1] * 2;
    }
};

using Edit = std::function<void(IpmOptions &)>;

#if HVEN_B3_ARM
// M8 DN-3 r4: the ONE change -- kNever set explicitly in every configuration (the flip made kAuto the
// default; HS071's KKT order is below the IPM's threshold, so at the default no pardiso() call is made).
void silence(IpmOptions &o) { o.linear.dense_factorization = hven::linear::DensePolicy::kNever; }
#else
void silence(IpmOptions &o) { o.common.print_level = 3; }
#endif

std::vector<std::pair<std::string, Edit>> configurations() {
    std::vector<std::pair<std::string, Edit>> v;
    v.emplace_back("default", [](IpmOptions &) {});
#if HVEN_B3_ARM
    v.emplace_back("ordering=minimum_degree",
                   [](IpmOptions &o) { o.linear.ordering = IpmOrdering::kMinimumDegree; });
    v.emplace_back("ordering=parallel_nested_dissection", [](IpmOptions &o) {
        o.linear.ordering = IpmOrdering::kParallelNestedDissection;
    });
    v.emplace_back("pivoting=one_by_one",
                   [](IpmOptions &o) { o.linear.pivot_strategy = IpmPivotStrategy::kOneByOne; });
    v.emplace_back("matching=off", [](IpmOptions &o) { o.linear.weighted_matching = false; });
    v.emplace_back("scaling=on", [](IpmOptions &o) { o.linear.matrix_scaling = true; });
    v.emplace_back("pivot_perturbation=13",
                   [](IpmOptions &o) { o.linear.pivot_perturbation_exponent = 13; });
    v.emplace_back("refinement_steps=2", [](IpmOptions &o) { o.linear.max_refinement_steps = 2; });
    v.emplace_back("parallel_solve=1", [](IpmOptions &o) {
        o.linear.solve_parallelism = IpmSolveParallelism::kSequential;
    });
    v.emplace_back("parallel_solve=2", [](IpmOptions &o) {
        o.linear.solve_parallelism = static_cast<IpmSolveParallelism>(2);
    });
    v.emplace_back("cnr=on", [](IpmOptions &o) { o.linear.conditional_reproducibility = true; });
    v.emplace_back("skip_unperturbed=off",
                   [](IpmOptions &o) { o.advanced.skip_unperturbed_attempts = false; });
    v.emplace_back("force_analysis=on", [](IpmOptions &o) { o.linear.force_analysis = true; });
    v.emplace_back("threads=2", [](IpmOptions &o) { o.common.threads = 2; });
    v.emplace_back("cnr=on+threads=2", [](IpmOptions &o) {
        o.linear.conditional_reproducibility = true;
        o.common.threads = 2;
    });
#endif
    return v;
}

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: dump_iparm <out-file>\n");
        return 2;
    }
    std::FILE *out = std::fopen(argv[1], "w");
    if (out == nullptr) {
        return 2;
    }
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    for (const auto &[name, edit] : configurations()) {
        std::fprintf(out, "== config %s\n", name.c_str());
        g_calls.clear();
        IpmOptions o;
        silence(o);
        edit(o);
        try {
            Problem problem(std::make_shared<Hs071>());
            IpmSolver solver(o);
            g_recording = true;
            const IpmResult r = solver.solve(problem, x0);
            g_recording = false;
            std::fprintf(out, "status=%s iterations=%lld calls=%zu\n", to_string(r.status),
                         static_cast<long long>(r.iterations), g_calls.size());
        } catch (const std::invalid_argument &) {
            g_recording = false;
            std::fprintf(out, "REFUSED (std::invalid_argument) calls=%zu\n", g_calls.size());
        }
        for (std::size_t k = 0; k < g_calls.size(); ++k) {
            const Call &c = g_calls[k];
            std::fprintf(out, "call %zu phase=%d mtype=%d n=%d nrhs=%d iparm=", k, c.phase, c.mtype,
                         c.n, c.nrhs);
            for (int i = 0; i < 64; ++i) {
                std::fprintf(out, "%s%lld", i == 0 ? "" : ",", static_cast<long long>(c.iparm[i]));
            }
            std::fprintf(out, "\n");
        }
    }
    std::fclose(out);
    return 0;
}
