#!/usr/bin/env python3
"""RF-1c: re-create the swept pilot harness's DIAGQP stderr line (the original patch was swept with
/home/ghecht/Projects/hven/.scratch/tu1-diag/). usage: diagqp_patch.py <hven repo> <hven_bench_cell.cpp>
After the solve, for every ledger QP record whose status is not kOptimal, one stderr line (format read off the
r3/r4 pilot cells' .err files):  DIAGQP #<1-based record index> label=<label> status=<int> then every nonzero
Index field of QpCounters, in struct order, as name=value. Observation only: nothing it reads is written back."""
import re, sys
hdr = open(sys.argv[1] + "/include/hven/core/solver_counters.h").read()
i = hdr.index("struct QpCounters {"); j = hdr.index("\n};", i)
fields = [n for t, n in re.findall(r"^\s+([A-Za-z_:<>0-9]+)\s+([a-z_0-9]+)\s*(?:=\s*[^;]*)?;", hdr[i:j], re.M) if t == "Index"]
code = ["    {",
        "        long long diag_idx = 0;",
        "        for (const auto &rec : ledger.records()) {",
        "            ++diag_idx;",
        "            if (rec.status == hven::QpStatus::kOptimal) {",
        "                continue;",
        "            }",
        "            std::fprintf(stderr, \"DIAGQP #%lld label=%s status=%d\", diag_idx, rec.label.c_str(),",
        "                         static_cast<int>(rec.status));",
        "            const auto &dq = rec.counters;"]
for n in fields:
    code.append(f'            if (dq.{n} != 0) {{ std::fprintf(stderr, " {n}=%lld", static_cast<long long>(dq.{n})); }}')
code += ['            std::fprintf(stderr, "\\n");', "        }", "    }"]
p = sys.argv[2]; s = open(p).read()
anchor = "    const auto sqp_result = solve_shared(solver, problem, source.x0(), budget, *model);\n"
assert s.count(anchor) == 1
open(p, "w").write(s.replace(anchor, anchor + "\n".join(code) + "\n"))
print("fields", len(fields))
