#!/usr/bin/env python3
# sparse-equil v4 r2 (../../DECLARATION-SEQ4-r2.md): ../../legs/mutate.py extended. Mt1 and Me1 now hit the ONE verdict
# function (platform_retake_verdict, src/kkt/kkt_factor.cpp) that both the eliminated and the bordered site call (I3);
# the rest of v4's set is unchanged; the r2 must-kills follow.
"""Apply (or restore) one declared mutation (sparse-equil v4 r2). usage: mutate.py <id> | restore.
THE TRIGGER (the verdict function, both sites)
  Mt1 no-trigger:       the verdict never asks for the scaled retake on platform numerics.
  Mt2 tol-off:          kPlatformSolveFailureTol 1e-5 -> 1e+5, the trigger blind (include/hven/detail/kkt/kkt_factor.h).
  Mt3 ungated:          a request is taken on a factorization that reported NO perturbed pivot.
THE ESCALATION (the verdict function, both sites)
  Me1 no-refusal:       the scaled numerics' verdict ignores the engine's refusal (backward error only).
  Me2 no-escalation:    the scaled numerics are never judged (header predicate).
THE MATCHING AND THE SCALED SOLVE
  Mm1 no-row-dual, Mm2 row-dual-only, Mm3 greedy-only (src/linear/matching_scaling.cpp); Mm4 unscaled-solve.
r2 MUST-KILLS
  Mi1 escaping-retake:  take_retake catches neither BackendError nor std::invalid_argument (I1).
  Mr1 request-survives: run() does not drop a pending K0 request at entry (I2).
  Mr2 latch-on-pending: rebuild_would_be_noop() ignores a pending K0 request (I2, the spurious conditioning latch).
  Mb1 bordered-blind:   solve_bordered_eqp never consults the verdict (I3, the bordered site).
  Md1 inplace-armed:    a failed in-place retake leaves the check armed (M8, the disarm).
  Md2 request-armed:    a failed requested retake leaves the check armed (M8, the disarm)."""
import subprocess
import sys

K = "src/kkt/kkt_factor.cpp"
KH = "include/hven/detail/kkt/kkt_factor.h"
M = "src/linear/matching_scaling.cpp"
W = "src/qp/walk/walk_engine.cpp"
BE = "include/hven/detail/qp/walk/bordered_eqp.h"


def sub1(path, old, new):
    s = open(path).read()
    if s.count(old) != 1:
        sys.exit(f"MUTATION_DOES_NOT_APPLY {path} (count {s.count(old)}): {old[:60]!r}")
    open(path, "w").write(s.replace(old, new))


arg = sys.argv[1]
if arg == "restore":
    subprocess.check_call(["git", "checkout", "--", K, KH, M, W, BE])
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
elif arg == "Mi1":
    sub1(K, "    } catch (const hven::BackendError &) {\n        return false;\n    } catch (const std::invalid_argument &) {\n"
            "        return false;\n    }",
         "    } catch (const std::bad_alloc &) {\n        throw;\n    }")
elif arg == "Mr1":
    sub1(W, "    if (border_ != nullptr) {\n        detail::clear_platform_retake_request(border_->kkt);\n    }\n"
            "    struct BorderRetakeExit {",
         "    struct BorderRetakeExit {")
elif arg == "Mr2":
    sub1(W, "    if (border.kkt.rebuild_pending) {\n        return false;\n    }\n", "")
elif arg == "Mb1":
    sub1(BE, "        platform_retake = detail::platform_retake_verdict(k0_factor, missed, !(ratio <= 1.0));",
         "        (void)missed;")
elif arg == "Md1":
    sub1(K, "    k.numerics = PlatformRetake::kNone;\n    k.check_armed = false;\n    k.scaled_perturbed_pivots.reset();\n"
            "    k.retake_scale.resize(0);\n    return before != PlatformRetake::kNone;",
         "    k.numerics = PlatformRetake::kNone;\n    k.scaled_perturbed_pivots.reset();\n"
         "    k.retake_scale.resize(0);\n    return before != PlatformRetake::kNone;")
elif arg == "Md2":
    sub1(K, "    if (!take_retake(k, K, request)) {\n        k.check_armed = false;\n    }",
         "    (void)take_retake(k, K, request);")
else:
    sys.exit("UNKNOWN")
print(f"APPLIED {arg}")
