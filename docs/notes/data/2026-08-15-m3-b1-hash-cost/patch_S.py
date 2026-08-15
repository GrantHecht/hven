#!/usr/bin/env python3
"""B1 arm-S patch: the direct cost-attribution scaffold (plan step 2).

Applied to a THROWAWAY worktree at 31e57b0 for measurement only. Never
committed. Reverted with `git checkout --` after the arm-S build links.

Accumulates, per process, and prints to stderr at exit:
  * hven::pattern_hash  -- call count, summed nnz, summed nanoseconds
  * session_->factorize -- call count, summed nanoseconds (the numeric
    factorization the disclosure names as the comparator)
  * SsnEngine's major loop -- iteration count, summed nanoseconds
    (the "SSN-major wall-clock" the decision rule is keyed on)
"""

import pathlib
import sys

ROOT = pathlib.Path(sys.argv[1])


def sub(path, old, new, count=1):
    p = ROOT / path
    s = p.read_text()
    n = s.count(old)
    if n != count:
        raise SystemExit(f"patch_S: {path}: expected {count} occurrence(s), found {n}\n{old!r}")
    p.write_text(s.replace(old, new))
    print(f"patched {path} ({count})")


PROBE_HEADER = r"""// [B1 MEASUREMENT SCAFFOLD -- NOT SHIPPED]
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace hven_b1_probe {

struct Acc {
    std::uint64_t calls = 0;
    std::uint64_t units = 0; // nnz for the hash, unused elsewhere
    std::uint64_t ns = 0;
};

// Namespace-scope inline variables (trivially destructible, constant
// initialization) so no static-destruction-order question can arise between
// them and the exit dump below.
inline Acc g_hash{};
inline Acc g_fact{};
inline Acc g_major{};

inline Acc &hash_acc() { return g_hash; }
inline Acc &fact_acc() { return g_fact; }
inline Acc &major_acc() { return g_major; }

struct Timer {
    Acc &a;
    std::chrono::steady_clock::time_point t0;
    explicit Timer(Acc &acc) : a(acc), t0(std::chrono::steady_clock::now()) {}
    ~Timer() {
        a.ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - t0)
                .count());
        ++a.calls;
    }
};

struct Dump {
    ~Dump() {
        std::fprintf(stderr,
                     "[B1PROBE] hash_calls=%llu hash_nnz=%llu hash_ns=%llu "
                     "fact_calls=%llu fact_ns=%llu major_calls=%llu major_ns=%llu\n",
                     (unsigned long long)hash_acc().calls, (unsigned long long)hash_acc().units,
                     (unsigned long long)hash_acc().ns, (unsigned long long)fact_acc().calls,
                     (unsigned long long)fact_acc().ns, (unsigned long long)major_acc().calls,
                     (unsigned long long)major_acc().ns);
    }
};

inline Dump &dump_at_exit() {
    static Dump d;
    return d;
}

inline bool armed = (dump_at_exit(), true);

} // namespace hven_b1_probe
"""

(ROOT / "include/hven/core/b1_probe.h").write_text(PROBE_HEADER)
print("wrote include/hven/core/b1_probe.h")

# ---- pattern_hash: count, nnz, ns -------------------------------------------
sub(
    "src/core/pattern_hash.cpp",
    '#include "hven/core/pattern_hash.h"\n',
    '#include "hven/core/pattern_hash.h"\n#include "hven/core/b1_probe.h"\n',
)
sub(
    "src/core/pattern_hash.cpp",
    """    const Index rows = static_cast<Index>(A.rows());""",
    """    hven_b1_probe::Timer b1_t(hven_b1_probe::hash_acc());
    hven_b1_probe::hash_acc().units += static_cast<std::uint64_t>(A.nonZeros());

    const Index rows = static_cast<Index>(A.rows());""",
)

# ---- the numeric factorization ----------------------------------------------
sub(
    "src/linear/symmetric_factor_mkl.cpp",
    '#include "hven/core/pattern_hash.h"\n',
    '#include "hven/core/pattern_hash.h"\n#include "hven/core/b1_probe.h"\n',
)
sub(
    "src/linear/symmetric_factor_mkl.cpp",
    """        backend_code = session_->factorize(A);""",
    """        hven_b1_probe::Timer b1_t(hven_b1_probe::fact_acc());
        backend_code = session_->factorize(A);""",
)

# ---- the SSN major loop ------------------------------------------------------
sub(
    "include/hven/detail/sqp/ssn_engine.h",
    """        for (Index it = 0;; ++it) {
            const detail::SsnNorms nrm =""",
    """        for (Index it = 0;; ++it) {
            hven_b1_probe::Timer b1_t(hven_b1_probe::major_acc());
            const detail::SsnNorms nrm =""",
)
sub(
    "include/hven/detail/sqp/ssn_engine.h",
    """#include <hven/detail/sqp/kkt_calls.h>""",
    """#include <hven/core/b1_probe.h>
#include <hven/detail/sqp/kkt_calls.h>""",
)

print("patch_S: applied")
