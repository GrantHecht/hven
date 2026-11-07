#!/usr/bin/env python3
"""AP-2 B8 riders: emit the COMPLETE old -> new table of B8 and its riders (Markdown) from the two groups'
manifests, composed: a name B8 renamed and a rider renamed again is listed once, oldest -> final. The
naming note's section 3 is this output. usage: gen_tables.py > tables.md"""
import os, re
here = os.path.dirname(os.path.abspath(__file__))
B8 = os.path.join(here, '..', '..', '2026-11-m8-ap2-b8', 'manifest')
def rows(p):
    return [l.rstrip('\n').split('\t') for l in open(p) if l.strip() and not l.startswith('#')]
r_types = {f[1]: f[2] for f in rows(os.path.join(here, 'ident_manifest.tsv')) if f[0] == 'type'}
r_tokens = [(f[1], f[2]) for f in rows(os.path.join(here, 'ident_manifest.tsv')) if f[0] in ('token', 'qualified')]
r_headers = [(f[1], f[2]) for f in rows(os.path.join(here, 'ident_manifest.tsv')) if f[0] == 'header']
r_members = rows(os.path.join(here, 'member_manifest.tsv'))
r_mem = {(f[1], f[2]): f[3] for f in r_members}
def T(n):  # final spelling of a type name
    return r_types.get(n, n)
b8_types, b8_tokens, b8_enums, b8_headers = [], [], [], []
for f in rows(os.path.join(B8, 'text_manifest.tsv')):
    if f[0] == 'type': b8_types.append((f[1], f[2]))
    elif f[0] == 'token': b8_tokens.append((f[1], f[2]))
    elif f[0] == 'enumerator': b8_enums.append((f[1], f[2].replace('([0-9]+)', 'N'), f[3].replace('\\1', 'N')))
    elif f[0] == 'header': b8_headers.append((f[1], f[2]))
b8_members = rows(os.path.join(B8, 'member_manifest.tsv'))
token_enum = {'kAugLang': 'LineSearchMode', 'kLang': 'LineSearchMode', 'kNoLs': 'LineSearchMode',
              'kPrimSlackEqSplitIq': 'PrimalDualStepStrategy', 'kPrimSlackSplitEqIq': 'PrimalDualStepStrategy',
              'kMaxEq': 'PrimalDualStepStrategy', 'kWmno': 'MeritPenaltyRule', 'kLDLT': 'FactorKind',
              'kLLT': 'FactorKind', 'kLU': 'FactorKind'}
P = print
P('### Namespaces\n\n| old | new |\n|---|---|')
for o, n in (('hven::solvers::X', 'hven::X (every public name)'), ('hven::solvers::detail::X', 'hven::detail::X'),
             ('hven::solvers::detail::testing::X', 'hven::detail::testing::X'),
             ('hven::utils::TypeStorage, hven::utils::TypeStorageBase', 'hven::TypeStorage, hven::TypeStorageBase'),
             ('hven::utils::X (internal utilities), hven::utils::detail::X', 'hven::detail::X'),
             ('internal types and functions declared in `detail/` headers (were hven::solvers::X, then hven::X)',
              'hven::detail::X (the list: B8 riders manifest/detail_moved_names.txt)'),
             ('hven::GlobalizationStrategy (named by `SqpAdvancedOptions::make_strategy`)', 'hven::detail::GlobalizationStrategy'),
             ('hven::linear, hven::linear::detail', 'unchanged')):
    P(f'| `{o}` | `{n}` |' if '`' not in o else f'| {o} | `{n}` |')
P('\n### Headers\n\n| old | new |\n|---|---|')
hm = dict(r_headers)
for o, n in b8_headers:
    P(f'| `<{o}>` | `<{hm.get(n, n)}>` |')
for o, n in r_headers:
    if 'detail/' not in o:
        P(f'| `<{o}>` | `<{n}>` |')
P('| `<hven/model/pieces.h>` | deleted; `<hven/hven.h>` includes every public header |')
P('| (none) | `<hven/hven.h>`, the umbrella: every public header |')
P('\n### Types and constants\n\n| old | new |\n|---|---|')
seen = set()
for o, n in b8_types:
    P(f'| `{o}` | `{T(n)}` |'); seen.add(n)
P('| `AlgorithmMode` (public, `IpmAdvancedOptions::solve_phase_mode`) | `IpmSolvePhaseMode`; `AlgorithmMode` is internal |')
for o, n in r_types.items():
    if o not in seen and o != 'SolvePhaseMode':
        P(f'| `{o}` | `{n}` |')
P('| `linear::FactorKind` | `linear::Decomposition` (only `kLdlt`; `kLlt`/`kLu`, refused by both backends, removed) |')
P('| `HVEN_DEFAULT_QP_THREADS` (macro) | `HVEN_DEFAULT_IPM_THREADS` (with an `#ifndef` fallback of 8) |')
P('\n### Enumerators\n\n| enum (final name) | old | new |\n|---|---|---|')
P('| `IpmSolvePhaseMode` (was `AlgorithmMode`) | `kOpt`, `kOptNo`, `kSoe` | `kOptimize`, `kOptimizeWithoutObjective`, `kSolveEquations` |')
P('| `IpmSolvePhaseMode` (was `AlgorithmMode`) | `kInit` | removed from the public enum (it is never a phase mode) |')
for o, n in b8_tokens:
    e = token_enum[o]
    if e == 'FactorKind':
        P(f'| `linear::Decomposition` (was `FactorKind`) | `{o}` | `{n}`' + (' |' if o == 'kLDLT' else ' -- removed by the riders |'))
    else:
        P(f'| `{T(e)}` (was `{e}`) | `{o}` | `{n}` |')
for e, o, n in b8_enums:
    P(f'| `{T(e)}` | `{o}` | `{n}` |')
owner = {'kMaxIter': '`SolveStatus`, `QpStatus`, `RestorationSubStatus`', 'kIterationCap': '`IpmStopReason`',
         'kAcceptF': '`StepVerdict`', 'kAcceptH': '`StepVerdict`'}
for o, n in r_tokens:
    if o.startswith('QpMode::') or o.startswith('KktSolveParallelism::'):
        e, a = o.split('::'); P(f'| `{T(e)}` | `{a}` | `{n.split("::")[1]}` (the trace string is unchanged) |' if e == 'QpMode' else f'| `{T(e)}` | `{a}` | `{n.split("::")[1]}` |')
    elif o in owner:
        P(f'| {owner[o]} | `{o}` | `{n}` (`to_string` keeps the frozen string) |')
P('\n### Members, functions and option fields\n\n| struct (final name) | old | new |\n|---|---|---|')
for s, o, n in b8_members:
    fin = r_mem.get((s, n), n)
    P(f'| `{T(s)}` | `{o}` | `{fin}` |')
for h, s, o, n in r_members:
    if (s, o) in {(T(a), c) for a, b, c in b8_members} or any(T(a) == s and c == o for a, b, c in b8_members):
        continue
    sn = {'Options': 'linear::SymmetricFactor::Options', 'InertiaEvidence': 'linear::InertiaEvidence',
          'SolveInfo': 'linear::SolveInfo', 'SymmetricFactor': 'linear::SymmetricFactor',
          'DenseSymmetricFactor': 'linear::DenseSymmetricFactor'}.get(s, s)
    if n.endswith('_') and o.endswith('_'):
        continue
    fn = '()' if s in ('SymmetricFactor', 'DenseSymmetricFactor', 'Problem', 'SqpSolver') or o == 'misc' else ''
    P(f'| `{sn}` | `{o}{fn}` | `{n}{fn}` |')
P('| (free function) | `serialize(const WarmStart &)` | `serialize_warm_start(const WarmStart &)` |')
P('| (free function) | `deserialize(std::span<const std::byte>)` | `deserialize_warm_start(std::span<const std::byte>)` |')
P('| `IpmLinearOptions` | `accelerate_zero_tol` declared only under `USE_ACCELERATE_SPARSE` | declared on every platform (validated everywhere, ignored on MKL) |')
P('| `InteriorPointSolverPresetFields` (internal) | `acceptance_strategy_` | `acceptance_mode_` |')
P('\n### Option dump keys (`dump_options`) and validation messages\n')
P('Every renamed option field above prints and is refused under its new name: the dump key is the field path')
P('(`trust_region_initial`, `qp.feasibility_tol`, `advanced.max_restoration_entries`, `advanced.ipqp.barrier_initial`,')
P('`linear.pivot_strategy`, ...). Enumerator values print as the new enumerator names (`kAugmentedLagrangian`,')
P('`kSolveEquations`, `kSinglePenalty`, `kEqualityMultipliersPrimal`, `kIpqp`, `kAdaptivePartitioning`, ...).')
P('`IpmOptions`\' dump has `linear.accelerate_zero_tol` on every platform (68 lines).')
