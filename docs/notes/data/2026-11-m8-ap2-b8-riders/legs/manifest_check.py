#!/usr/bin/env python3
"""AP-2 B8 riders: the manifest's coverage check (DECLARATION P14), B8's extended to the riders. Every OLD
spelling of B8's text manifest and of the riders' ident manifest (types, tokens, qualified enumerators,
header paths), every riders' member rename in its QUALIFIED forms (`Struct::old`, and an option's dump path
`linear.old` / `advanced.old` / `advanced.ipqp.old`), and the retired namespaces occur NOWHERE in the tracked
tree -- code AND comment text -- outside string literals of C++ files (a bench's printed labels and a
golden's mode names are frozen output, counted and listed separately, not hits) and outside the allowed
residual: the frozen evidence and history (docs/notes/, docs/migration/,
docs/superpowers/, docs/dockets/, docs/audit/, the retarget/consumed-surface design records,
bench/baselines/), and the two P-SYM instruments whose comments and patterns cite historical symbols.
Prints every hit outside it; exit 1 if any.  usage: manifest_check.py"""
import os, re, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(here, '../../../../..'))
man = os.path.join(here, '..', '..', '2026-11-m8-ap2-b8', 'manifest', 'text_manifest.tsv')  # B8's, read
ALLOWED_PREFIX = ('docs/notes/', 'docs/migration/', 'docs/superpowers/', 'docs/dockets/', 'docs/audit/',
                  'docs/retarget-design', 'docs/consumed-surface-audit.md', 'bench/baselines/', 'notices/',
                  'scripts/psym_compare.sh', 'scripts/psym_bin_pairs.awk', 'dep/', 'CHANGELOG.md',
                  'tests/golden_rig/seam_psiopt.cpp', 'tests/golden_rig/seam_sqp.cpp',
                  'tests/golden_rig/expected/', 'tests/qp/fixtures/',
                  # the MPL-derived session files stay a clean diff against their upstream (CLAUDE.md section 6);
                  # their comments name the adapter's option fields as spelled when they were derived
                  'include/hven/detail/linear/pardiso_session.h', 'include/hven/detail/linear/accelerate_session.h',
                  'src/linear/pardiso_session.cpp', 'src/linear/accelerate_session.cpp')  # the last two: frozen pins/fixtures
pats = []
for line in open(man):
    if line.startswith('#') or not line.strip():
        continue
    f = line.rstrip('\n').split('\t')
    if f[0] in ('type', 'token'):
        pats.append((f[0] + ' ' + f[1], re.compile(r'(?<![\w])' + re.escape(f[1]) + r'(?![\w])')))
    elif f[0] == 'enumerator':
        enum = f[1].split('::')[-1]
        pats.append((f'enumerator {enum}::{f[2]}', re.compile(r'\b' + enum + r'::' + f[2] + r'(?![\w])')))
    elif f[0] == 'header':
        pats.append((f'header {f[1]}', re.compile(r'(?<![\w/])' + re.escape(f[1]) + r'(?![\w])')))
rman = os.path.join(here, '..', 'manifest')
for line in open(os.path.join(rman, 'ident_manifest.tsv')):
    if line.startswith('#') or not line.strip():
        continue
    f = line.rstrip('\n').split('\t')
    if f[0] in ('type', 'token', 'qualified'):
        pats.append((f[0] + ' ' + f[1], re.compile(r'(?<![\w])' + re.escape(f[1]) + r'(?![\w])')))
    elif f[0] == 'header':
        pats.append((f'header {f[1]}', re.compile(r'(?<![\w/])' + re.escape(f[1]) + r'(?![\w])')))
PREFIX = {'IpmLinearOptions': 'linear.', 'SqpLinearOptions': 'linear.', 'IpmAdvancedOptions': 'advanced.',
          'SqpAdvancedOptions': 'advanced.', 'IpqpOptions': 'advanced.ipqp.'}
for line in open(os.path.join(rman, 'member_manifest.tsv')):
    if line.startswith('#') or not line.strip():
        continue
    h, st, o, n = line.rstrip('\n').split('\t')
    pats.append((f'member {st}::{o}', re.compile(r'\b' + re.escape(st) + r'::' + re.escape(o) + r'(?![\w])')))
    if st in PREFIX:
        pats.append((f'dump key {PREFIX[st]}{o}', re.compile(r'(?<![\w.])' + re.escape(PREFIX[st] + o) + r'(?![\w])')))
pats.append(('linear::FactorKind', re.compile(r'\bFactorKind\b')))
pats.append(('namespace hven::solvers', re.compile(r'\bhven::solvers\b|namespace solvers\b')))
pats.append(('namespace hven::utils', re.compile(r'\bhven::utils\b|namespace utils\b')))
files = subprocess.run(['git', '-C', WT, 'ls-files'], capture_output=True, text=True).stdout.split()
LIT = re.compile(r'"(?:\\.|[^"\\\n])*"')
CPP = ('.h', '.hpp', '.cpp', '.cc', '.in')
hits = lits = 0
for p in files:
    if p.startswith(ALLOWED_PREFIX) or p.endswith(('.png', '.pdf')):
        continue
    try:
        s = open(os.path.join(WT, p), errors='ignore').read()
    except (IsADirectoryError, FileNotFoundError):
        continue
    spans = [m.span() for m in LIT.finditer(s)] if p.endswith(CPP) else []
    for name, rx in pats:
        for m in rx.finditer(s):
            ln = s.count('\n', 0, m.start()) + 1
            if any(a <= m.start() < b for a, b in spans):
                print(f'LITERAL {name}: {p}:{ln}')
                lits += 1
                continue
            print(f'HIT {name}: {p}:{ln}')
            hits += 1
print(f'patterns {len(pats)} files {len(files)} hits {hits} (inside C++ string literals, not hits: {lits})')
sys.exit(1 if hits else 0)
