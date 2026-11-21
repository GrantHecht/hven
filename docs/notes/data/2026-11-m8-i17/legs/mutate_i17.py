#!/usr/bin/env python3
"""I17: apply (or restore) one declared mutation. usage: mutate_i17.py <id> | restore | check.
`check` verifies every anchor occurs exactly once in the tree without writing. Ids and dispositions: ../DECLARATION-I17.md section 5."""
import subprocess
import sys

S = "src/sqp/sqp_solver.cpp"
W = "src/qp/qp_dump_writer.cpp"
O = "src/sqp/sqp_options.cpp"
WE = "src/qp/walk/walk_engine.cpp"
H = "include/hven/detail/sqp/sqp_solver_impl.h"
C = "src/trace/console_trace_sink.cpp"
J = "src/trace/json_lines_trace_sink.cpp"

MUT = {
    # item 1: ladder diagnostics
    "L1": (S, "            mj.row.qp_status = main_qp_status;", "            mj.row.qp_status = report.qp_status;"),
    "L2": (S, "            : main_qp_status == QpStatus::kInfeasible ? ElasticEntry::kVerifiedInfeasible",
              "            : main_qp_status == QpStatus::kInfeasible ? ElasticEntry::kWorkingSetTermination"),
    "L3": (S, "    report.rho_final = rho;", "    report.rho_final = rho_0;"),
    "L4": (S, "        lr.exhausted = !ladder_usable;", "        lr.exhausted = false;"),
    "L5": (C, '                return "verified infeasible in trust region";', '                return "verified infeasible";'),
    "L6": (J, '        key_enum(o, lf, "main_status", to_json(l.main_status));', '        key_enum(o, lf, "main_status", to_json(l.rung_status));'),
    "L7": (S, "            sub.attach_ledger(ledger_, fmt::format(", "            if (false) sub.attach_ledger(ledger_, fmt::format("),
    # item 2: caller-unit step limit
    "S1": (S, "        if (opts_.caller_step_limit > 0.0 && !is_restoration_sub_driver_) {\n            apply_caller_step_limit(st.qp, st.x, column_units_, opts_.caller_step_limit);\n        }\n        if (!st.eqb.basis.reduce(st.qp)) {\n            ++st.out.equality_basis.lift_refusals;\n            ++st.out.equality_basis.lift_refusal_sites.qp_projection;\n            refuse_equality_basis(st.eqb, st.out.equality_basis, seam.me());\n            drop_row_coordinate_carries(st);\n        }\n        st.subproblem_is_stale = false;",
              "        if (!st.eqb.basis.reduce(st.qp)) {\n            ++st.out.equality_basis.lift_refusals;\n            ++st.out.equality_basis.lift_refusal_sites.qp_projection;\n            refuse_equality_basis(st.eqb, st.out.equality_basis, seam.me());\n            drop_row_coordinate_carries(st);\n        }\n        st.subproblem_is_stale = false;"),
    "S2": (S, "        const double lim = r * (1.0 + std::abs(x(j) * dj)) / dj;", "        const double lim = r * (1.0 + std::abs(x(j) * dj));"),
    "S3": (S, "        const double lim = r * (1.0 + std::abs(x(j) * dj)) / dj;", "        const double lim = r * (1.0 + std::abs(x(j))) / dj;"),
    "S4": (S, "        const double lo = std::max(qp.lower(j), -lim);\n        const double hi = std::min(qp.upper(j), lim);",
              "        const double lo = -lim;\n        const double hi = lim;"),
    # item 3: separate column cap
    "C1": (S, "        rule.factor_limit = column_factor_limit();", "        rule.factor_limit = opts_.scaling_factor_max;"),
    "C2": (H, "        return opts_.column_scaling == SqpColumnScaling::kAlways ? opts_.column_scaling_factor_max\n                                                                 : opts_.scaling_factor_max;",
              "        return opts_.column_scaling_factor_max;"),
    # item 4: dump
    "D1": (W, "    return cfg.enabled() && (cfg.policy == QpDumpPolicy::kAll || status != QpStatus::kOptimal);",
              "    return cfg.enabled();"),
    "D2": (W, "    put_vec(o, \"g\", qp.g);", "    put_vec(o, \"g\", Vec(-qp.g));"),
    "D3": (S, "            rung_tag.site = fmt::format(\"rung{}\", rungs_climbed);", "            rung_tag.site = \"main\";"),
    "D4": (WE, "    if (qp_dump_wanted(dump_, status)) [[unlikely]] {", "    if (false && qp_dump_wanted(dump_, status)) [[unlikely]] {"),
}


def count(path, old):
    return open(path).read().count(old)


arg = sys.argv[1]
if arg == "restore":
    subprocess.check_call(["git", "checkout", "--", S, W, O, WE, C, J, H])
    print("RESTORED")
    sys.exit(0)
if arg == "check":
    bad = [(k, count(p, o)) for k, (p, o, n) in MUT.items() if count(p, o) != 1]
    if bad:
        sys.exit(f"ANCHORS_NOT_UNIQUE {bad}")
    print(f"ALL {len(MUT)} ANCHORS UNIQUE")
    sys.exit(0)
if arg not in MUT:
    sys.exit("UNKNOWN")
p, old, new = MUT[arg]
s = open(p).read()
if s.count(old) != 1:
    sys.exit(f"MUTATION_DOES_NOT_APPLY {arg} (count {s.count(old)})")
open(p, "w").write(s.replace(old, new))
print(f"APPLIED {arg}")
