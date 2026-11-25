#!/usr/bin/env python3
"""sparse-equil v4 r2, I4 DEV INSTRUMENT, second layer (never shipped; local branch seq4r2-diag2 = seq4r2-diag + this).
HVEN_SEQ_QPDUMP=<dir>: every walk solve during which at least one REFUSAL-ONLY escalation was taken (the r2 verdict says
kMatched while the backward error alone would not: platform_solve_response(..., engine_refused=false) == kNone on the
eliminated path, !missed on the bordered path) is written as one D4QP v1 file <dir>/qp<seq>.txt (tests/qp/fixtures/
e2-d4/FORMAT.txt; the writer is E2-D4's, docs/notes/data/2026-09-m7-w2-e2-d4-feasible-qps/d4-patch.diff.txt, re-spelled to
the current QpOptions names), with one stderr line SEQDUMP. usage: dump_patch.py <worktree root>"""
import sys

root = sys.argv[1].rstrip("/")


def sub1(rel, old, new):
    p = f"{root}/{rel}"
    s = open(p).read()
    assert s.count(old) == 1, (rel, old[:70])
    open(p, "w").write(s.replace(old, new))


sub1("include/hven/detail/kkt/kkt_factor.h", "inline constexpr double kPlatformSolveFailureTol = 1e-5;\n",
     "inline constexpr double kPlatformSolveFailureTol = 1e-5;\ninline long seqdiag_refusal_only = 0; // DIAG\n")
sub1("include/hven/detail/qp/walk/eqp_solve.h",
     "        std::fprintf(stderr, \"SEQDIAG site=elim",
     "        if (retake == detail::PlatformRetake::kMatched &&\n"
     "            detail::platform_solve_response(kkt, asm_.K, rhs, opts.dual_regularization == 0.0 ? y : y_refined,\n"
     "                                            false) == detail::PlatformRetake::kNone) {\n"
     "            ++detail::seqdiag_refusal_only;\n"
     "        }\n"
     "        std::fprintf(stderr, \"SEQDIAG site=elim")
sub1("include/hven/detail/qp/walk/bordered_eqp.h",
     "        std::fprintf(stderr, \"SEQDIAG site=border",
     "        if (platform_retake == detail::PlatformRetake::kMatched && !missed) {\n"
     "            ++detail::seqdiag_refusal_only;\n"
     "        }\n"
     "        std::fprintf(stderr, \"SEQDIAG site=border")
W = "src/qp/walk/walk_engine.cpp"
sub1(W, "    border_valid_ = false;\n\n    // THE PLATFORM-SOLVE RESPONSE'S LIFETIME ON K0",
     "    border_valid_ = false;\n    const long seqdiag_ro0 = detail::seqdiag_refusal_only;\n\n    // THE PLATFORM-SOLVE RESPONSE'S LIFETIME ON K0")
sub1(W, "    // Emit a record to the ledger if attached.\n    if (ledger_ != nullptr) {",
     r'''    if (const char *dd = std::getenv("HVEN_SEQ_QPDUMP"); dd != nullptr && detail::seqdiag_refusal_only > seqdiag_ro0) {
        static long seq = 0;
        const long s = seq++;
        std::ofstream o(std::string(dd) + "/qp" + std::to_string(s) + ".txt");
        const auto dvec = [&](const char *name, const Vec &v) {
            o << name << " " << v.size() << "\n";
            char b[64];
            for (Index i = 0; i < v.size(); ++i) { std::snprintf(b, sizeof b, "%.17g\n", v(i)); o << b; }
        };
        const auto dsp = [&](const char *name, const SpMatRM &A) {
            o << name << " " << A.rows() << " " << A.cols() << " " << A.nonZeros() << "\n";
            char b[96];
            for (Index k = 0; k < A.outerSize(); ++k)
                for (SpMatRM::InnerIterator it(A, k); it; ++it) {
                    std::snprintf(b, sizeof b, "%ld %ld %.17g\n", (long)it.row(), (long)it.col(), it.value());
                    o << b;
                }
        };
        const auto divec = [&](const char *name, const std::vector<long> &v) {
            o << name << " " << v.size() << "\n";
            for (long x : v) o << x << "\n";
        };
        o << "D4QP 1\n" << "dims " << n << " " << me << " " << mi << "\n";
        char b[512];
        std::snprintf(b, sizeof b, "opts %.17g %.17g %.17g %.17g %ld %ld %.17g %d %.17g %d %d\n", opts_.primal_regularization,
                      opts_.dual_regularization, opts_.feasibility_tol, opts_.optimality_tol, (long)opts_.max_iterations.value_or(0),
                      (long)opts_.max_schur_border_dimension, opts_.schur_condition_max, (int)opts_.working_set_algebra,
                      opts_.trust_region_radius, (int)opts_.independent_working_set, threads_);
        o << b;
        std::snprintf(b, sizeof b, "overrides %.17g %.17g %.17g\n", overrides.trust_region_radius, overrides.primal_regularization,
                      overrides.dual_regularization);
        o << b;
        o << "warm " << (int)warm << " seed " << (seed != nullptr ? 1 : 0) << " hot " << (hot != nullptr ? 1 : 0) << " k0_reused "
          << (int)counters.k0_reused << "\n";
        std::snprintf(b, sizeof b, "result status=%d minors=%ld facts=%ld scaled=%ld matched=%ld retake_refusals=%ld hkc=%d maxiter=%ld\n",
                      (int)status, (long)counters.minor_iters, (long)counters.factorizations, (long)counters.sparse_scaled_retakes,
                      (long)counters.sparse_matched_retakes, (long)counters.sparse_retake_refusals, (int)opts_.hessian_known_convex,
                      (long)eff_max_iter);
        o << b;
        dsp("H", qp_in.H); dvec("g", qp_in.g); dsp("Ae", qp_in.Ae); dvec("be", qp_in.be); dsp("Ai", qp_in.Ai); dvec("bi", qp_in.bi);
        dvec("lower", qp_in.lower); dvec("upper", qp_in.upper);
        if (seed != nullptr) {
            dvec("seed_x", seed->x);
            std::vector<long> sb, sa;
            for (BoundState bs : seed->bound_state) sb.push_back((long)bs);
            for (bool a : seed->ineq_active) sa.push_back(a ? 1 : 0);
            divec("seed_bound_state", sb);
            divec("seed_ineq_active", sa);
        }
        dvec("x_final", out.x);
        std::vector<long> fb, fa;
        for (BoundState bs : ws.bound_state()) fb.push_back((long)bs);
        for (Index r : ws.active_ineq()) fa.push_back((long)r);
        divec("final_bound_state", fb);
        divec("final_active_ineq", fa);
        std::fprintf(stderr, "SEQDUMP seq=%ld status=%d minors=%ld scaled=%ld matched=%ld refusal_only=%ld\n", s, (int)status,
                     (long)counters.minor_iters, (long)counters.sparse_scaled_retakes, (long)counters.sparse_matched_retakes,
                     detail::seqdiag_refusal_only - seqdiag_ro0);
    }

    // Emit a record to the ledger if attached.
    if (ledger_ != nullptr) {''')
sub1(W, "#include <bit>\n", "#include <bit>\n#include <cstdio>\n#include <cstdlib>\n#include <fstream>\n")
print("DUMP_PATCH_APPLIED")
