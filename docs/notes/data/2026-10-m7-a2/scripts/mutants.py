#!/usr/bin/env python3
"""A2 mutation run (design page section 6.1 G5): each mutant applied alone, hven_sqp_tests
rebuilt (Release), the A2 filter run; KILLED iff the build succeeds and the filter fails.
Source restored with `git checkout -- FILE` after every mutant. Every build/run is a leg of
build.sh / leg.sh (pgrep + flock /tmp/box-build.lock inside)."""
import subprocess
import sys

W = "/home/ghecht/Projects/hven/.claude/worktrees/agent-aa38d0451de957235"
ENG = "src/qp/ipqp_engine.cpp"
HDR = "include/hven/detail/qp/ipqp_engine.h"
SQP = "tests/sqp/hven_sqp_tests"
F = "IpqpMu0Rule*:IpqpMuResidualFloor*"
F_FOLD = "AccumulateIpqpCounters.*"

MUTANTS = [
    ("M1 the 1.5 positivity shift factor", HDR,
     "inline constexpr double kIpqpMu0ShiftFactor = 1.5;",
     "inline constexpr double kIpqpMu0ShiftFactor = 2.0;", F),
    ("M2 the 0.5 centring fraction", HDR,
     "inline constexpr double kIpqpMu0CentreFraction = 0.5;",
     "inline constexpr double kIpqpMu0CentreFraction = 0.25;", F),
    ("M3 the band's ceiling dropped", ENG,
     "ok = zlo > 0.0 && mu_derived >= iopts.ipqp_min_mu && mu_derived <= iopts.ipqp_init_mu;",
     "ok = zlo > 0.0 && mu_derived >= iopts.ipqp_min_mu;", F),
    ("M4 the band's floor dropped", ENG,
     "ok = zlo > 0.0 && mu_derived >= iopts.ipqp_min_mu && mu_derived <= iopts.ipqp_init_mu;",
     "ok = zlo > 0.0 && mu_derived <= iopts.ipqp_init_mu;", F),
    ("M5 strict positivity not checked", ENG,
     "ok = zlo > 0.0 && mu_derived >= iopts.ipqp_min_mu && mu_derived <= iopts.ipqp_init_mu;",
     "ok = mu_derived >= iopts.ipqp_min_mu && mu_derived <= iopts.ipqp_init_mu;", F),
    ("M6 the fallback keeps the unit-scaled point", ENG,
     "        } else {\n            cold_start();\n            out.counters.ipqp_mu0_fallbacks = 1;",
     "        } else {\n            out.counters.ipqp_mu0_fallbacks = 1;", F),
    ("M7 the ladder high-water mark is not restored", ENG,
     "        rho_dem_max = saved_rho_dem_max;", "        (void)saved_rho_dem_max;", F),
    ("M8 the ladder memory is not restored", ENG,
     "        rho_dem_last = saved_rho_dem_last;", "        (void)saved_rho_dem_last;", F),
    ("M9 the unit-scaled point is not unit-scaled", ENG,
     "            w.zl(i) = detail::ipqp_has_lower(bounds.lower(i)) ? w.dL(i) : 0.0;",
     "            w.zl(i) = detail::ipqp_has_lower(bounds.lower(i)) ? 2.0 * w.dL(i) : 0.0;", F),
    ("M10 the row estimates keep the mu-placed values", ENG,
     "            w.lam_est_i.setZero();", "            (void)0;", F),
    ("M11 the rule runs on warm starts", ENG,
     "    if (!started_warm && iopts.ipqp_mu0_rule == IpqpMu0Rule::kComplementarity && npairs > 0) {",
     "    if (iopts.ipqp_mu0_rule == IpqpMu0Rule::kComplementarity && npairs > 0) {", F),
    ("M12 the derived counter is not written", ENG,
     "            out.counters.ipqp_mu0_derived = 1;", "            (void)0;", F),
    ("M13 the floor never binds", ENG,
     "                if (floor_k > target && floor_k > iopts.ipqp_min_mu) {",
     "                if (false && floor_k > target && floor_k > iopts.ipqp_min_mu) {", F),
    ("M14 the floor's beta is r_s, not max(1, r_s)", ENG,
     "                const double floor_k = floor_mu_s * r / std::max(1.0, floor_r_s);",
     "                const double floor_k = floor_mu_s * r / floor_r_s;", F),
    ("M15 the floor reads complementarity too", ENG,
     "                const double r = std::max({res.primal_eq, res.primal_iq, res.stationarity});",
     "                const double r = std::max({res.primal_eq, res.primal_iq, res.stationarity,\n"
     "                                           res.complementarity});", F),
    ("M16 the bind counter is not counted", ENG,
     "                    ++out.counters.ipqp_mu_floor_binds;", "                    (void)0;", F),
    ("M17 the anchor is re-taken every iteration", ENG,
     "        if (iopts.ipqp_mu_residual_floor && !floor_anchor_set) {",
     "        if (iopts.ipqp_mu_residual_floor) {", F),
    ("M18 the warm-kill does not re-anchor", ENG,
     "            floor_anchor_set = false;\n            iters_base = out.counters.ipqp_iters;",
     "            iters_base = out.counters.ipqp_iters;", F),
    ("M19 validation admits the count sentinel", "src/drivers/sqp_options.cpp",
     "          opts.ipqp.ipqp_mu0_rule == IpqpMu0Rule::kComplementarity)) {",
     "          opts.ipqp.ipqp_mu0_rule == IpqpMu0Rule::kComplementarity || true)) {", F),
    ("M20 the fold overwrites ipqp_mu_floor_binds", "src/drivers/sqp_solver.cpp",
     "    total.ipqp_mu_floor_binds += one.ipqp_mu_floor_binds;",
     "    total.ipqp_mu_floor_binds = one.ipqp_mu_floor_binds;", F_FOLD),
]


def run(cmd):
    return subprocess.run(cmd, cwd=W, shell=False).returncode


def main():
    only = sys.argv[1:]
    results = []
    for name, path, old, new, filt in MUTANTS:
        if only and name.split()[0] not in only:
            continue
        src = open(f"{W}/{path}").read()
        n = src.count(old)
        if n != 1:
            results.append((name, f"SITE_COUNT_{n}"))
            print(f"MUTANT {name}: SITE_COUNT {n}", flush=True)
            continue
        open(f"{W}/{path}", "w").write(src.replace(old, new))
        try:
            b = run(["bash", ".scratch/a2/build.sh", "rel", "hven_sqp_tests"])
            if b != 0:
                verdict = "BUILD_FAIL"
            else:
                t = run(["bash", ".scratch/a2/leg.sh", "mutant", f".scratch/a2/build-rel/{SQP}",
                         f"--gtest_filter={filt}"])
                verdict = "KILLED" if t != 0 else "SURVIVED"
                if t != 0:
                    log = open(f"{W}/.scratch/a2/logs/mutant.log").read()
                    failed = [l for l in log.splitlines() if l.startswith("[  FAILED  ] ") and "." in l]
                    verdict += " by " + ";".join(sorted(set(f.split()[3] for f in failed)))[:300]
        finally:
            run(["git", "checkout", "--", path])
        results.append((name, verdict))
        print(f"MUTANT {name}: {verdict}", flush=True)
    print("SUMMARY")
    for name, v in results:
        print(f"  {name}: {v}")
    rc = run(["bash", ".scratch/a2/build.sh", "rel", "hven_sqp_tests"])
    print(f"RESTORED_BUILD rc={rc}")


if __name__ == "__main__":
    main()
