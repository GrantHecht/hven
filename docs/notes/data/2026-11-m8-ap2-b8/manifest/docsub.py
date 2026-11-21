#!/usr/bin/env python3
"""Whole-file substitution for Markdown docs. usage: docsub.py <map.tsv>... -- <file>..."""
import re, sys
i = sys.argv.index('--')
maps, files = sys.argv[1:i], sys.argv[i + 1:]
subs = []
for mf in maps:
    for line in open(mf):
        if not line.strip() or line.startswith('#'):
            continue
        o, n = line.rstrip('\n').split('\t')[:2]
        subs.append((re.compile(o[3:]) if o.startswith('re:') else re.compile(r'(?<![\w])' + re.escape(o) + r'(?![\w])'), n))
for p in files:
    s = open(p).read()
    t = s
    for rx, n in subs:
        t = rx.sub(n, t)
    if t != s:
        open(p, 'w').write(t)
        print(p)
