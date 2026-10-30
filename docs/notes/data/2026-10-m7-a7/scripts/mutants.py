# A7 mutation run (design page G4). Each mutant: apply one textual edit, build hven_sqp_tests
# (Release, under the box lock via build.sh), run WsAlgebraRule* (under the lock via leg.sh),
# record KILLED/SURVIVED, restore the file byte for byte. Run from the worktree root.
import subprocess
import sys

W = "/home/ghecht/Projects/hven/.claude/worktrees/agent-a05b3c395d8efca12"
S = W + "/.scratch/a7"
DRV = "src/drivers/sqp_solver.cpp"
HDR = "include/hven/detail/drivers/ws_algebra_rule.h"
OPT = "src/drivers/sqp_options.cpp"

MUTANTS = [
    ("M1 strict > becomes >=", HDR, "return border_facts > nominal_facts;",
     "return border_facts >= nominal_facts;"),
    ("M2 the +1 of the nominal dropped", HDR, "return c.minor_iters + 1;", "return c.minor_iters;"),
    ("M3 no accumulation before the switch", DRV,
     "    ws_algebra_border_facts_ += qp.factorizations;\n", "\n"),
    ("M4 no reset at solve entry", DRV, "    reset_ws_algebra_rule();\n    SqpResult out", "    SqpResult out"),
    ("M5 set_options leaves the switch", DRV,
     "    ws_algebra_other_engine_.reset();\n    ws_algebra_switched_ = false;\n", "\n"),
    ("M6 inert-on-refactorize test removed (before)", DRV,
     "    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost ||\n"
     "        opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder || ws_algebra_switched_ ||",
     "    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost || ws_algebra_switched_ ||"),
    ("M7 the twin built on the border algebra", DRV,
     "        q.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;\n", "\n"),
    ("M8 switches not counted", DRV, "    ++counters.ws_algebra_switches;\n", "\n"),
    ("M9 switched subproblems not counted", DRV, "        ++counters.ws_algebra_switched_qps;\n", "\n"),
    ("M10 restoration fold dropped", DRV,
     "        st.out.counters.ws_algebra_switches += phase.counters.ws_algebra_switches;\n"
     "        st.out.counters.ws_algebra_switched_qps += phase.counters.ws_algebra_switched_qps;\n", "\n"),
    ("M11 ledger sequence not carried at the switch", DRV,
     "    // The ledger's record labels are a per-driver sequence: carried across the swap.\n"
     "    ws_algebra_other_engine_->adopt_solve_counter(engine_->solve_counter());\n", "\n"),
    ("M12 twin not attached to the ledger", DRV,
     "            twin->attach_ledger(ledger_, label_prefix_ + \"_qp\");\n", "\n"),
    ("M13 the sentinel accepted", OPT,
     "    if (opts.ws_algebra_rule == SqpWsAlgebraRule::kSqpWsAlgebraRuleCount) {",
     "    if (false && opts.ws_algebra_rule == SqpWsAlgebraRule::kSqpWsAlgebraRuleCount) {"),
    ("M14 dump line dropped", OPT, "    kv_str(os, \"ws_algebra_rule\", enum_name(o.ws_algebra_rule));\n", "\n"),
    ("M15 the rule always on", DRV,
     "    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost ||\n"
     "        opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder || ws_algebra_switched_ ||",
     "    if (opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder || ws_algebra_switched_ ||"),
    ("M6b inert-on-refactorize test removed (after)", DRV,
     "    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost ||\n"
     "        opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder) {\n        return;\n    }\n"
     "    if (ws_algebra_switched_) {",
     "    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost) {\n        return;\n    }\n"
     "    if (ws_algebra_switched_) {"),
    ("M15b the rule always on (after)", DRV,
     "    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost ||\n"
     "        opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder) {\n        return;\n    }\n"
     "    if (ws_algebra_switched_) {",
     "    if (opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder) {\n        return;\n    }\n"
     "    if (ws_algebra_switched_) {"),
    ("M6c both inert-on-refactorize tests removed", DRV,
     ["    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost ||\n"
      "        opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder || ws_algebra_switched_ ||",
      "    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost ||\n"
      "        opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder) {\n        return;\n    }\n"
      "    if (ws_algebra_switched_) {"],
     ["    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost || ws_algebra_switched_ ||",
      "    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost) {\n        return;\n    }\n"
      "    if (ws_algebra_switched_) {"]),
    ("M15c the rule always on (both gates)", DRV,
     ["    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost ||\n"
      "        opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder || ws_algebra_switched_ ||",
      "    if (opts_.ws_algebra_rule != SqpWsAlgebraRule::kMeasuredCost ||\n"
      "        opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder) {\n        return;\n    }\n"
      "    if (ws_algebra_switched_) {"],
     ["    if (opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder || ws_algebra_switched_ ||",
      "    if (opts_.qp.ws_algebra != WorkingSetLinearAlgebra::kSchurBorder) {\n        return;\n    }\n"
      "    if (ws_algebra_switched_) {"]),
    ("M16 the reset does not swap back", DRV,
     "        std::swap(engine_, ws_algebra_other_engine_);\n        ws_algebra_switched_ = false;\n    }\n"
     "    ws_algebra_border_facts_ = 0;",
     "        ws_algebra_switched_ = false;\n    }\n    ws_algebra_border_facts_ = 0;"),
]


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def main():
    only = set(sys.argv[1:])
    results = []
    for name, path, old, new in MUTANTS:
        if only and name.split()[0] not in only:
            continue
        full = W + "/" + path
        orig = open(full).read()
        olds = old if isinstance(old, list) else [old]
        news = new if isinstance(new, list) else [new]
        mutated = orig
        bad = [o for o in olds if mutated.count(o) != 1]
        if bad:
            results.append("{}: NOT APPLICABLE (pattern count {})".format(name, mutated.count(bad[0])))
            continue
        for o, n in zip(olds, news):
            mutated = mutated.replace(o, n)
        open(full, "w").write(mutated)
        try:
            tag = name.split()[0]
            p = run(["pgrep", "-a", "hven_bench_cell"])
            b = run(["bash", S + "/build.sh", "rel", "hven_sqp_tests"])
            if b.returncode != 0:
                verdict = "KILLED (does not compile)"
            else:
                t = run(["bash", S + "/leg.sh", "mutant-" + tag,
                         S + "/build-rel/tests/sqp/hven_sqp_tests", "--gtest_filter=WsAlgebraRule*"])
                log = open(S + "/logs/mutant-" + tag + ".log").read()
                failed = [l.split("]", 1)[1].strip() for l in log.splitlines()
                          if l.startswith("[  FAILED  ]") and "listed below" not in l
                          and "FAILED TESTS" not in l]
                failed = sorted(set(f.split(" (")[0] for f in failed))
                verdict = ("KILLED by " + ", ".join(failed)) if t.returncode != 0 else "SURVIVED"
            results.append("{}: {}".format(name, verdict))
        finally:
            open(full, "w").write(orig)
        print(results[-1], flush=True)
    # Restore the build to the unmutated tree.
    run(["bash", S + "/build.sh", "rel", "hven_sqp_tests"])
    open(S + "/logs/mutants-summary.txt", "a").write("\n".join(results) + "\n")


if __name__ == "__main__":
    main()
