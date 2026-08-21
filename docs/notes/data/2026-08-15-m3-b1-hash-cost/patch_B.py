#!/usr/bin/env python3
"""B1 arm-B patch: remove the retarget's extra steady-state pattern hash.

Applied to a THROWAWAY worktree at 31e57b0 for measurement only. Never
committed. Reverted with `git checkout --` after the arm-B build links.

Steady state before: 3 pattern_hash(K) per SSN major
  (1) call-site needs_analysis
  (2) factorize_checked's own needs_analysis
  (3) SymmetricFactor::factorize's internal hven::pattern_hash
Steady state after:  2  -- the dissolved seam's count.

Analysis branch before: 5 (the two above + the record + analyze()'s own +
factorize()'s own). After: 3 (decision + analyze()'s + factorize()'s).

Counter contract preserved exactly: the call site still decides
symbolic_analyses from a value computed BEFORE the factorize, and
factorize_checked still analyzes iff that decision said so.
"""

import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1])


def sub(path, old, new, count=1):
    p = ROOT / path
    s = p.read_text()
    n = s.count(old)
    if n != count:
        raise SystemExit(f"patch_B: {path}: expected {count} occurrence(s), found {n}\n{old!r}")
    p.write_text(s.replace(old, new))
    print(f"patched {path} ({count})")


# ---- kkt_calls.h: the threaded decision -------------------------------------
sub(
    "include/hven/detail/sqp/kkt_calls.h",
    """// True iff factorize_checked() would run an analysis for K. SQP call sites
// consult this before factorize_checked() to preserve their
// symbolic_analyses counting contract.
bool needs_analysis(const KktFactor &k, const SpMatU &K);""",
    """// True iff factorize_checked() would run an analysis for K. SQP call sites
// consult this before factorize_checked() to preserve their
// symbolic_analyses counting contract.
bool needs_analysis(const KktFactor &k, const SpMatU &K);

// [B1 MEASUREMENT SCAFFOLD -- NOT SHIPPED] The same decision, carrying the
// pattern hash it was taken on, so factorize_checked() need not recompute it.
struct AnalysisDecision {
    bool needed = false;
    std::uint64_t hash = 0;
};
AnalysisDecision analysis_decision(const KktFactor &k, const SpMatU &K);
hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatU &K,
                                                const AnalysisDecision &d);""",
)

# ---- kkt_calls.cpp ----------------------------------------------------------
sub(
    "src/sqp/kkt_calls.cpp",
    """hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatU &K) {
    if (needs_analysis(k, K)) {
        k.factor.analyze(K);
        k.analyzed_pattern = hven::pattern_hash(K);
        k.analyzed = true;
    }
""",
    """AnalysisDecision analysis_decision(const KktFactor &k, const SpMatU &K) {
    AnalysisDecision d;
    d.hash = hven::pattern_hash(K);
    d.needed = !k.analyzed || d.hash != k.analyzed_pattern;
    return d;
}

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatU &K,
                                                const AnalysisDecision &d) {
    if (d.needed) {
        k.factor.analyze(K);
        k.analyzed_pattern = d.hash;
        k.analyzed = true;
    }

    hven::linear::FactorizeOutcome outcome = k.factor.factorize(K);
    if (outcome.status == hven::linear::FactorizeOutcome::Status::kBackendError) {
        throw std::runtime_error(
            fmt::format("KktFactor::factorize_checked: factorization failed, backend error {}",
                        outcome.backend_code));
    }
    return outcome;
}

hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatU &K) {
    if (needs_analysis(k, K)) {
        k.factor.analyze(K);
        k.analyzed_pattern = hven::pattern_hash(K);
        k.analyzed = true;
    }
""",
)

# ---- ssn_engine.h: the per-major site and the deferred-certification site ----
sub(
    "include/hven/detail/sqp/ssn_engine.h",
    """                if (detail::needs_analysis(kkt_, k_)) {
                    ++out->symbolic_analyses;
                }
                detail::factorize_checked(kkt_, k_);""",
    """                const detail::AnalysisDecision d_ = detail::analysis_decision(kkt_, k_);
                if (d_.needed) {
                    ++out->symbolic_analyses;
                }
                detail::factorize_checked(kkt_, k_, d_);""",
)

sub(
    "include/hven/detail/sqp/ssn_engine.h",
    """            if (detail::needs_analysis(kkt_, k_)) {
                ++out->symbolic_analyses;
            }
            detail::factorize_checked(kkt_, k_);""",
    """            const detail::AnalysisDecision d_ = detail::analysis_decision(kkt_, k_);
            if (d_.needed) {
                ++out->symbolic_analyses;
            }
            detail::factorize_checked(kkt_, k_, d_);""",
)

# ---- qp_engine.h: the border rebuild site -----------------------------------
sub(
    "include/hven/detail/sqp/qp_engine.h",
    """        if (detail::needs_analysis(border.kkt, border.k0.K)) {
            ++counters.symbolic_analyses;
        }
        detail::factorize_checked(border.kkt, border.k0.K);""",
    """        const detail::AnalysisDecision d_ = detail::analysis_decision(border.kkt, border.k0.K);
        if (d_.needed) {
            ++counters.symbolic_analyses;
        }
        detail::factorize_checked(border.kkt, border.k0.K, d_);""",
)

print("patch_B: applied")
