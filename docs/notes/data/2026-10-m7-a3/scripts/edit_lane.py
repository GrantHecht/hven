p = '/home/ghecht/Projects/hven/.claude/worktrees/agent-afecbe0a5611f9eff/.scratch/a3/bench/lanes/hven/hven_bench_cell.cpp'
s = open(p).read()
old = '''    } else if (label == "sqp-ipm") {
        o.qp_mode = hven::solvers::QpMode::kIpm;
'''
assert s.count(old) == 1
s = s.replace(old, old + '''    } else if (label == "sqp-ipm-slack1") {
        // hven M7 W2 A3 (scratch clone only, never pushed): `sqp-ipm` plus ONE field each
        // (docs/notes/2026-10-m7-w2-a3-ipqp-levers-design.md section 5.3).
        o.qp_mode = hven::solvers::QpMode::kIpm;
        o.ipqp.ipqp_converge_slack = 1.0;
    } else if (label == "sqp-ipm-slack1e3") {
        o.qp_mode = hven::solvers::QpMode::kIpm;
        o.ipqp.ipqp_converge_slack = 1000.0;
    } else if (label == "sqp-ipm-skip1") {
        o.qp_mode = hven::solvers::QpMode::kIpm;
        o.ipqp.ipqp_ladder_skip_after = 1;
    } else if (label == "sqp-ipm-skipnever") {
        o.qp_mode = hven::solvers::QpMode::kIpm;
        o.ipqp.ipqp_ladder_skip_after = 1000000;
''')
open(p, 'w').write(s)
print("ok")
