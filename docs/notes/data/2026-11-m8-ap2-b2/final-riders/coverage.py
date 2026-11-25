#!/usr/bin/env python3
"""Final-riders coverage argument: samples in runtime-r5's recorded perf tables
(runtime-r5/<mode>/syms/*.txt, every process, every arm, every round) for each function
the rider diff changes in src/.  Row format: '<pct>% <samples> [.] <symbol> - -'."""
import glob, re, sys
R = 'docs/notes/data/2026-11-m8-ap2-b2/runtime-r5'
LINE = re.compile(r'\s*([\d.]+)%\s+(\d+)\s+\[(.)\]\s+(.*?)(?:\s+-\s+-)?\s*$')
# symbol substrings: the demangled names in the tables
CHANGED = [
 ('ProblemState::report (kNlp arm; emitted inside ProblemAccess::report)', 'ProblemAccess::report'),
 ('ProblemState::report (as its own symbol, if emitted)', 'ProblemState::report'),
 ('ProblemState::map_row_values', 'map_row_values'),
 ('ProblemState::extra_fixed_rows (new, inline)', 'extra_fixed_rows'),
 ('ProblemState::adopt_bounds', 'adopt_bounds'),
 ('NonLinearProgram::update_variable_bound_values', 'update_variable_bound_values'),
]
modes = ['interior', 'ipm', 'ssn', 'walk']
tab = {}
tot = {}
nproc = {}
for m in modes:
    files = sorted(glob.glob(f'{R}/{m}/syms/*.txt'))
    nproc[m] = len(files)
    tot[m] = 0
    for f in files:
        for ln in open(f):
            mm = LINE.match(ln)
            if not mm:
                if ln.strip():
                    sys.exit(f'PARSE ERROR {f}: {ln[:80]!r}')
                continue
            n = int(mm.group(2)); sym = mm.group(4)
            tot[m] += n
            for label, pat in CHANGED:
                if pat in sym:
                    tab[(label, m)] = tab.get((label, m), 0) + n
print('recorded perf tables: ' + ', '.join(f'{m}: {nproc[m]} processes / {tot[m]} samples' for m in modes))
print()
print(f'{"changed function":<75}' + ''.join(f'{m:>10}' for m in modes) + f'{"total":>10}')
grand = 0
for label, pat in CHANGED:
    row = [tab.get((label, m), 0) for m in modes]
    grand += sum(row)
    print(f'{label:<75}' + ''.join(f'{v:>10}' for v in row) + f'{sum(row):>10}')
print(f'\nGRAND TOTAL samples in changed functions: {grand}')
