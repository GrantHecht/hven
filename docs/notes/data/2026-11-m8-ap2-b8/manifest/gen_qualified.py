#!/usr/bin/env python3
import os
d = '/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4/build-b8/tools/'
out = open(d + 'qualified.tsv', 'w')
for line in open(d + 'member_manifest.tsv'):
    if line.startswith('#') or not line.strip():
        continue
    s, o, n = line.rstrip('\n').split('\t')
    out.write(f"re:\\b{s}(::|\\.){o}(?![\\w])\t{s}\\1{n}\n")
out.close()
q = open(d + 'qp_messages.tsv', 'w')
for o, n in (('tr_radius', 'trust_region_radius'), ('primal_delta', 'primal_regularization'),
             ('dual_mu', 'dual_regularization'), ('max_iter', 'max_iterations'),
             ('feas_tol', 'feasibility_tol'), ('opt_tol', 'optimality_tol')):
    q.write(f"re:\\b(opts_|opts|options|overrides|QpOptions|SolveOverrides)(::|\\.){o}(?![\\w])\t\\1\\2{n}\n")
q.close()
print('ok')
