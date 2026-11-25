#!/usr/bin/env python3
"""AP-2 B8: the manifest's coverage check (DECLARATION P14). Every OLD spelling the text manifest renames
(types, tokens, qualified enumerators, header paths) and the retired namespaces occur NOWHERE in the
tracked tree outside the allowed residual: the frozen evidence and history (docs/notes/, docs/migration/,
docs/superpowers/, docs/dockets/, docs/audit/, the retarget/consumed-surface design records,
bench/baselines/), and the two P-SYM instruments whose comments and patterns cite historical symbols.
Prints every hit outside it; exit 1 if any.  usage: manifest_check.py"""
import os, re, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
WT = os.path.abspath(os.path.join(here, '../../../../..'))
man = os.path.join(here, '..', 'manifest', 'text_manifest.tsv')
ALLOWED_PREFIX = ('docs/notes/', 'docs/migration/', 'docs/superpowers/', 'docs/dockets/', 'docs/audit/',
                  'docs/retarget-design', 'docs/consumed-surface-audit.md', 'bench/baselines/', 'notices/',
                  'scripts/psym_compare.sh', 'scripts/psym_bin_pairs.awk', 'dep/', 'CHANGELOG.md')
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
pats.append(('namespace hven::solvers', re.compile(r'\bhven::solvers\b|namespace solvers\b')))
pats.append(('namespace hven::utils', re.compile(r'\bhven::utils\b|namespace utils\b')))
files = subprocess.run(['git', '-C', WT, 'ls-files'], capture_output=True, text=True).stdout.split()
hits = 0
for p in files:
    if p.startswith(ALLOWED_PREFIX) or p.endswith(('.png', '.pdf')):
        continue
    try:
        s = open(os.path.join(WT, p), errors='ignore').read()
    except (IsADirectoryError, FileNotFoundError):
        continue
    for name, rx in pats:
        for m in rx.finditer(s):
            ln = s.count('\n', 0, m.start()) + 1
            print(f'HIT {name}: {p}:{ln}')
            hits += 1
print(f'patterns {len(pats)} files {len(files)} hits {hits}')
sys.exit(1 if hits else 0)
