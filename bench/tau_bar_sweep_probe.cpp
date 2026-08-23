// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// hven_sqp_tau_bar_sweep_probe -- reproducible artifact for
// docs/notes/2026-08-01-tuning-elastic-tau.md §4.4 (Phase-5 Task 10, fix
// round 1, finding I-5).
//
// PLACEMENT (fix round 1). This lives here, as a normal CMake target linked
// against `hven::hven` and BUILT IN BOTH CONFIGURATIONS, rather than in
// `prototypes/` (that directory's own carve-out is Python/NumPy/SciPy only --
// CLAUDE.md -- and this has to drive the real `SqpDriver` engine to be a
// reproduction rather than a re-implementation) or as a loose file under
// `docs/notes/`. `hven_sqp_f7_cold`'s own banner (this directory's
// CMakeLists.txt) states the reason exactly: "BUILT in both configurations...
// an uncompiled probe rots silently." Like that probe and `hven_sqp_bench`,
// this one is deliberately NOT ctest-registered (add_test /
// gtest_discover_tests) -- it is a measurement instrument for this note, not
// a per-commit correctness assertion.
//
// WHAT THIS IS. `kFunnelTauBar` (globalization.h) is an `inline constexpr`,
// not a runtime option, so the shipped, CMake-built form of this program only
// ever reads the ONE value the library ships (100.0) -- running it via
// `cmake --build build && ./build/bench/hven_sqp_tau_bar_sweep_probe`
// reproduces this note's §4.4 baseline row exactly, and keeps doing so as
// long as the corpus or the shipped constant changes, because it is compiled
// against the real headers every time.
//
// SWEEPING kFunnelTauBar itself (what §4.4 actually needs) requires
// recompiling against a PATCHED copy of the one header that declares it, with
// the patched copy's directory placed BEFORE the real `include/` on the
// search path -- there is no way to inject a different value into an
// `inline constexpr` at runtime, and the ruling this note reaches is "do not
// change globalization.h", so the patch is external to the shipped build, not
// a CMake option on it. EXACT INVOCATION (from the repo root; matches this
// project's own compiler/backend banner -- clang++, MKL LP64,
// MKL_NUM_THREADS=1):
//
//   for TAU in 1 10 100 1e3 1e5; do
//     D=/tmp/tau_bar_patch/hven/detail/globalization/sqp; rm -rf /tmp/tau_bar_patch
//     mkdir -p "$D" && cp include/hven/detail/globalization/sqp/globalization.h "$D/"
//     F="$D/globalization.h"
//     sed -i "s/kFunnelTauBar = 100.0;/kFunnelTauBar = ${TAU};/" "$F"
//     clang++ -DFMT_HEADER_ONLY -DMKL_LP64 -m64 -O3 -DNDEBUG -std=c++20 \
//       -I /tmp/tau_bar_patch -I include -I tests/sqp \
//       -isystem dep/eigen -isystem dep/fmt/include \
//       -isystem /opt/intel/oneapi/mkl/latest/include \
//       bench/tau_bar_sweep_probe.cpp src/globalization/sqp/funnel.cpp \
//       build/libhven.a -o /tmp/tau_bar_probe \
//       -Wl,--start-group /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_lp64.a \
//       /opt/intel/oneapi/mkl/latest/lib/libmkl_intel_thread.a \
//       /opt/intel/oneapi/mkl/latest/lib/libmkl_core.a -Wl,--end-group \
//       /opt/intel/oneapi/compiler/latest/lib/libiomp5.so -lpthread -ldl -lm
//     echo "=== tau_bar = ${TAU} ==="
//     MKL_NUM_THREADS=1 LD_LIBRARY_PATH=/opt/intel/oneapi/compiler/latest/lib /tmp/tau_bar_probe
//   done
//
// The `-I /tmp/tau_bar_patch` ahead of `-I include` is what makes `#include
// <hven/detail/globalization/sqp/globalization.h>` resolve to the patched copy
// while every OTHER header (sqp_driver.h, qp_types.h, ...) still resolves to the
// real `include/hven/detail/globalization/sqp/`, unmodified -- this file does
// not, and cannot, change anything the shipped library ships; only the SWEEP
// invocation above (never `cmake --build`) touches a patched header, and only
// in a scratch directory outside the repository.
//
// `src/globalization/sqp/funnel.cpp` JOINS THE SWEEP COMMAND LINE (M3 phase-C
// T4) for the same reason the `-I` ordering is there. That carve moved
// FunnelStrategy's three virtual definitions -- including reset(), the ONLY
// reader of kFunnelTauBar -- out of the header and into a library TU, so this
// standalone compile needs the TU to resolve them.
//
// `build/libhven.a` JOINS THE COMMAND LINE TOO (fix round 1). The SQP tree is
// NOT header-only today -- T3 carved sqp_options.cpp and src/linear has its
// own TUs -- so linking only funnel.cpp still leaves undefined references
// (validate_sqp_options, SymmetricFactor, ...) that have nothing to do with
// this probe; the archive is now required to resolve those. It does NOT
// reintroduce the shipped-constant trap: a linker satisfies a symbol from an
// explicit object file before it ever opens an archive member, so `reset()`
// still resolves to the funnel.cpp compiled here -- against the PATCHED
// header -- and the archive's own copy of FunnelStrategy is never pulled.
//
// WHAT IT MEASURES: the COLD ARM of all six `tests/sqp/support/hs_sweeps.h`
// corpus problems (the same vehicle `tests/test_hs_sweeps.cpp`'s
// `run_cold_grid`/`spec_grid` use, at the same default options
// `sweep_options(StartLevel::kCold, /*full_step=*/false, /*enable_soc=*/false,
// spec.max_iter)` builds -- KD and SOC both off, the simplest baseline) --
// and prints, per problem, majors/minors/factorizations summed over the whole
// p-grid plus the objective at the last grid point, so a diff between two
// `kFunnelTauBar` values is a diff of this program's stdout.

#include <cstdio>

#include <hven/drivers/sqp_driver.h>

#include "support/hs_sweeps.h"

using namespace hven::solvers;
using hven::Index;
using hven::Vec;
using namespace hven::solvers::test_support;

namespace {

SqpOptions cold_options(Index max_iter) {
    SqpOptions opts;
    opts.kkt_tol = 1e-6;
    opts.feas_tol = 1e-6;
    opts.max_iter = max_iter;
    opts.adaptive_mu = false;
    opts.start_level = StartLevel::kCold;
    opts.warm_full_step = false;
    opts.enable_soc = false;
    return opts;
}

std::vector<double> spec_grid(const HsSweepSpec &spec) {
    std::vector<double> g;
    for (double v = spec.p0; v <= spec.p1 + 1e-12; v += spec.dp) {
        g.push_back(v);
    }
    return g;
}

} // namespace

int main() {
    std::printf("tau_bar=%.6g\n", kFunnelTauBar);
    Index grand_majors = 0, grand_minors = 0, grand_fact = 0;
    for (const HsSweepSpec &spec : hs_sweep_specs()) {
        auto model = make_hs_sweep(spec.number);
        const std::vector<double> grid = spec_grid(spec);
        Index majors = 0, minors = 0, fact = 0, rejects = 0;
        bool all_optimal = true;
        double f_last = 0.0;
        for (double p : grid) {
            SqpDriver driver(cold_options(spec.max_iter));
            model->set_parameters(Vec::Constant(1, p));
            const SqpSolution sol = driver.solve(*model, model->start_point());
            all_optimal = all_optimal && sol.status == SqpStatus::kOptimal;
            majors += sol.counters.major_iters;
            minors += sol.counters.qp_minor_iters;
            fact += sol.counters.factorizations;
            rejects += sol.counters.rejected_steps;
            f_last = sol.f;
        }
        std::printf("  HS%-3d majors=%4lld minors=%5lld fact=%4lld rejects=%3lld all_optimal=%d "
                    "f(p1)=%.10g\n",
                    spec.number, (long long)majors, (long long)minors, (long long)fact,
                    (long long)rejects, all_optimal ? 1 : 0, f_last);
        grand_majors += majors;
        grand_minors += minors;
        grand_fact += fact;
    }
    std::printf("  TOTAL   majors=%4lld minors=%5lld fact=%4lld\n", (long long)grand_majors,
                (long long)grand_minors, (long long)grand_fact);
    return 0;
}
