#!/usr/bin/env python3
"""sparse-equil v4 r2, I4 DEV INSTRUMENT (never shipped; applied only on the local, never-pushed branch seq4r2-diag).
Two changes to the r2 code, nothing else:
  1. HVEN_SEQ_ME1 (environment, read once): when set, the verdict drops the refusal clause -- Me1 at run time, so ONE
     lane binary runs both arms (unset: the r2 verdict, byte-for-byte the shipped logic).
  2. One stderr line per verdict at each site (SEQDIAG ...): the site, the round, the numerics' tier, the perturbed
     pivots, missed, engine_refused, the verdict, dual_mu, n_free and the constraint rows (nc), and the residual ratio.
     Printing reads values only; the numerics are untouched (checked: the instrument's rows with HVEN_SEQ_ME1 unset
     must equal the r2 build's rows).
usage: diag_patch.py <worktree root>"""
import sys

root = sys.argv[1].rstrip("/")


def sub1(rel, old, new):
    p = f"{root}/{rel}"
    s = open(p).read()
    assert s.count(old) == 1, (rel, old[:70])
    open(p, "w").write(s.replace(old, new))


sub1("src/kkt/kkt_factor.cpp",
     "    return (missed || engine_refused) ? PlatformRetake::kMatched : PlatformRetake::kNone;",
     "    static const bool me1 = std::getenv(\"HVEN_SEQ_ME1\") != nullptr;\n"
     "    return (missed || (!me1 && engine_refused)) ? PlatformRetake::kMatched : PlatformRetake::kNone;")
sub1("src/kkt/kkt_factor.cpp", "#include <cmath>\n", "#include <cmath>\n#include <cstdlib>\n")
# eliminated site
sub1("include/hven/detail/qp/walk/eqp_solve.h",
     "        const detail::PlatformRetake retake = detail::platform_solve_response(\n"
     "            kkt, asm_.K, rhs, opts.dual_regularization == 0.0 ? y : y_refined, refused);\n",
     "        const int diag_tier = static_cast<int>(kkt.numerics);\n"
     "        const long diag_pp = kkt.numerics == detail::PlatformRetake::kScaled\n"
     "                                 ? static_cast<long>(kkt.scaled_perturbed_pivots.value_or(-1))\n"
     "                                 : static_cast<long>(kkt.factor.inertia().perturbed_pivots.value_or(-1));\n"
     "        const detail::PlatformRetake retake = detail::platform_solve_response(\n"
     "            kkt, asm_.K, rhs, opts.dual_regularization == 0.0 ? y : y_refined, refused);\n"
     "        std::fprintf(stderr, \"SEQDIAG site=elim tier=%d pp=%ld refused=%d ratio=%.3e verdict=%d mu=%.3e nfree=%ld nc=%ld\\n\",\n"
     "                     diag_tier, diag_pp, refused ? 1 : 0, ratio, static_cast<int>(retake), opts.dual_regularization,\n"
     "                     static_cast<long>(n_free), static_cast<long>(nc));\n")
sub1("include/hven/detail/qp/walk/eqp_solve.h", "#include <algorithm>\n", "#include <algorithm>\n#include <cstdio>\n")
# bordered site
sub1("include/hven/detail/qp/walk/bordered_eqp.h",
     "        platform_retake = detail::platform_retake_verdict(k0_factor, missed, !(ratio <= 1.0));\n",
     "        platform_retake = detail::platform_retake_verdict(k0_factor, missed, !(ratio <= 1.0));\n"
     "        std::fprintf(stderr, \"SEQDIAG site=border tier=%d pp=%ld missed=%d refused=%d ratio=%.3e verdict=%d mu=%.3e n=%ld nc=%ld\\n\",\n"
     "                     static_cast<int>(k0_factor.numerics),\n"
     "                     static_cast<long>(k0_factor.numerics == detail::PlatformRetake::kScaled\n"
     "                                           ? k0_factor.scaled_perturbed_pivots.value_or(-1)\n"
     "                                           : k0_factor.factor.inertia().perturbed_pivots.value_or(-1)),\n"
     "                     missed ? 1 : 0, !(ratio <= 1.0) ? 1 : 0, ratio, static_cast<int>(platform_retake),\n"
     "                     opts.dual_regularization, static_cast<long>(n), static_cast<long>(nc));\n")
sub1("include/hven/detail/qp/walk/bordered_eqp.h", "#include <algorithm>\n", "#include <algorithm>\n#include <cstdio>\n")
print("DIAG_PATCH_APPLIED")
