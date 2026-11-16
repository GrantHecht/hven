#!/usr/bin/env python3
# M8 AP-5 G6 (../DECLARATION.md, "Trace goldens"): proves every re-derived golden line differs from its pre-G6 line by the
# declared key rename ALONE. Every removed/added diff line (git diff -U0 A B) that carries the JSON key
# "last_kkt_info" (old) or "kkt_factor_status" (new) is paired in order; applying the declared map to the removed line must give
# the added line byte for byte. A diff line carrying either key that cannot be paired, or whose pair differs after the map, fails.
# usage: goldens_rename_check.py <rev A> <rev B> <out.txt>
import re, subprocess, sys

A, B, OUT = sys.argv[1:4]
MAP = [('last_kkt_info', 'kkt_factor_status')]  # the declared trace-key map (ipm.phase.exit)
# a golden is a quoted JSON key (\"key\" or "key") or the schema table's key cell; code and prose occurrences are proven by the build
KEY = re.compile(r'(\\?"(last_kkt_info|kkt_factor_status)\\?"|\| `(last_kkt_info|kkt_factor_status)`)')
# test goldens and the schema document; src/ and include/ are code, proven by the build and the suites
paths = ['tests', 'docs/trace-schema-v0.md']
d = subprocess.run(['git', 'diff', '-U0', A, B, '--'] + paths, capture_output=True, text=True, check=True).stdout
cur = None
old_lines, new_lines = [], []
for line in d.split('\n'):
    if line.startswith('+++ ') or line.startswith('--- ') or line.startswith('diff ') or line.startswith('index '):
        if line.startswith('+++ '):
            cur = line[6:]
        continue
    if line.startswith('@@'):
        continue
    if line.startswith('-') and KEY.search(line):
        old_lines.append((cur, line[1:]))
    elif line.startswith('+') and KEY.search(line):
        new_lines.append((cur, line[1:]))
fail = 0
out = []
def mapped(s):
    for a, b in MAP:
        s = s.replace(a, b)
    return s
# A golden that needed no key edit (the line already carried the new spelling on both sides) is not in the diff at all;
# every changed line must pair.
if len(old_lines) != len(new_lines):
    out.append(f'FAIL: {len(old_lines)} removed key lines vs {len(new_lines)} added')
    fail = 1
for (fo, lo), (fn, ln) in zip(old_lines, new_lines):
    ok = mapped(lo) == ln and fo == fn
    out.append(('OK   ' if ok else 'FAIL ') + f'{fo}: {lo.strip()[:110]}')
    if not ok:
        out.append(f'     map(old) != new: {mapped(lo).strip()[:110]} vs {ln.strip()[:110]}')
        fail = 1
out.append(f'pairs: {len(new_lines)}  failures: {fail}')
open(OUT, 'w').write('\n'.join(out) + '\n')
print('\n'.join(out[-3:]))
sys.exit(fail)
