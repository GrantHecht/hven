#!/usr/bin/env python3
# TR-D r2: every changed golden in tests/ (adjacent C string literals concatenated) at <A> vs <B> equals map(old) byte for byte;
# the schema doc's changed lines that carry the key pair likewise. usage: goldens_rename_check_r2.py <revA> <revB> <out>
import re, subprocess, sys
A, B, OUT = sys.argv[1:4]
MAP = [('dual_regularization', 'regularization_dual')]
def m(s):
    for a, b in MAP: s = s.replace(a, b)
    return s
def show(rev, p): return subprocess.run(['git', 'show', f'{rev}:{p}'], capture_output=True, text=True, check=True).stdout
def runs(src):
    # adjacent string literals (whitespace/newline-separated) concatenated, escapes left as written
    lit = r'"(?:[^"\\\n]|\\.)*"'
    out = []
    for mt in re.finditer(rf'{lit}(?:\s*{lit})*', src):
        out.append(''.join(x[1:-1] for x in re.findall(lit, mt.group(0))))
    return out
files = subprocess.run(['git', 'diff', '--name-only', A, B, '--', 'tests'], capture_output=True, text=True, check=True).stdout.split()
res, fail, changed = [], 0, 0
for f in files:
    ra, rb = runs(show(A, f)), runs(show(B, f))
    if len(ra) != len(rb):
        res.append(f'FAIL {f}: literal run count {len(ra)} vs {len(rb)}'); fail += 1; continue
    for i, (x, y) in enumerate(zip(ra, rb)):
        if x != y:
            changed += 1
            ok = m(x) == y
            res.append(('OK   ' if ok else 'FAIL ') + f'{f} run#{i}: {x[:70]}')
            fail += 0 if ok else 1
        elif 'dual_regularization' in y:
            res.append(f'FAIL {f} run#{i}: unchanged literal still carries old key'); fail += 1
# schema doc: removed/added lines carrying either key
d = subprocess.run(['git', 'diff', '-U0', A, B, '--', 'docs/trace-schema-v1.md'], capture_output=True, text=True, check=True).stdout.split('\n')
rm = [l[1:] for l in d if l.startswith('-') and not l.startswith('---')]
ad = [l[1:] for l in d if l.startswith('+') and not l.startswith('+++')]
adk = [l for l in ad if 'regularization_dual' in l and not l.startswith('| none | M8 TR-D rider')]
rmk = [l for l in rm if 'dual_regularization' in l]
if len(rmk) != len(adk): res.append(f'FAIL schema: {len(rmk)} removed vs {len(adk)} added key lines'); fail += 1
for x, y in zip(rmk, adk):
    ok = m(x) == y; res.append(('OK   ' if ok else 'FAIL ') + f'schema: {x[:80]}'); fail += 0 if ok else 1
other = [l for l in ad if l not in adk]
res.append(f'schema added lines that are not key renames (the freeze-table row): {len(other)}')
res.append(f'golden runs changed: {changed}; schema pairs: {len(adk)}; failures: {fail}')
open(OUT, 'w').write('\n'.join(res) + '\n'); print('\n'.join(res[-3:])); sys.exit(1 if fail else 0)
