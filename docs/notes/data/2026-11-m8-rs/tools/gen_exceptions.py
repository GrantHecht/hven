#!/usr/bin/env python3
"""Write a P-SYM exceptions file naming ONLY the findings of the given objects (the test objects the design mandates editing).
usage: gen_exceptions.py <psym-output.txt> <exceptions-out> <reason> <object-relpath> [<object-relpath>...]
Fails if the output carries a finding (DIFFERS / ONLY-AFTER / ONLY-BEFORE / SYMBOLS / MISSING) in any OTHER object."""
import re, sys
src, out, reason, objs = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
txt = open(src).read().splitlines()
entries, cur, bad = [], None, []
for l in txt:
    m = re.match(r'^(SYMBOLS|PER-SYMBOL)\s+(\S+?)(:|\s)', l)
    if m:
        cur = m.group(2)
        continue
    m = re.match(r'^(DIFFERS|MISSING|ONLY-BEFORE|ONLY-AFTER|COLLISION|MAP-ERROR)\s+(\S+)', l)
    if m and m.group(1) in ('MISSING', 'COLLISION', 'MAP-ERROR'):
        bad.append(l)
        continue
    if re.match(r'^\S', l) and not l.startswith(('SYMBOLS', 'PER-SYMBOL')):
        cur = None
    m = re.match(r'^\s{10,}(ONLY-AFTER|ONLY-BEFORE|LOCAL-FAMILY|DIFFERS)\s+(.*)$', l)
    if m and cur:
        k, name = m.groups()
        if k == 'LOCAL-FAMILY': name = name.split(':')[0]
        if cur in objs:
            if (k, name) not in entries: entries.append((k, name))
        else:
            bad.append('%s in %s: %s' % (k, cur, name))
if bad:
    print('FINDINGS OUTSIDE THE NAMED OBJECTS:'); [print(' ', b) for b in bad]; sys.exit(1)
with open(out, 'w') as f:
    f.write('# P-SYM exceptions: the findings of the deliberately edited test object(s) only (see DECLARATION-r2.md G2\')\n')
    for o in objs:
        for k, name in entries:
            f.write('%s::%s [%s] ## %s\n' % (o, name, k, reason))
print('%d entries for %s' % (len(entries), objs))
