#!/usr/bin/env python3
"""I17 r2: apply (or restore) one declared mutation. usage: mutate_i17_r2.py <id> | restore | check.
`check` verifies every anchor occurs exactly once in the tree without writing. Ids and dispositions: ../DECLARATION-I17-r2.md section 5.
mutate_i17.py's seventeen, re-anchored on the r2 code, plus the rider's (I1a I1b I2 I4a I4b I5a I5b I5c I6)."""
import subprocess
import sys

S = "src/sqp/sqp_solver.cpp"
W = "src/qp/qp_dump_writer.cpp"
WE = "src/qp/walk/walk_engine.cpp"
H = "include/hven/detail/sqp/sqp_solver_impl.h"
C = "src/trace/console_trace_sink.cpp"
J = "src/trace/json_lines_trace_sink.cpp"
FILES = [S, W, WE, H, C, J]

MUT = {
    # item 1: ladder diagnostics
    "L1": (S, "            mj.row.qp_status = main_qp_status;", "            mj.row.qp_status = report.qp_status;"),
    "L2": (S, "            : mj.qs.status == QpStatus::kInfeasible ? ElasticEntry::kVerifiedInfeasible",
              "            : mj.qs.status == QpStatus::kInfeasible ? ElasticEntry::kWorkingSetTermination"),
    "L3": (S, "    report.rho_final = rho;", "    report.rho_final = rho_0;"),
    "L4": (S, "        lr.exhausted = !usable;", "        lr.exhausted = false;"),
    "L5": (C, '                return "verified infeasible in trust region";', '                return "verified infeasible";'),
    "L6": (J, '    key_enum(o, lf, "main_status", to_json(l.main_status));', '    key_enum(o, lf, "main_status", to_json(l.rung_status));'),
    "L7": (S, "            sub.attach_ledger(ledger_, fmt::format(", "            if (false) sub.attach_ledger(ledger_, fmt::format("),
    # item 2: caller-unit step limit
    "S1": (S, "    if (opts_.caller_step_limit <= 0.0) {\n        st.step_limit_sides.clear();",
              "    if (true) {\n        st.step_limit_sides.clear();"),
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
    "D3": (S, "            engine.set_dump_site(fmt::format(\"rung{}\", rungs_climbed));", "            engine.set_dump_site(\"main\");"),
    "D4": (WE, "    if (qp_dump_wanted(dump_, status)) [[unlikely]] {", "    if (false && qp_dump_wanted(dump_, status)) [[unlikely]] {"),
    # rider ruling 1: the certified fallback's ladder row
    "I1a": (S, "    if (mj.fallback_report && mj.qs.status != QpStatus::kInfeasible) {", "    if (false) {"),
    "I1b": (S, "        const QpStatus main_qp_status = mj.fallback_report ? mj.fallback_main_status : mj.qs.status;",
               "        const QpStatus main_qp_status = mj.qs.status;"),
    # rider ruling 2: limit pins reported free
    "I2": (S, "    release_step_limit_pins(mj.qs, st.step_limit_sides);\n    mj.row.tr_binding =", "    mj.row.tr_binding ="),
    # rider ruling 4: the limit in restoration, in the parent's column units
    "I4a": (S, "        sub.step_limit_block_ = resto_base.n();", "        sub.step_limit_block_ = 0;"),
    "I4b": (S, "        sub.step_limit_factors_ = column_units_;", "        sub.step_limit_factors_ = nullptr;"),
    # rider ruling 5: exclusive create, process and solver tokens
    "I5a": (W, "    fs::create_hard_link(tmp_path, final_path, ec);", "    fs::rename(tmp_path, final_path, ec);"),
    "I5b": (W, "std::uint64_t next_qp_dump_solver_id() noexcept { return g_solver_id.fetch_add(1) + 1; }",
               "std::uint64_t next_qp_dump_solver_id() noexcept { return 1; }"),
    "I5c": (W, "    return static_cast<long>(getpid());", "    return 0L;"),
    # rider ruling 6: unique restoration ledger labels
    "I6": (S, "                                                   st.out.counters.restoration_entries, phase_run,\n                                                   restoration_phase_runs_));",
              "                                                   st.out.counters.restoration_entries, phase_run,\n                                                   0));"),
}


def count(path, old):
    return open(path).read().count(old)


arg = sys.argv[1]
if arg == "restore":
    subprocess.check_call(["git", "checkout", "--"] + FILES)
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
