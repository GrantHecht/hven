#!/usr/bin/env python3
"""AP-2 B8 riders: rename each USE of a renamed member at the exact location the compiler reports it.
Reads a build log; for every `F:L:C: error: no member named 'OLD' in 'T'` (or a field designator /
member-initializer error) whose T's last component and OLD are a row of member_manifest.tsv, replaces
OLD at F:L:C with NEW (the text there must be OLD, else the site is reported and left).
usage: fix_from_errors.py <member_manifest.tsv> <build.log>"""
import re, sys, collections
man, log = sys.argv[1], sys.argv[2]
rows = collections.defaultdict(dict)
for l in open(man):
    if l.strip() and not l.startswith('#'):
        h, st, o, n = l.rstrip('\n').split('\t')
        rows[o][st] = n
PAT = [re.compile(r"^(\S+?):(\d+):(\d+): error: no member named '(\w+)' in '([^']+)'"),
       re.compile(r"^(\S+?):(\d+):(\d+): error: field designator '(\w+)' does not refer to any field in type '([^']+)'"),
       re.compile(r"^(\S+?):(\d+):(\d+): error: no viable overloaded '=' .*'(\w+)'.*'([^']+)'")]
fixes = collections.defaultdict(set)
unmatched = set()
for line in open(log, errors='replace'):
    for rx in PAT:
        m = rx.match(line)
        if not m:
            continue
        f, ln, col, old, ty = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4), m.group(5)
        ty = re.sub(r'^(const |volatile )+', '', ty).split('<')[0].strip()
        last = ty.split('::')[-1]
        new = rows.get(old, {}).get(last) or rows.get(old, {}).get('*')
        if new:
            fixes[f].add((ln, col, old, new))
        else:
            unmatched.add(line.strip()[:200])
        break
applied = 0
for f, sites in fixes.items():
    lines = open(f).read().split('\n')
    for ln, col, old, new in sorted(sites, key=lambda t: (t[0], -t[1])):
        s = lines[ln - 1]
        if s[col - 1:col - 1 + len(old)] == old:
            lines[ln - 1] = s[:col - 1] + new + s[col - 1 + len(old):]
            applied += 1
        else:
            print(f'NOT-AT-SITE {f}:{ln}:{col} {old}')
    open(f, 'w').write('\n'.join(lines))
print(f'applied {applied} in {len(fixes)} files; unmatched member errors {len(unmatched)}')
for u in sorted(unmatched)[:40]:
    print('  UNMATCHED', u)
