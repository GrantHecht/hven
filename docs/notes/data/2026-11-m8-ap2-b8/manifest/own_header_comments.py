#!/usr/bin/env python3
"""In each renamed struct's OWN header (and the doc that specifies it), rewrite bare old member names
inside comments. usage: own_header_comments.py"""
import os, re, subprocess, sys
WT = '/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4'
os.chdir(WT)
owners = {
    'IpmIterate': ['include/hven/drivers/ipm_iterate.h', 'include/hven/drivers/trace.h', 'docs/trace-schema-v0.md'],
    'VariableBound': ['include/hven/model/assembly_declaration.h', 'docs/assembly-protocol.md'],
    'AssemblyDeclaration': ['include/hven/model/assembly_declaration.h', 'docs/assembly-protocol.md',
                            'include/hven/model/structure_identity.h'],
    'CandidatePoint': ['include/hven/model/candidate_point.h'],
    'CandidateValues': ['include/hven/model/candidate_point.h'],
    'CandidateFirstOrder': ['include/hven/model/candidate_point.h'],
    'ClaimBlock': ['include/hven/model/claim_stream_source.h'],
    'SolverFunctionBase': ['include/hven/model/solver_function_base.h'],
    'ConstraintInterface': ['include/hven/model/piece_interfaces.h'],
    'ObjectiveInterface': ['include/hven/model/piece_interfaces.h'],
    'DeclarationKey': ['include/hven/model/structure_identity.h'],
    'IdentityProbe': ['include/hven/model/structure_identity.h'],
    'ModelStructureKey': ['include/hven/model/structure_identity.h'],
    'IpmPolishData': ['include/hven/warmstart/ipm_polish_extension.h'],
    'KktClaimSpace': ['include/hven/model/claim_space.h'],
    'KktScatterView': ['include/hven/model/claim_space.h'],
    'RhsArenaView': ['include/hven/model/claim_space.h'],
    'RhsClaimSpace': ['include/hven/model/claim_space.h'],
    'RhsScatterView': ['include/hven/model/claim_space.h'],
    'SolverIndexingData': ['include/hven/model/solver_indexing_data.h', 'docs/assembly-protocol.md'],
    'NlpRowClassification': ['include/hven/detail/model/nlp_problem_model.h'],
}
man = {}
for line in open('build-b8/tools/member_manifest.tsv'):
    if line.startswith('#') or not line.strip():
        continue
    s, o, n = line.rstrip('\n').split('\t')
    if o.endswith('_'):
        man.setdefault(s, {})[o] = n
byfile = {}
for s, files in owners.items():
    for f in files:
        byfile.setdefault(f, {}).update(man.get(s, {}))
for f, m in byfile.items():
    tsv = f'build-b8/tools/_own_{os.path.basename(f)}.tsv'
    with open(tsv, 'w') as out:
        for o, n in m.items():
            out.write(f'{o}\t{n}\n')
    if f.endswith('.md'):
        subprocess.run([sys.executable, 'build-b8/tools/docsub.py', tsv, '--', f])
    else:
        subprocess.run([sys.executable, 'build-b8/tools/region_sub.py', 'comments', tsv, f])
