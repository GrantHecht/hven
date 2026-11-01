#!/usr/bin/env python3
"""A1 mutation run: each mutant applied alone, the named binaries rebuilt (Release), the
named filter run; KILLED iff the build succeeds and the filter fails. Source restored with
`git checkout -- FILE` after every mutant. Every build/run is a leg of build.sh / leg.sh
(pgrep + flock /tmp/box-build.lock inside)."""
import subprocess
import sys

W = "/home/ghecht/Projects/hven/.claude/worktrees/agent-ad5e6e56c9da5d8a5"
ENG = "src/qp/ipqp_engine.cpp"
SQP = "tests/sqp/hven_sqp_tests"
SEAM = "tests/hven_ipqp_seam_tests"
F_STOP = "IpqpMarginStop.*"
F_SEAM = "IpqpMarginSeams.*"

MUTANTS = [
    ("M1 exponent test never flags", ENG,
     "                   detail::IpqpBarrierNoiseClass::kSuspect;",
     "                   detail::IpqpBarrierNoiseClass::kPriced;", "hven_sqp_tests", SQP, F_STOP),
    ("M2 band fallback inverted", ENG, "                return z <= band;",
     "                return z > band;", "hven_sqp_tests", SQP, F_STOP),
    ("M3 ON-2X acts on band suspects at the candidate", ENG,
     "                              !(margin_arm == MarginArm::kExponentOnly && !marks.informative);",
     "                              true;", "hven_ipqp_seam_tests", SEAM, F_SEAM),
    ("M4 extension keeps the configured slack", ENG,
     "            ext_iopts.ipqp_converge_slack = 1.0;",
     "            ext_iopts.ipqp_converge_slack = iopts.ipqp_converge_slack;", "hven_sqp_tests", SQP,
     F_STOP),
    ("M5 warm-kill during the extension restarts cold", ENG,
     "                break; // A1 item 4: a warm-kill is an extension exit -> the candidate result.",
     "                (void)0;", "hven_sqp_tests", SQP, F_STOP),
    ("M6 evidence failure during the extension is carried", ENG,
     "        if (extending && evidence_failed) {", "        if (false && evidence_failed) {",
     "hven_ipqp_seam_tests", SEAM, F_SEAM),
    ("M7 demotion forces INACTIVE", ENG, "                f = IpqpFace::kUncertain;",
     "                f = IpqpFace::kInactive;", "hven_sqp_tests", SQP, F_STOP),
    ("M8 non-converged extension returns the live result", ENG,
     "        use_candidate || (extending && (!converged || final_read_refused));",
     "        use_candidate;", "hven_sqp_tests", SQP, F_STOP),
    ("M9 a budget-refused second read is finalised", ENG,
     "        use_candidate || (extending && (!converged || final_read_refused));",
     "        use_candidate || (extending && !converged && !final_read_refused);", "hven_sqp_tests",
     SQP, F_STOP),
    ("M10 ON-2X demotes by the band at an uninformative final pass", ENG,
     "            if (!(margin_arm == MarginArm::kExponentOnly && !final_marks.informative)) {",
     "            if (true) {", "hven_ipqp_seam_tests hven_sqp_tests", SEAM, F_SEAM),
    ("M11 validation admits the count sentinel", "src/drivers/sqp_options.cpp",
     "          opts.ipqp.ipqp_margin_stop == IpqpMarginStop::kExtendAndDemote)) {",
     "          opts.ipqp.ipqp_margin_stop == IpqpMarginStop::kExtendAndDemote || true)) {",
     "hven_sqp_tests", SQP, F_STOP),
    ("M12 fold overwrites ipqp_margin_demoted", "src/drivers/sqp_solver.cpp",
     "    total.ipqp_margin_demoted += one.ipqp_margin_demoted;",
     "    total.ipqp_margin_demoted = one.ipqp_margin_demoted;", "hven_sqp_tests", SQP,
     "IpqpCounters*:IpqpCountersTest*:*Accumulate*"),
    ("M13 a returned candidate's certify event is dropped", ENG,
     "    emit_final_events(out, converged || returns_candidate);",
     "    emit_final_events(out, converged);", "hven_sqp_tests", SQP, F_STOP),
    ("M14 rows read a scrambled previous multiplier", ENG,
     "                prev_yi = w.yi; // A1: rows read the exponent test too.",
     "                prev_yi = w.yi.reverse(); // A1 mutant", "hven_sqp_tests", SQP, F_STOP),
    ("M15 fallback counter counts informative suspects", ENG,
     "            out.counters.ipqp_margin_fallback_suspects += marks.informative ? 0 : marks.total;",
     "            out.counters.ipqp_margin_fallback_suspects += marks.total;", "hven_sqp_tests", SQP,
     F_STOP),
    ("M16 demotion disabled", ENG, "                demote = &final_marks;",
     "                demote = nullptr;", "hven_sqp_tests", SQP, F_STOP),
    ("M17 the candidate is not finalised first", ENG,
     "            finalize(cand, /*conv=*/true, IpqpEscape::kNone, nullptr);",
     "            (void)0;", "hven_sqp_tests", SQP, F_STOP),
]


def run(cmd):
    return subprocess.run(cmd, cwd=W, shell=False).returncode


def main():
    only = sys.argv[1:]
    results = []
    for name, path, old, new, targets, binary, filt in MUTANTS:
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
            b = run(["bash", ".scratch/a1i/build.sh", "rel"] + targets.split())
            if b != 0:
                verdict = "BUILD_FAIL"
            else:
                t = run(["bash", ".scratch/a1i/leg.sh", "mutant", f".scratch/a1i/build-rel/{binary}",
                         f"--gtest_filter={filt}"])
                verdict = "KILLED" if t != 0 else "SURVIVED"
                if t != 0:
                    log = open(f"{W}/.scratch/a1i/logs/mutant.log").read()
                    failed = [l for l in log.splitlines() if l.startswith("[  FAILED  ] ") and "." in l]
                    verdict += " by " + ";".join(sorted(set(f.split()[3] for f in failed)))[:300]
        finally:
            run(["git", "checkout", "--", path])
        results.append((name, verdict))
        print(f"MUTANT {name}: {verdict}", flush=True)
    print("SUMMARY")
    for name, v in results:
        print(f"  {name}: {v}")
    rc = run(["bash", ".scratch/a1i/build.sh", "rel"] + "hven_sqp_tests hven_ipqp_seam_tests".split())
    print(f"RESTORED_BUILD rc={rc}")


if __name__ == "__main__":
    main()
