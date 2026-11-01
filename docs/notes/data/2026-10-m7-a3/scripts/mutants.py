#!/usr/bin/env python3
"""A3 mutation run (design page s5.1 G5): each mutant applied alone, hven_sqp_tests (and, for the
dump/validation mutants, hven_driver_tests) rebuilt in Release, the A3 filter run; KILLED iff the
build succeeds and the filter fails. Source restored with `git checkout -- FILE` after every mutant.
Every build/run is a leg of build.sh / leg.sh (pgrep + flock /tmp/box-build.lock inside)."""
import subprocess
import sys

W = "/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff"
ENG = "src/qp/ipqp_engine.cpp"
OPT = "src/drivers/sqp_options.cpp"
SQP_BIN = "tests/sqp/hven_sqp_tests"
F = "IpqpLadderSkip*"
F_FOLD = "AccumulateIpqpCounters.*"

# (name, file, [(old, new), ...], filter)
MUTANTS = [
    ("K1 the field read replaced by the old literal 3", ENG,
     [("consec_modified >= iopts.ipqp_ladder_skip_after;", "consec_modified >= 3;")], F),
    ("K2 >= becomes >", ENG,
     [("consec_modified >= iopts.ipqp_ladder_skip_after;",
       "consec_modified > iopts.ipqp_ladder_skip_after;")], F),
    ("K3 the memory condition dropped", ENG,
     [("return rho_dem_last > 0.0 && consec_modified >= iopts.ipqp_ladder_skip_after;",
       "return consec_modified >= iopts.ipqp_ladder_skip_after;")], F),
    ("K4 the counter is not incremented", ENG,
     [("            ++out.counters.ipqp_ladder_zero_trial_skips;\n", "            (void)0;\n")], F),
    ("K5 the counter moved to the A2 estimate site", ENG,
     [("            ++out.counters.ipqp_ladder_zero_trial_skips;\n", "            (void)0;\n"),
      ("        const double saved_rho_dem_max = rho_dem_max;\n        double rho_dem = ladder_trial();",
       "        const double saved_rho_dem_max = rho_dem_max;\n"
       "        if (skip_licensed()) {\n            ++out.counters.ipqp_ladder_zero_trial_skips;\n        }\n"
       "        double rho_dem = ladder_trial();")], F),
    ("K6 the counter counts every iteration", ENG,
     [("        if (skip_licensed()) {\n            ++out.counters.ipqp_ladder_zero_trial_skips;",
       "        if (true) {\n            ++out.counters.ipqp_ladder_zero_trial_skips;")], F),
    ("K7 validation admits a negative count", OPT,
     [("    if (!(opts.ipqp.ipqp_ladder_skip_after >= 0)) {",
       "    if (false && !(opts.ipqp.ipqp_ladder_skip_after >= 0)) {")], F),
    ("K8 the dump line dropped", OPT,
     [("    kv_int(os, \"ipqp.ipqp_ladder_skip_after\",\n"
       "           static_cast<long long>(o.ipqp.ipqp_ladder_skip_after));\n", "")], F),
    ("K9 the fold overwrites ipqp_ladder_zero_trial_skips", "src/drivers/sqp_solver.cpp",
     [("    total.ipqp_ladder_zero_trial_skips += one.ipqp_ladder_zero_trial_skips;",
       "    total.ipqp_ladder_zero_trial_skips = one.ipqp_ladder_zero_trial_skips;")], F_FOLD),
]


def run(cmd):
    return subprocess.run(cmd, cwd=W, shell=False).returncode


def main():
    only = sys.argv[1:]
    results = []
    for name, path, edits, filt in MUTANTS:
        if only and name.split()[0] not in only:
            continue
        src = open(f"{W}/{path}").read()
        bad = [old for old, _ in edits if src.count(old) != 1]
        if bad:
            results.append((name, f"SITE_COUNT {[src.count(o) for o in bad]}"))
            print(f"MUTANT {name}: SITE_COUNT", flush=True)
            continue
        mutated = src
        for old, new in edits:
            mutated = mutated.replace(old, new)
        open(f"{W}/{path}", "w").write(mutated)
        try:
            b = run(["bash", ".scratch/a3/build.sh", "head", "rel", "hven_sqp_tests"])
            if b != 0:
                verdict = "BUILD_FAIL"
            else:
                t = run(["bash", ".scratch/a3/leg.sh", "mutant", f".scratch/a3/build-head-rel/{SQP_BIN}",
                         f"--gtest_filter={filt}"])
                verdict = "KILLED" if t != 0 else "SURVIVED"
                if t != 0:
                    log = open(f"{W}/.scratch/a3/logs/mutant.log").read()
                    failed = [l for l in log.splitlines() if l.startswith("[  FAILED  ] ") and "." in l]
                    verdict += " by " + ";".join(sorted(set(f.split()[3] for f in failed)))[:300]
        finally:
            run(["git", "checkout", "--", path])
        results.append((name, verdict))
        print(f"MUTANT {name}: {verdict}", flush=True)
    print("SUMMARY")
    for name, v in results:
        print(f"  {name}: {v}")
    rc = run(["bash", ".scratch/a3/build.sh", "head", "rel", "hven_sqp_tests"])
    print(f"RESTORED_BUILD rc={rc}")


if __name__ == "__main__":
    main()
