#!/usr/bin/env python3
"""Emit the B8 old->new tables (Markdown) from the manifests, so the doc and the applied rename are
one source. usage: gen_tables.py > tables.md"""
import os
T = '/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4/build-b8/tools/'
types, tokens, enums, headers = [], [], [], []
for line in open(T + 'text_manifest.tsv'):
    if line.startswith('#') or not line.strip():
        continue
    f = line.rstrip('\n').split('\t')
    if f[0] == 'type':
        types.append((f[1], f[2]))
    elif f[0] == 'token':
        tokens.append((f[1], f[2]))
    elif f[0] == 'enumerator':
        enums.append((f[1], f[2].replace('([0-9]+)', 'N'), f[3].replace('\\1', 'N')))
    elif f[0] == 'header':
        headers.append((f[1], f[2]))
members = []
for line in open(T + 'member_manifest.tsv'):
    if line.startswith('#') or not line.strip():
        continue
    members.append(tuple(line.rstrip('\n').split('\t')))
# which enum each token belongs to (for the reader)
token_enum = {
    'kAugLang': 'LineSearchMode', 'kLang': 'LineSearchMode', 'kNoLs': 'LineSearchMode',
    'kPrimSlackEqSplitIq': 'PrimalDualStepStrategy', 'kPrimSlackSplitEqIq': 'PrimalDualStepStrategy',
    'kMaxEq': 'PrimalDualStepStrategy', 'kWmno': 'MeritPenaltyRule', 'kLDLT': 'linear::FactorKind',
    'kLLT': 'linear::FactorKind', 'kLU': 'linear::FactorKind'}
rename_ns_enum = {'TypeStorage::Kind': 'TypeStorage::Kind', 'ParsedIoFlags': 'ParsedIoFlags (was ParsedIOFlags)',
                  'NlpRowKind': 'NlpRowKind (internal)', 'HessOwner': 'NlpAdapterCore::HessOwner (internal)'}
print('### Namespaces\n')
print('| old | new |\n|---|---|')
for o, n in (('hven::solvers::X', 'hven::X (every name)'), ('hven::solvers::detail::X', 'hven::detail::X'),
             ('hven::solvers::detail::testing::X', 'hven::detail::testing::X'),
             ('hven::utils::TypeStorage, hven::utils::TypeStorageBase', 'hven::TypeStorage, hven::TypeStorageBase'),
             ('hven::utils::X (internal utilities), hven::utils::detail::X', 'hven::detail::X'),
             ('hven::linear, hven::linear::detail', 'unchanged')):
    print(f'| `{o}` | `{n}` |')
print('\n### Headers\n')
print('| old | new |\n|---|---|')
for o, n in headers:
    print(f'| `<{o}>` | `<{n}>` |')
print('| (none) | `<hven/hven.h>`, the umbrella: every public header |')
print('\n### Types and constants\n')
print('| old | new |\n|---|---|')
for o, n in types:
    print(f'| `{o}` | `{n}` |')
print('| `AlgorithmMode` (public, `IpmAdvancedOptions::solve_phase_mode`) | `SolvePhaseMode`; `AlgorithmMode` is internal (`detail/drivers/ipm_solver_impl.h`) |')
print('\n### Enumerators\n')
print('| enum | old | new |\n|---|---|---|')
print('| `SolvePhaseMode` (was `AlgorithmMode`) | `kOpt`, `kOptNo`, `kSoe` | `kOptimize`, `kOptimizeWithoutObjective`, `kSolveEquations` |')
print('| `SolvePhaseMode` (was `AlgorithmMode`) | `kInit` | removed from the public enum (it is never a phase mode) |')
for o, n in tokens:
    print(f'| `{token_enum[o]}` | `{o}` | `{n}` |')
for e, o, n in enums:
    print(f'| `{rename_ns_enum.get(e, e)}` | `{o}` | `{n}` |')
print('\n### Members and option fields\n')
print('| struct | old | new |\n|---|---|---|')
for s, o, n in members:
    print(f'| `{s}` | `{o}` | `{n}` |')
print('\n### Option dump keys (`dump_options`) and validation messages\n')
print('Every renamed option field above prints and is refused under its new name: the dump key is the')
print('field path (`trust_region_initial`, `qp.feasibility_tol`, `advanced.max_restoration_entries`, ...).')
print('Enumerator values print as the new enumerator names (`kAugmentedLagrangian`, `kSolveEquations`,')
print('`kSinglePenalty`, `kEqualityMultipliersPrimal`, ...).')
