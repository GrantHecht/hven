#!/usr/bin/env python3
"""Apply (or restore) one declared mutation (sparse-equil v4, ../DECLARATION-SEQ4.md). usage: mutate.py <id> | restore.
THE TRIGGER
  Mt1 no-trigger:       the platform numerics' check never asks for a retake (src/kkt/kkt_factor.cpp).
  Mt2 tol-off:          kPlatformSolveFailureTol 1e-5 -> 1e+5, the trigger blind (include/hven/detail/kkt/kkt_factor.h).
  Mt3 ungated:          a request is taken on a factorization that reported NO perturbed pivot (src/kkt/kkt_factor.cpp).
THE ESCALATION
  Me1 no-refusal:       the scaled numerics' check ignores the engine's refusal (backward error only, round v3's rule).
  Me2 no-escalation:    the scaled numerics are never judged (src/kkt/kkt_factor.cpp, header predicate).
THE MATCHING AND THE SCALED SOLVE
  Mm1 no-row-dual:      the Dijkstra dual update leaves the matched rows' u alone (src/linear/matching_scaling.cpp).
  Mm2 row-dual-only:    the symmetric scaling reads exp(u_i) instead of exp((u_i + v_i)/2).
  Mm3 greedy-only:      no augmenting search (the initial greedy matching is returned).
  Mm4 unscaled-solve:   the scaled retake's solve does not scale the answer back by D (src/kkt/kkt_factor.cpp)."""
import subprocess
import sys

K = "src/kkt/kkt_factor.cpp"
KH = "include/hven/detail/kkt/kkt_factor.h"
M = "src/linear/matching_scaling.cpp"


def sub1(path, old, new):
    s = open(path).read()
    if s.count(old) != 1:
        sys.exit(f"MUTATION_DOES_NOT_APPLY {path} (count {s.count(old)}): {old[:60]!r}")
    open(path, "w").write(s.replace(old, new))


arg = sys.argv[1]
if arg == "restore":
    subprocess.check_call(["git", "checkout", "--", K, KH, M])
    print("RESTORED")
    sys.exit(0)
if arg == "Mt1":
    sub1(K, "        return missed ? PlatformRetake::kScaled : PlatformRetake::kNone;",
         "        return PlatformRetake::kNone;")
elif arg == "Mt2":
    sub1(KH, "inline constexpr double kPlatformSolveFailureTol = 1e-5;",
         "inline constexpr double kPlatformSolveFailureTol = 1e+5;")
elif arg == "Mt3":
    sub1(K, "    const bool warranted = !pp.has_value() || *pp > 0;", "    const bool warranted = true;")
elif arg == "Me1":
    sub1(K, "    return (missed || engine_refused) ? PlatformRetake::kMatched : PlatformRetake::kNone;",
         "    return missed ? PlatformRetake::kMatched : PlatformRetake::kNone;")
elif arg == "Me2":
    sub1(KH, "        return k.scaled_perturbed_pivots.value_or(0) > 0;", "        return false;")
elif arg == "Mm1":
    sub1(M, "                if (r >= 0) {\n                    u(r) += shift;\n                }",
         "                (void)r;")
elif arg == "Mm2":
    sub1(M, "            const double di = std::exp(0.5 * (m.u(i) + m.v(i)));",
         "            const double di = std::exp(m.u(i));")
elif arg == "Mm3":
    sub1(M, "        if (out.col_of_row[static_cast<std::size_t>(i0)] >= 0) {\n            continue;\n        }",
         "        if (true) {\n            continue;\n        }")
elif arg == "Mm4":
    sub1(K, "        x.array() *= k.retake_scale.array();\n", "")
else:
    sys.exit("UNKNOWN")
print(f"APPLIED {arg}")
