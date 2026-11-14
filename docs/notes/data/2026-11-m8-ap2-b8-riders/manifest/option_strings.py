#!/usr/bin/env python3
"""AP-2 B8 riders: re-spell renamed OPTION FIELDS inside string literals (dump_options keys, validation
messages and the tests that pin them). Only literals; only these files' literals; trace keys are never in
the list (init_mu / min_mu are trace keys too and are re-spelled by hand where they name the option).
usage: option_strings.py <file>..."""
import re, sys
DISTINCT = ['pivot_perturbation', 'refinement_steps', 'parallel_solve', 'accelerate_zero_tolerance',
            'dense_max_dimension', 'primal_dual_step_strategy', 'hard_iteration_cap', 'rho_init', 'delta_init',
            'reg_floor', 'reg_max', 'reg_decrease', 'face_kappa', 'converge_slack', 'warm_iter_budget',
            'mu_adopt_factor', 'retire_after', 'hoist_symbolic', 'mu0_rule', 'mu_residual_floor', 'enable_soc',
            'adaptive_mu', 'ssn_prox_carry', 'qp_failure_budget', 'qp_cap_retry', 'qn_update_on_reject',
            'qn_nonlinear_only', 'qn_full_memory_max', 'pivot_perturb_exp', 'max_refinement_iters', 'cnr_threads']
NEW = dict(l.rstrip('\n').split('\t')[2:4] for l in open(__file__.rsplit('/', 1)[0] + '/member_manifest.tsv')
           if l.strip() and not l.startswith('#'))
PREFIXED = {'linear.pivoting': 'linear.pivot_strategy', 'linear.matching': 'linear.weighted_matching',
            'linear.scaling': 'linear.matrix_scaling', 'ipqp.tau': 'ipqp.fraction_to_boundary',
            'ipqp.ruiz': 'ipqp.ruiz_equilibration', 'ipqp.init_mu': 'ipqp.barrier_initial',
            'ipqp.min_mu': 'ipqp.barrier_min'}
rules = [(re.compile(r'(?<![\w])' + re.escape(o) + r'(?![\w])'), NEW[o]) for o in DISTINCT]
rules += [(re.compile(r'(?<![\w])' + re.escape(o) + r'(?![\w])'), n) for o, n in PREFIXED.items()]
LIT = re.compile(r'"(?:\\.|[^"\\\n])*"')
for p in sys.argv[1:]:
    s = open(p).read()
    def sub(m):
        t = m.group(0)
        for rx, n in rules:
            t = rx.sub(n, t)
        return t
    # skip comments: blank them out to find literal spans only in code
    out, pos = [], 0
    for m in re.finditer(r'R"([^(\s]{0,16})\((?:.|\n)*?\)\1"|"(?:\\.|[^"\\\n])*"|//[^\n]*|/\*(?:.|\n)*?\*/', s):
        out.append(s[pos:m.start()])
        tok = m.group(0)
        out.append(sub(m) if tok.startswith('"') or tok.startswith('R"') else tok)
        pos = m.end()
    out.append(s[pos:])
    t = ''.join(out)
    if t != s:
        open(p, 'w').write(t)
        print(p)
