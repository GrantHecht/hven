p = '/home/ghecht/Projects/hven/.claude/worktrees/w3/.scratch/w3/bench/lanes/hven/hven_bench_cell.cpp'
s = open(p).read()
old = '''    if (label == "sqp-ssn") {
        o.qp_mode = hven::solvers::QpMode::kSsn;
'''
assert s.count(old) == 1
s = s.replace(old, old + '''    } else if (label == "sqp-ssn-sigma-armed") {
        // hven M7 W3 (scratch clone only, never pushed): `sqp-ssn` plus ONE field each
        // (docs/notes/2026-10-m7-w3-ratification-design.md section 5.5).
        o.qp_mode = hven::solvers::QpMode::kSsn;
        o.ssn_sigma_rule = hven::solvers::SsnSigmaRule::kResidualArmed;
    } else if (label == "sqp-ssn-farkas") {
        o.qp_mode = hven::solvers::QpMode::kSsn;
        o.ssn_infeasibility_rule = hven::solvers::SsnInfeasibilityRule::kFarkasGated;
    } else if (label == "sqp-ssn-certify-face") {
        o.qp_mode = hven::solvers::QpMode::kSsn;
        o.ssn_certify_from_face = true;
''')
open(p, 'w').write(s)
print("ok")
